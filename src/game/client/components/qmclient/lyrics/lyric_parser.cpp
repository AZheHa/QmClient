// C++ rewrite of lyric parsing/decryption ideas from Lyricify-Lyrics-Helper
// (Apache-2.0): QrcParser.cs, YrcParser.cs, Qrc/Decrypter.cs and
// Netease/EapiHelper.cs. This file does not copy C# implementation code.

#include "lyric_parser.h"

#include <base/hash.h>
#include <base/str.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <utility>

#include <zlib.h>

#if defined(CONF_OPENSSL)
#include <openssl/evp.h>
#include <openssl/des.h>
#endif

namespace QmLyrics
{
namespace
{

static void SetError(char *pErr, size_t ErrSize, const char *pText)
{
	if(pErr != nullptr && ErrSize > 0)
		str_copy(pErr, pText, ErrSize);
}

static void TrimString(std::string &Str)
{
	while(!Str.empty() && isspace((unsigned char)Str.back()))
		Str.pop_back();
	size_t Start = 0;
	while(Start < Str.size() && isspace((unsigned char)Str[Start]))
		Start++;
	if(Start > 0)
		Str.erase(0, Start);
}

static bool ParseInt64(const char *pStart, const char *pEnd, int64_t &Out)
{
	if(pStart >= pEnd || !isdigit((unsigned char)*pStart))
		return false;
	int64_t Value = 0;
	for(const char *p = pStart; p < pEnd; ++p)
	{
		if(!isdigit((unsigned char)*p))
			return false;
		Value = Value * 10 + (*p - '0');
	}
	Out = Value;
	return true;
}

static bool ParseTimestampMs(const char *pStart, const char *pEnd, int64_t &OutMs)
{
	if(pStart >= pEnd || !isdigit((unsigned char)*pStart))
		return false;
	int Minutes = 0;
	const char *p = pStart;
	while(p < pEnd && isdigit((unsigned char)*p))
	{
		Minutes = Minutes * 10 + (*p - '0');
		p++;
	}
	if(p >= pEnd || *p != ':')
		return false;
	p++;
	if(p >= pEnd || !isdigit((unsigned char)*p))
		return false;
	int Seconds = 0;
	while(p < pEnd && isdigit((unsigned char)*p))
	{
		Seconds = Seconds * 10 + (*p - '0');
		p++;
	}
	int Milli = 0;
	if(p < pEnd && (*p == '.' || *p == ','))
	{
		p++;
		int Fraction = 0;
		int Digits = 0;
		while(p < pEnd && isdigit((unsigned char)*p) && Digits < 3)
		{
			Fraction = Fraction * 10 + (*p - '0');
			p++;
			Digits++;
		}
		if(Digits == 1)
			Milli = Fraction * 100;
		else if(Digits == 2)
			Milli = Fraction * 10;
		else
			Milli = Fraction;
	}
	OutMs = (int64_t)Minutes * 60000 + (int64_t)Seconds * 1000 + Milli;
	return true;
}

static bool ParseBracketMsDuration(const std::string &Line, size_t &Pos, int64_t &StartMs, int64_t &DurationMs)
{
	if(Pos >= Line.size() || Line[Pos] != '[')
		return false;
	const size_t Comma = Line.find(',', Pos + 1);
	const size_t Close = Line.find(']', Pos + 1);
	if(Comma == std::string::npos || Close == std::string::npos || Comma > Close)
		return false;
	if(!ParseInt64(Line.c_str() + Pos + 1, Line.c_str() + Comma, StartMs))
		return false;
	if(!ParseInt64(Line.c_str() + Comma + 1, Line.c_str() + Close, DurationMs))
		return false;
	Pos = Close + 1;
	return true;
}

static bool ParseParenMsDuration(const std::string &Line, size_t &Pos, int64_t &StartMs, int64_t &DurationMs)
{
	if(Pos >= Line.size() || Line[Pos] != '(')
		return false;
	const size_t Comma = Line.find(',', Pos + 1);
	if(Comma == std::string::npos)
		return false;
	const size_t Comma2 = Line.find(',', Comma + 1);
	const size_t Close = Line.find(')', Comma + 1);
	if(Close == std::string::npos)
		return false;
	const size_t DurationEnd = Comma2 != std::string::npos && Comma2 < Close ? Comma2 : Close;
	if(!ParseInt64(Line.c_str() + Pos + 1, Line.c_str() + Comma, StartMs))
		return false;
	if(!ParseInt64(Line.c_str() + Comma + 1, Line.c_str() + DurationEnd, DurationMs))
		return false;
	Pos = Close + 1;
	return true;
}

static std::string JoinSyllables(const std::vector<CSyllable> &vSyllables)
{
	std::string Text;
	for(const CSyllable &Syllable : vSyllables)
		Text.append(Syllable.m_Text);
	TrimString(Text);
	return Text;
}

static bool ParseSyllableLine(const std::string &Line, bool HasLineDuration, CLyricLine &OutLine)
{
	size_t Pos = 0;
	int64_t LineStart = 0;
	int64_t LineDuration = 0;
	if(HasLineDuration)
	{
		if(!ParseBracketMsDuration(Line, Pos, LineStart, LineDuration))
			return false;
	}
	else
	{
		const size_t Close = Line.find(']');
		if(Close != std::string::npos)
			Pos = Close + 1;
	}

	std::vector<CSyllable> vSyllables;
	while(Pos < Line.size())
	{
		const size_t Open = Line.find('(', Pos);
		if(Open == std::string::npos)
			break;
		std::string Text = Line.substr(Pos, Open - Pos);
		int64_t StartMs = 0;
		int64_t DurationMs = 0;
		size_t SyllablePos = Open;
		if(!ParseParenMsDuration(Line, SyllablePos, StartMs, DurationMs))
		{
			Pos = Open + 1;
			continue;
		}
		CSyllable Syllable;
		Syllable.m_StartMs = StartMs;
		Syllable.m_DurationMs = DurationMs;
		Syllable.m_Text = std::move(Text);
		vSyllables.push_back(std::move(Syllable));
		Pos = SyllablePos;
	}

	if(vSyllables.empty())
		return false;
	if(!HasLineDuration)
	{
		LineStart = vSyllables.front().m_StartMs;
		const CSyllable &Last = vSyllables.back();
		LineDuration = std::max<int64_t>(0, Last.m_StartMs + Last.m_DurationMs - LineStart);
	}

	OutLine.m_TimeMs = LineStart;
	OutLine.m_DurationMs = LineDuration;
	OutLine.m_vSyllables = std::move(vSyllables);
	OutLine.m_Text = JoinSyllables(OutLine.m_vSyllables);
	return !OutLine.m_Text.empty();
}

static bool ParseYrcSyllableLine(const std::string &Line, CLyricLine &OutLine)
{
	size_t Pos = 0;
	int64_t LineStart = 0;
	int64_t LineDuration = 0;
	if(!ParseBracketMsDuration(Line, Pos, LineStart, LineDuration))
		return false;

	std::vector<CSyllable> vSyllables;
	while(Pos < Line.size())
	{
		if(Line[Pos] != '(')
		{
			++Pos;
			continue;
		}
		int64_t StartMs = 0;
		int64_t DurationMs = 0;
		if(!ParseParenMsDuration(Line, Pos, StartMs, DurationMs))
			return false;

		const size_t TextStart = Pos;
		const size_t NextOpen = Line.find('(', Pos);
		const size_t TextEnd = NextOpen == std::string::npos ? Line.size() : NextOpen;
		std::string TextPart = Line.substr(TextStart, TextEnd - TextStart);
		if(!TextPart.empty())
		{
			CSyllable Syllable;
			Syllable.m_StartMs = StartMs;
			Syllable.m_DurationMs = DurationMs;
			Syllable.m_Text = std::move(TextPart);
			vSyllables.push_back(std::move(Syllable));
		}
		Pos = TextEnd;
	}

	if(vSyllables.empty())
		return false;
	OutLine.m_TimeMs = LineStart;
	OutLine.m_DurationMs = LineDuration;
	OutLine.m_vSyllables = std::move(vSyllables);
	OutLine.m_Text = JoinSyllables(OutLine.m_vSyllables);
	return !OutLine.m_Text.empty();
}

static std::string ExtractJsonCreditsText(const std::string &Line)
{
	std::string Text;
	size_t Pos = 0;
	while(true)
	{
		const size_t Key = Line.find("\"tx\"", Pos);
		if(Key == std::string::npos)
			break;
		const size_t Colon = Line.find(':', Key + 4);
		if(Colon == std::string::npos)
			break;
		const size_t Quote = Line.find('"', Colon + 1);
		if(Quote == std::string::npos)
			break;
		std::string Part;
		for(size_t i = Quote + 1; i < Line.size(); ++i)
		{
			if(Line[i] == '"' && (i == 0 || Line[i - 1] != '\\'))
			{
				Pos = i + 1;
				break;
			}
			if(Line[i] == '\\' && i + 1 < Line.size())
			{
				++i;
				Part.push_back(Line[i]);
			}
			else
			{
				Part.push_back(Line[i]);
			}
			if(i + 1 >= Line.size())
				Pos = Line.size();
		}
		Text.append(Part);
		if(Pos >= Line.size())
			break;
	}
	return Text;
}

static bool ExtractJsonCreditTime(const std::string &Line, int64_t &OutMs)
{
	const size_t Key = Line.find("\"t\"");
	if(Key == std::string::npos)
		return false;
	const size_t Colon = Line.find(':', Key + 3);
	if(Colon == std::string::npos)
		return false;
	size_t End = Colon + 1;
	while(End < Line.size() && isspace((unsigned char)Line[End]))
		End++;
	const size_t Start = End;
	while(End < Line.size() && isdigit((unsigned char)Line[End]))
		End++;
	return ParseInt64(Line.c_str() + Start, Line.c_str() + End, OutMs);
}

static bool ParseBase64(const std::string &Text, std::vector<unsigned char> &vOut)
{
	std::string Clean;
	Clean.reserve(Text.size());
	for(char c : Text)
	{
		if(!isspace((unsigned char)c))
			Clean.push_back(c);
	}
	if(Clean.empty())
		return false;
	const int MaxDecodedSize = static_cast<int>((Clean.size() * 3) / 4 + 4);
	vOut.resize(MaxDecodedSize);
	const int DecodedSize = str_base64_decode(vOut.data(), MaxDecodedSize, Clean.c_str());
	if(DecodedSize <= 0)
		return false;
	vOut.resize(DecodedSize);
	return true;
}

static bool LooksLikeSyncedLyricText(const std::string &Text)
{
	return Text.find('[') != std::string::npos || Text.find('(') != std::string::npos;
}

static bool ParseHex(const std::string &Text, std::vector<unsigned char> &vOut)
{
	std::string Clean;
	Clean.reserve(Text.size());
	for(char c : Text)
	{
		if(!isspace((unsigned char)c))
			Clean.push_back(c);
	}
	if(Clean.empty() || (Clean.size() % 2) != 0)
		return false;
	vOut.clear();
	vOut.reserve(Clean.size() / 2);
	for(size_t i = 0; i < Clean.size(); i += 2)
	{
		unsigned int Value = 0;
		if(std::sscanf(Clean.c_str() + i, "%2x", &Value) != 1)
			return false;
		vOut.push_back((unsigned char)Value);
	}
	return true;
}

static bool InflateBytes(const std::vector<unsigned char> &vInput, std::string &Out)
{
	if(vInput.empty())
		return false;
	z_stream Stream{};
	Stream.next_in = const_cast<Bytef *>(reinterpret_cast<const Bytef *>(vInput.data()));
	Stream.avail_in = (uInt)vInput.size();
	if(inflateInit(&Stream) != Z_OK)
		return false;
	std::vector<unsigned char> vOutput;
	unsigned char aBuffer[4096];
	int Result = Z_OK;
	while(Result == Z_OK)
	{
		Stream.next_out = aBuffer;
		Stream.avail_out = sizeof(aBuffer);
		Result = inflate(&Stream, Z_NO_FLUSH);
		if(Result != Z_OK && Result != Z_STREAM_END)
		{
			inflateEnd(&Stream);
			return false;
		}
		const size_t Written = sizeof(aBuffer) - Stream.avail_out;
		vOutput.insert(vOutput.end(), aBuffer, aBuffer + Written);
	}
	inflateEnd(&Stream);
	if(vOutput.size() >= 3 && vOutput[0] == 0xEF && vOutput[1] == 0xBB && vOutput[2] == 0xBF)
		Out.assign(reinterpret_cast<const char *>(vOutput.data() + 3), vOutput.size() - 3);
	else
		Out.assign(reinterpret_cast<const char *>(vOutput.data()), vOutput.size());
	return true;
}

#if defined(CONF_OPENSSL)
static bool TripleDesEcbDecrypt(const std::vector<unsigned char> &vInput, std::vector<unsigned char> &vOut)
{
	static const unsigned char s_aQqKey[24] = {
		'!', '@', '#', ')', '(', '*', '$', '%',
		'1', '2', '3', 'Z', 'X', 'C', '!', '@',
		'!', '@', '#', ')', '(', 'N', 'H', 'L'};
	if(vInput.empty() || (vInput.size() % 8) != 0)
		return false;
	vOut.resize(vInput.size() + 8);
	std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> pCtx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
	if(!pCtx)
		return false;
	if(EVP_DecryptInit_ex(pCtx.get(), EVP_des_ede3_ecb(), nullptr, s_aQqKey, nullptr) != 1)
		return false;
	EVP_CIPHER_CTX_set_padding(pCtx.get(), 0);
	int OutLen1 = 0;
	if(EVP_DecryptUpdate(pCtx.get(), vOut.data(), &OutLen1, vInput.data(), (int)vInput.size()) != 1)
		return false;
	int OutLen2 = 0;
	if(EVP_DecryptFinal_ex(pCtx.get(), vOut.data() + OutLen1, &OutLen2) != 1)
		return false;
	vOut.resize(OutLen1 + OutLen2);
	return true;
}

static bool Aes128EcbEncrypt(const std::string &Data, std::vector<unsigned char> &vOut)
{
	static const unsigned char s_aKey[16] = {
		'e', '8', '2', 'c', 'k', 'e', 'n', 'h',
		'8', 'd', 'i', 'c', 'h', 'e', 'n', '8'};
	std::unique_ptr<EVP_CIPHER_CTX, decltype(&EVP_CIPHER_CTX_free)> pCtx(EVP_CIPHER_CTX_new(), EVP_CIPHER_CTX_free);
	if(!pCtx)
		return false;
	if(EVP_EncryptInit_ex(pCtx.get(), EVP_aes_128_ecb(), nullptr, s_aKey, nullptr) != 1)
		return false;
	vOut.resize(Data.size() + EVP_CIPHER_block_size(EVP_aes_128_ecb()));
	int OutLen1 = 0;
	if(EVP_EncryptUpdate(pCtx.get(), vOut.data(), &OutLen1, reinterpret_cast<const unsigned char *>(Data.data()), (int)Data.size()) != 1)
		return false;
	int OutLen2 = 0;
	if(EVP_EncryptFinal_ex(pCtx.get(), vOut.data() + OutLen1, &OutLen2) != 1)
		return false;
	vOut.resize(OutLen1 + OutLen2);
	return true;
}
#endif

static std::string BytesToHexUpper(const std::vector<unsigned char> &vData)
{
	static const char s_aHex[] = "0123456789ABCDEF";
	std::string Result;
	Result.resize(vData.size() * 2);
	for(size_t i = 0; i < vData.size(); ++i)
	{
		Result[i * 2] = s_aHex[vData[i] >> 4];
		Result[i * 2 + 1] = s_aHex[vData[i] & 0x0F];
	}
	return Result;
}

static std::string ToEapiPath(const char *pUrl)
{
	std::string Url = pUrl ? pUrl : "";
	const char *apPrefixes[] = {
		"https://interface3.music.163.com/e",
		"https://interface.music.163.com/e",
		"http://interface3.music.163.com/e",
		"http://interface.music.163.com/e",
	};
	for(const char *pPrefix : apPrefixes)
	{
		if(Url.rfind(pPrefix, 0) == 0)
			return "/" + Url.substr(str_length(pPrefix));
	}
	const size_t Scheme = Url.find("://");
	if(Scheme != std::string::npos)
	{
		const size_t PathStart = Url.find('/', Scheme + 3);
		if(PathStart != std::string::npos)
			return Url.substr(PathStart);
	}
	return Url;
}

} // namespace

void SortAndFillDurations(std::vector<CLyricLine> &vLines)
{
	std::sort(vLines.begin(), vLines.end(), [](const CLyricLine &A, const CLyricLine &B) {
		if(A.m_TimeMs != B.m_TimeMs)
			return A.m_TimeMs < B.m_TimeMs;
		return A.m_Text < B.m_Text;
	});
	for(size_t i = 0; i < vLines.size(); ++i)
	{
		if(vLines[i].m_DurationMs <= 0 && i + 1 < vLines.size())
			vLines[i].m_DurationMs = std::max<int64_t>(0, vLines[i + 1].m_TimeMs - vLines[i].m_TimeMs);
	}
}

bool BuildVisibleLineText(const CLyricLine &Line, int64_t PositionMs, char *pBuf, size_t BufSize)
{
	if(pBuf == nullptr || BufSize == 0)
		return false;

	pBuf[0] = '\0';
	if(Line.m_Text.empty())
		return false;

	if(Line.m_vSyllables.empty())
	{
		str_copy(pBuf, Line.m_Text.c_str(), BufSize);
		return pBuf[0] != '\0';
	}

	for(const CSyllable &Syllable : Line.m_vSyllables)
	{
		if(Syllable.m_Text.empty())
			continue;

		const int64_t StartMs = Syllable.m_StartMs;
		const int64_t EndMs = Syllable.m_DurationMs > 0 ? StartMs + Syllable.m_DurationMs : StartMs;
		if(PositionMs < StartMs)
			break;

		if(Syllable.m_DurationMs <= 0 || PositionMs >= EndMs)
		{
			str_append(pBuf, Syllable.m_Text.c_str(), BufSize);
			continue;
		}

		size_t SyllableSize = 0;
		size_t SyllableCount = 0;
		str_utf8_stats(Syllable.m_Text.c_str(), Syllable.m_Text.size() + 1, Syllable.m_Text.size() + 1, &SyllableSize, &SyllableCount);
		(void)SyllableSize;
		if(SyllableCount == 0)
			break;

		const int64_t ElapsedMs = std::max<int64_t>(0, PositionMs - StartMs);
		size_t RevealCount = (size_t)((ElapsedMs * (int64_t)SyllableCount + Syllable.m_DurationMs - 1) / Syllable.m_DurationMs);
		RevealCount = std::clamp<size_t>(RevealCount, 1, SyllableCount);

		char aPartial[256];
		str_utf8_truncate(aPartial, sizeof(aPartial), Syllable.m_Text.c_str(), (int)RevealCount);
		str_append(pBuf, aPartial, BufSize);
		break;
	}

	str_utf8_fix_truncation(pBuf);
	return pBuf[0] != '\0';
}

bool ParseLrcLyrics(const std::string &Text, std::vector<CLyricLine> &OutLines, char *pErr, size_t ErrSize)
{
	OutLines.clear();
	const char *p = Text.c_str();
	const char *pEnd = p + Text.size();
	while(p < pEnd)
	{
		const char *pLineEnd = (const char *)memchr(p, '\n', pEnd - p);
		if(!pLineEnd)
			pLineEnd = pEnd;

		std::string Line(p, pLineEnd);
		if(!Line.empty() && Line.back() == '\r')
			Line.pop_back();

		std::vector<int64_t> vTimes;
		size_t Pos = 0;
		while(Pos < Line.size() && Line[Pos] == '[')
		{
			const size_t Close = Line.find(']', Pos);
			if(Close == std::string::npos)
				break;
			int64_t TimeMs = 0;
			if(ParseTimestampMs(Line.c_str() + Pos + 1, Line.c_str() + Close, TimeMs))
				vTimes.push_back(TimeMs);
			Pos = Close + 1;
		}

		std::string TextPart = Line.substr(Pos);
		TrimString(TextPart);
		if(!vTimes.empty() && !TextPart.empty())
		{
			for(int64_t TimeMs : vTimes)
			{
				CLyricLine LineEntry;
				LineEntry.m_TimeMs = TimeMs;
				LineEntry.m_Text = TextPart;
				OutLines.push_back(std::move(LineEntry));
			}
		}

		p = pLineEnd + (pLineEnd < pEnd ? 1 : 0);
	}

	if(OutLines.empty())
	{
		SetError(pErr, ErrSize, "No synced lyrics");
		return false;
	}
	SortAndFillDurations(OutLines);
	return true;
}

bool ParseQrcLyrics(const std::string &Text, std::vector<CLyricLine> &OutLines, char *pErr, size_t ErrSize)
{
	OutLines.clear();
	const char *p = Text.c_str();
	const char *pEnd = p + Text.size();
	while(p < pEnd)
	{
		const char *pLineEnd = (const char *)memchr(p, '\n', pEnd - p);
		if(!pLineEnd)
			pLineEnd = pEnd;
		std::string Line(p, pLineEnd);
		if(!Line.empty() && Line.back() == '\r')
			Line.pop_back();
		TrimString(Line);
		CLyricLine Parsed;
		if(ParseSyllableLine(Line, true, Parsed))
			OutLines.push_back(std::move(Parsed));
		p = pLineEnd + (pLineEnd < pEnd ? 1 : 0);
	}
	if(OutLines.empty())
	{
		SetError(pErr, ErrSize, "No QRC lyrics");
		return false;
	}
	SortAndFillDurations(OutLines);
	return true;
}

bool ParseYrcLyrics(const std::string &Text, std::vector<CLyricLine> &OutLines, char *pErr, size_t ErrSize)
{
	OutLines.clear();
	const char *p = Text.c_str();
	const char *pEnd = p + Text.size();
	while(p < pEnd)
	{
		const char *pLineEnd = (const char *)memchr(p, '\n', pEnd - p);
		if(!pLineEnd)
			pLineEnd = pEnd;
		std::string Line(p, pLineEnd);
		if(!Line.empty() && Line.back() == '\r')
			Line.pop_back();
		TrimString(Line);
		CLyricLine Parsed;
		if(!Line.empty() && Line[0] == '{')
		{
			int64_t TimeMs = 0;
			std::string TextPart = ExtractJsonCreditsText(Line);
			TrimString(TextPart);
			if(!TextPart.empty() && ExtractJsonCreditTime(Line, TimeMs))
			{
				Parsed.m_TimeMs = TimeMs;
				Parsed.m_Text = TextPart;
				OutLines.push_back(std::move(Parsed));
			}
		}
		else if(ParseYrcSyllableLine(Line, Parsed))
		{
			OutLines.push_back(std::move(Parsed));
		}
		p = pLineEnd + (pLineEnd < pEnd ? 1 : 0);
	}
	if(OutLines.empty())
	{
		SetError(pErr, ErrSize, "No YRC lyrics");
		return false;
	}
	SortAndFillDurations(OutLines);
	return true;
}

bool MergeLineTextByTimestamp(std::vector<CLyricLine> &vLines, const std::string &TimedText, bool Translation, char *pErr, size_t ErrSize)
{
	std::vector<CLyricLine> vTextLines;
	if(!ParseLrcLyrics(TimedText, vTextLines, pErr, ErrSize))
		return false;
	for(CLyricLine &Target : vLines)
	{
		auto Best = std::min_element(vTextLines.begin(), vTextLines.end(), [&](const CLyricLine &A, const CLyricLine &B) {
			return std::llabs(A.m_TimeMs - Target.m_TimeMs) < std::llabs(B.m_TimeMs - Target.m_TimeMs);
		});
		if(Best != vTextLines.end() && std::llabs(Best->m_TimeMs - Target.m_TimeMs) <= 800)
		{
			if(Translation)
				Target.m_Translation = Best->m_Text;
			else
				Target.m_Romanized = Best->m_Text;
		}
	}
	return true;
}

bool DecryptQqQrcPayload(const std::string &Payload, std::string &OutText, char *pErr, size_t ErrSize)
{
	OutText.clear();
	std::string Trimmed = Payload;
	TrimString(Trimmed);
	if(Trimmed.empty())
	{
		SetError(pErr, ErrSize, "Empty QRC payload");
		return false;
	}
	if(LooksLikeSyncedLyricText(Trimmed))
	{
		OutText = Trimmed;
		return true;
	}

	std::vector<unsigned char> vBytes;
	std::string Inflated;
	if(ParseBase64(Trimmed, vBytes) && InflateBytes(vBytes, Inflated))
	{
		OutText = std::move(Inflated);
		return true;
	}
	if(!vBytes.empty())
	{
		std::string Decoded(reinterpret_cast<const char *>(vBytes.data()), vBytes.size());
		if(Decoded.size() >= 3 && (unsigned char)Decoded[0] == 0xEF && (unsigned char)Decoded[1] == 0xBB && (unsigned char)Decoded[2] == 0xBF)
			Decoded.erase(0, 3);
		if(LooksLikeSyncedLyricText(Decoded))
		{
			OutText = std::move(Decoded);
			return true;
		}
	}
	if(ParseHex(Trimmed, vBytes))
	{
#if defined(CONF_OPENSSL)
		std::vector<unsigned char> vDecrypted;
		if(TripleDesEcbDecrypt(vBytes, vDecrypted) && InflateBytes(vDecrypted, Inflated))
		{
			OutText = std::move(Inflated);
			return true;
		}
#else
		SetError(pErr, ErrSize, "OpenSSL unavailable for QRC");
		return false;
#endif
	}
	SetError(pErr, ErrSize, "Unsupported QRC payload");
	return false;
}

std::string BuildNeteaseEapiBody(const char *pUrl, const std::string &JsonPayload, char *pErr, size_t ErrSize)
{
#if defined(CONF_OPENSSL)
	const std::string Path = ToEapiPath(pUrl);
	const std::string Message = "nobody" + Path + "use" + JsonPayload + "md5forencrypt";
	char aMd5[MD5_MAXSTRSIZE];
	md5_str(md5(Message.data(), Message.size()), aMd5, sizeof(aMd5));
	const std::string Data = Path + "-36cd479b6b5-" + JsonPayload + "-36cd479b6b5-" + aMd5;
	std::vector<unsigned char> vEncrypted;
	if(!Aes128EcbEncrypt(Data, vEncrypted))
	{
		SetError(pErr, ErrSize, "EAPI AES failed");
		return {};
	}
	std::string Body = "params=";
	Body += BytesToHexUpper(vEncrypted);
	return Body;
#else
	SetError(pErr, ErrSize, "OpenSSL unavailable for EAPI");
	return {};
#endif
}

} // namespace QmLyrics

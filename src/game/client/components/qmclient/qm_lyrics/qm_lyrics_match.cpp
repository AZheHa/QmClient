#include "qm_lyrics_match.h"

#include <base/system.h>

#include <algorithm>
#include <cctype>
#include <cstring>
#include <set>
#include <vector>

namespace QmLyrics
{

	namespace
	{

		// 删除括号注释：(feat. xxx) / (cover) / (live) / (ost) / (remix) 等。
		// 也处理 [live] / [remaster*] / "- remaster*" 后缀。
		void StripModifiers(std::string &Text)
		{
			// 删 (xxx) 块（一对括号内含 feat/cover/live/ost/remix/remaster 之一）
			const char *apTokens[] = {"feat", "cover", "live", "ost", "remix", "remaster", "version", "edit", "acoustic", "instrumental"};
			auto ContainsAnyToken = [&](std::string_view Inside) -> bool {
				// 简化：lowercase 比较（输入此时尚未 lowercase，但 ContainsAnyToken 内自己 lower）
				std::string Lower(Inside);
				for(char &C : Lower)
					C = (char)std::tolower((unsigned char)C);
				for(const char *pTok : apTokens)
				{
					if(Lower.find(pTok) != std::string::npos)
						return true;
				}
				return false;
			};

			auto StripBracketed = [&](char OpenCh, char CloseCh) {
				while(true)
				{
					const size_t Open = Text.find(OpenCh);
					if(Open == std::string::npos)
						break;
					const size_t Close = Text.find(CloseCh, Open + 1);
					if(Close == std::string::npos)
						break;
					std::string_view Inside(Text.data() + Open + 1, Close - Open - 1);
					if(ContainsAnyToken(Inside))
						Text.erase(Open, Close - Open + 1);
					else
						break; // 没含修饰 token，保留以避免误删（如 "(Remix)" 误删但 "(Acoustic Version)" 含 token 也删）
				}
			};
			StripBracketed('(', ')');
			StripBracketed('[', ']');

			// 删 " - remaster*" / " - live" 等后缀分隔的修饰
			const size_t Dash = Text.find(" - ");
			if(Dash != std::string::npos)
			{
				std::string_view Tail(Text.data() + Dash + 3, Text.size() - Dash - 3);
				if(ContainsAnyToken(Tail))
					Text.resize(Dash);
			}
		}

		// ASCII 去重音表（Latin-1 supplement）。U+00C0-U+00FF 部分。
		char SimplifyChar(unsigned int Codepoint)
		{
			if(Codepoint < 0x80)
				return (char)Codepoint;

			// Latin-1 supplement 重音字母 → 基础 ASCII
			static const struct
			{
				unsigned int m_From;
				char m_To;
			} s_aMap[] = {
				{0x00C0, 'a'},
				{0x00C1, 'a'},
				{0x00C2, 'a'},
				{0x00C3, 'a'},
				{0x00C4, 'a'},
				{0x00C5, 'a'},
				{0x00C6, 'a'},
				{0x00C7, 'c'},
				{0x00C8, 'e'},
				{0x00C9, 'e'},
				{0x00CA, 'e'},
				{0x00CB, 'e'},
				{0x00CC, 'i'},
				{0x00CD, 'i'},
				{0x00CE, 'i'},
				{0x00CF, 'i'},
				{0x00D1, 'n'},
				{0x00D2, 'o'},
				{0x00D3, 'o'},
				{0x00D4, 'o'},
				{0x00D5, 'o'},
				{0x00D6, 'o'},
				{0x00D8, 'o'},
				{0x00D9, 'u'},
				{0x00DA, 'u'},
				{0x00DB, 'u'},
				{0x00DC, 'u'},
				{0x00DD, 'y'},
				{0x00E0, 'a'},
				{0x00E1, 'a'},
				{0x00E2, 'a'},
				{0x00E3, 'a'},
				{0x00E4, 'a'},
				{0x00E5, 'a'},
				{0x00E6, 'a'},
				{0x00E7, 'c'},
				{0x00E8, 'e'},
				{0x00E9, 'e'},
				{0x00EA, 'e'},
				{0x00EB, 'e'},
				{0x00EC, 'i'},
				{0x00ED, 'i'},
				{0x00EE, 'i'},
				{0x00EF, 'i'},
				{0x00F1, 'n'},
				{0x00F2, 'o'},
				{0x00F3, 'o'},
				{0x00F4, 'o'},
				{0x00F5, 'o'},
				{0x00F6, 'o'},
				{0x00F8, 'o'},
				{0x00F9, 'u'},
				{0x00FA, 'u'},
				{0x00FB, 'u'},
				{0x00FC, 'u'},
				{0x00FD, 'y'},
				{0x00FF, 'y'},
			};
			for(const auto &Entry : s_aMap)
			{
				if(Entry.m_From == Codepoint)
					return Entry.m_To;
			}
			return 0; // 不在表里：保留原 UTF-8（中日韩等）
		}

		// 判断码点是否为 CJK 表意字符（粗粒度）：保留这些字符不被标点过滤删除。
		bool IsCjkCodepoint(unsigned int Codepoint)
		{
			return (Codepoint >= 0x3040 && Codepoint <= 0x30FF) || // 日文 Hiragana/Katakana
			       (Codepoint >= 0x3400 && Codepoint <= 0x4DBF) || // CJK Ext A
			       (Codepoint >= 0x4E00 && Codepoint <= 0x9FFF) || // CJK Unified
			       (Codepoint >= 0xAC00 && Codepoint <= 0xD7AF); // Hangul
		}

	} // anonymous namespace

	std::string NormalizeForMatch(std::string_view Input)
	{
		if(Input.empty())
			return "";

		std::string Work(Input);
		StripModifiers(Work);

		// UTF-8 遍历，构建归一化输出
		std::string Out;
		Out.reserve(Work.size());
		const char *p = Work.data();
		const char *pEnd = p + Work.size();
		bool LastWasSpace = true; // 折叠开头空格
		while(p < pEnd)
		{
			const char *pNext = p;
			const int Codepoint = str_utf8_decode(&pNext);
			if(Codepoint < 0)
			{
				++p;
				continue;
			}
			if(Codepoint < 0x80)
			{
				const char C = (char)Codepoint;
				if(C >= 'A' && C <= 'Z')
				{
					Out.push_back(C - 'A' + 'a');
					LastWasSpace = false;
				}
				else if((C >= 'a' && C <= 'z') || (C >= '0' && C <= '9'))
				{
					Out.push_back(C);
					LastWasSpace = false;
				}
				else if(C == ' ' || C == '\t' || C == '\n' || C == '\r')
				{
					if(!LastWasSpace)
					{
						Out.push_back(' ');
						LastWasSpace = true;
					}
				}
				// 其它 ASCII 标点：删除（不引入空格，避免 "A.B" 变成 "a b"）
			}
			else
			{
				const char Simplified = SimplifyChar((unsigned int)Codepoint);
				if(Simplified != 0)
				{
					Out.push_back(Simplified);
					LastWasSpace = false;
				}
				else if(IsCjkCodepoint((unsigned int)Codepoint))
				{
					// 保留 UTF-8 原样
					Out.append(p, pNext - p);
					LastWasSpace = false;
				}
				// 其它非 ASCII 字符（如希腊、阿拉伯）：丢弃
			}
			p = pNext;
		}

		while(!Out.empty() && Out.back() == ' ')
			Out.pop_back();
		return Out;
	}

	int LevenshteinDistance(std::string_view A, std::string_view B)
	{
		// 以 UTF-8 码点拆分为序列
		auto ToCodepoints = [](std::string_view S) -> std::vector<int> {
			std::vector<int> v;
			v.reserve(S.size());
			const char *p = S.data();
			const char *pEnd = p + S.size();
			while(p < pEnd)
			{
				const char *pNext = p;
				const int Cp = str_utf8_decode(&pNext);
				if(Cp >= 0)
					v.push_back(Cp);
				if(pNext == p)
					++p;
				else
					p = pNext;
			}
			return v;
		};
		const std::vector<int> vA = ToCodepoints(A);
		const std::vector<int> vB = ToCodepoints(B);
		const size_t N = vA.size();
		const size_t M = vB.size();
		if(N == 0)
			return (int)M;
		if(M == 0)
			return (int)N;

		std::vector<int> vPrev(M + 1);
		std::vector<int> vCurr(M + 1);
		for(size_t j = 0; j <= M; ++j)
			vPrev[j] = (int)j;
		for(size_t i = 1; i <= N; ++i)
		{
			vCurr[0] = (int)i;
			for(size_t j = 1; j <= M; ++j)
			{
				const int Cost = vA[i - 1] == vB[j - 1] ? 0 : 1;
				vCurr[j] = std::min({vPrev[j] + 1, vCurr[j - 1] + 1, vPrev[j - 1] + Cost});
			}
			std::swap(vPrev, vCurr);
		}
		return vPrev[M];
	}

	float TokenSetRatio(std::string_view A, std::string_view B)
	{
		auto Tokenize = [](std::string_view S) -> std::set<std::string> {
			std::set<std::string> Out;
			const std::string Normalized = NormalizeForMatch(S);
			const char *p = Normalized.data();
			const char *pEnd = p + Normalized.size();
			while(p < pEnd)
			{
				while(p < pEnd && *p == ' ')
					++p;
				const char *pTokStart = p;
				while(p < pEnd && *p != ' ')
					++p;
				if(p > pTokStart)
					Out.emplace(pTokStart, p - pTokStart);
			}
			return Out;
		};
		const std::set<std::string> SetA = Tokenize(A);
		const std::set<std::string> SetB = Tokenize(B);
		if(SetA.empty() && SetB.empty())
			return 1.0f;
		if(SetA.empty() || SetB.empty())
			return 0.0f;
		std::vector<std::string> vIntersect;
		std::set_intersection(SetA.begin(), SetA.end(), SetB.begin(), SetB.end(), std::back_inserter(vIntersect));
		return (2.0f * (float)vIntersect.size()) / (float)(SetA.size() + SetB.size());
	}

	float DurationScore(int QuerySec, int CandidateSec, int TolMs)
	{
		if(QuerySec <= 0 || CandidateSec <= 0)
			return 0.5f;
		const int DiffMs = std::abs(QuerySec - CandidateSec) * 1000;
		if(TolMs <= 0)
			return DiffMs == 0 ? 1.0f : 0.0f;
		const float Norm = (float)DiffMs / (float)TolMs;
		if(Norm >= 1.0f)
			return 0.0f;
		return 1.0f - Norm;
	}

	namespace
	{

		// 把 Levenshtein 距离转 0..1 相似度（基于 max 长度）。
		float LevenshteinSimilarity(std::string_view A, std::string_view B)
		{
			const std::string NormA = NormalizeForMatch(A);
			const std::string NormB = NormalizeForMatch(B);
			if(NormA.empty() && NormB.empty())
				return 1.0f;
			if(NormA.empty() || NormB.empty())
				return 0.0f;
			const int Distance = LevenshteinDistance(NormA, NormB);
			// 以 UTF-8 码点数为长度
			auto CountCp = [](std::string_view S) -> int {
				int N = 0;
				const char *p = S.data();
				const char *pEnd = p + S.size();
				while(p < pEnd)
				{
					const char *pNext = p;
					if(str_utf8_decode(&pNext) >= 0 && pNext > p)
						p = pNext;
					else
						++p;
					++N;
				}
				return N;
			};
			const int MaxLen = std::max(CountCp(NormA), CountCp(NormB));
			if(MaxLen == 0)
				return 1.0f;
			return 1.0f - (float)Distance / (float)MaxLen;
		}

	} // anonymous namespace

	float ScoreMatch(const SMatchQuery &Query, const SMatchCandidate &Candidate, const SMatchWeights &Weights)
	{
		const float TitleSim = LevenshteinSimilarity(Query.m_Title, Candidate.m_Title);
		const float ArtistSim = TokenSetRatio(Query.m_Artist, Candidate.m_Artist);
		const float DurSim = DurationScore(Query.m_DurationSec, Candidate.m_DurationSec);

		const float TotalWeight = Weights.m_TitleWeight + Weights.m_ArtistWeight + Weights.m_DurationWeight;
		if(TotalWeight <= 0.0f)
			return 0.0f;
		const float Weighted = TitleSim * Weights.m_TitleWeight + ArtistSim * Weights.m_ArtistWeight + DurSim * Weights.m_DurationWeight;
		return Weighted * 100.0f / TotalWeight;
	}

} // namespace QmLyrics

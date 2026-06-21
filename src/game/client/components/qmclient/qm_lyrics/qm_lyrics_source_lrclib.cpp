#include "qm_lyrics_source_lrclib.h"

#include <base/system.h>

#include <engine/external/json-parser/json.h>
#include <engine/http.h>
#include <engine/shared/http.h>

#include <cstring>

namespace QmLyrics
{

	namespace
	{

		void SetError(char *pErr, size_t ErrSize, const char *pMessage)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, pMessage, ErrSize);
		}

		// URL 编码（保留 unreserved，其余 %HH）。
		void UrlEncode(std::string_view In, std::string *pOut)
		{
			for(unsigned char C : In)
			{
				const bool Unreserved = (C >= 'A' && C <= 'Z') || (C >= 'a' && C <= 'z') ||
							(C >= '0' && C <= '9') || C == '-' || C == '_' || C == '.' || C == '~';
				if(Unreserved)
					pOut->push_back((char)C);
				else
				{
					char aBuf[4];
					str_format(aBuf, sizeof(aBuf), "%%%02X", (int)C);
					pOut->append(aBuf);
				}
			}
		}

		void AppendQueryParam(std::string *pOut, std::string_view Key, std::string_view Value, bool First)
		{
			pOut->push_back(First ? '?' : '&');
			pOut->append(Key);
			pOut->push_back('=');
			UrlEncode(Value, pOut);
		}

		const char *JsonString(const json_value *pVal, const char *pDefault = "")
		{
			if(pVal == nullptr || pVal->type != json_string)
				return pDefault;
			return pVal->u.string.ptr;
		}

		int JsonInt(const json_value *pVal, int Default = 0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_integer)
				return (int)pVal->u.integer;
			if(pVal->type == json_double)
				return (int)pVal->u.dbl;
			return Default;
		}

		bool ParseCandidate(const json_value *pItem, SSourceCandidate *pOut)
		{
			if(pItem == nullptr || pItem->type != json_object)
				return false;
			const json_value &Item = *pItem;

			const char *pSynced = JsonString(&Item["syncedLyrics"]);
			const char *pPlain = JsonString(&Item["plainLyrics"]);

			if((pSynced == nullptr || pSynced[0] == '\0') && (pPlain == nullptr || pPlain[0] == '\0'))
				return false;

			if(pSynced != nullptr && pSynced[0] != '\0')
			{
				pOut->m_RawText = pSynced;
				pOut->m_FormatHint = std::strchr(pSynced, '<') != nullptr ? EFormat::LRC_ENHANCED : EFormat::LRC_STANDARD;
			}
			else
			{
				pOut->m_RawText = pPlain;
				pOut->m_FormatHint = EFormat::LRC_STANDARD;
			}
			pOut->m_Metadata.m_Title = JsonString(&Item["trackName"]);
			pOut->m_Metadata.m_Artist = JsonString(&Item["artistName"]);
			pOut->m_Metadata.m_Album = JsonString(&Item["albumName"]);
			pOut->m_Metadata.m_DurationSec = JsonInt(&Item["duration"]);
			// LRCLIB 不给评分，固定 0.5 表示中性置信
			pOut->m_SourceScore = 0.5f;
			return true;
		}

		json_value *ParseJsonRoot(const char *pBody, size_t BodyLen, char *pErr, size_t ErrSize)
		{
			json_settings Settings{};
			char aJsonErr[json_error_max];
			json_value *pRoot = json_parse_ex(&Settings, pBody, BodyLen, aJsonErr);
			if(pRoot == nullptr)
			{
				SetError(pErr, ErrSize, aJsonErr);
				return nullptr;
			}
			return pRoot;
		}

	} // anonymous namespace

	std::string BuildLrclibGetUrl(const SSourceQuery &Query)
	{
		std::string Url = "https://lrclib.net/api/get";
		bool First = true;
		if(!Query.m_Title.empty())
		{
			AppendQueryParam(&Url, "track_name", Query.m_Title, First);
			First = false;
		}
		if(!Query.m_Artist.empty())
		{
			AppendQueryParam(&Url, "artist_name", Query.m_Artist, First);
			First = false;
		}
		if(!Query.m_Album.empty())
		{
			AppendQueryParam(&Url, "album_name", Query.m_Album, First);
			First = false;
		}
		if(Query.m_DurationSec > 0)
		{
			char aBuf[16];
			str_format(aBuf, sizeof(aBuf), "%d", Query.m_DurationSec);
			AppendQueryParam(&Url, "duration", aBuf, First);
			First = false;
		}
		return Url;
	}

	std::string BuildLrclibSearchUrl(const SSourceQuery &Query)
	{
		std::string Url = "https://lrclib.net/api/search";
		std::string Q = Query.m_Title;
		if(!Query.m_Artist.empty())
		{
			if(!Q.empty())
				Q.push_back(' ');
			Q.append(Query.m_Artist);
		}
		AppendQueryParam(&Url, "q", Q, true);
		return Url;
	}

	std::vector<SSourceCandidate> ParseLrclibGetResponse(const char *pBody, size_t BodyLen, char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		if(pBody == nullptr || BodyLen == 0)
		{
			SetError(pErr, ErrSize, "empty body");
			return {};
		}
		json_value *pRoot = ParseJsonRoot(pBody, BodyLen, pErr, ErrSize);
		if(pRoot == nullptr)
			return {};
		std::vector<SSourceCandidate> vResult;
		SSourceCandidate Candidate;
		if(ParseCandidate(pRoot, &Candidate))
			vResult.push_back(std::move(Candidate));
		json_value_free(pRoot);
		return vResult;
	}

	std::vector<SSourceCandidate> ParseLrclibSearchResponse(const char *pBody, size_t BodyLen, char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		if(pBody == nullptr || BodyLen == 0)
		{
			SetError(pErr, ErrSize, "empty body");
			return {};
		}
		json_value *pRoot = ParseJsonRoot(pBody, BodyLen, pErr, ErrSize);
		if(pRoot == nullptr)
			return {};
		std::vector<SSourceCandidate> vResult;
		if(pRoot->type == json_array)
		{
			for(unsigned int i = 0; i < pRoot->u.array.length; ++i)
			{
				SSourceCandidate Candidate;
				if(ParseCandidate(pRoot->u.array.values[i], &Candidate))
					vResult.push_back(std::move(Candidate));
			}
		}
		json_value_free(pRoot);
		return vResult;
	}

	struct CLyricsSourceLrclib::SImpl
	{
		IHttp *m_pHttp = nullptr;
		int m_TimeoutMs = 8000;
		std::shared_ptr<CHttpRequest> m_pRequest;
		FSourceDoneCallback m_Done;
		FSourceErrorCallback m_Error;
		SSourceQuery m_PendingQuery;
		enum class EStage
		{
			IDLE,
			GET,
			SEARCH,
		} m_Stage = EStage::IDLE;
	};

	CLyricsSourceLrclib::CLyricsSourceLrclib(IHttp *pHttp, int TimeoutMs) :
		m_pImpl(std::make_unique<SImpl>())
	{
		m_pImpl->m_pHttp = pHttp;
		m_pImpl->m_TimeoutMs = TimeoutMs > 0 ? TimeoutMs : 8000;
	}

	CLyricsSourceLrclib::~CLyricsSourceLrclib()
	{
		Cancel();
	}

	bool CLyricsSourceLrclib::BusyForTests() const
	{
		return m_pImpl->m_Stage != SImpl::EStage::IDLE;
	}

	void CLyricsSourceLrclib::Cancel()
	{
		if(m_pImpl->m_pRequest)
		{
			m_pImpl->m_pRequest->Abort();
			m_pImpl->m_pRequest.reset();
		}
		m_pImpl->m_Done = nullptr;
		m_pImpl->m_Error = nullptr;
		m_pImpl->m_Stage = SImpl::EStage::IDLE;
	}

	namespace
	{

		void DispatchRequest(CLyricsSourceLrclib::SImpl *pImpl, const std::string &Url)
		{
			pImpl->m_pRequest = std::make_shared<CHttpRequest>(Url.c_str());
			pImpl->m_pRequest->Timeout(CTimeout{pImpl->m_TimeoutMs, pImpl->m_TimeoutMs, 500, 5});
			pImpl->m_pRequest->LogProgress(HTTPLOG::FAILURE);
			pImpl->m_pRequest->HeaderString("User-Agent", "QmClient (https://github.com/Q1menG)");
			pImpl->m_pHttp->Run(pImpl->m_pRequest);
		}

	} // anonymous namespace

	void CLyricsSourceLrclib::QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error)
	{
		Cancel();
		if(m_pImpl->m_pHttp == nullptr)
		{
			if(Error)
				Error("no IHttp");
			return;
		}
		m_pImpl->m_Done = std::move(Done);
		m_pImpl->m_Error = std::move(Error);
		m_pImpl->m_PendingQuery = Query;
		m_pImpl->m_Stage = SImpl::EStage::GET;
		DispatchRequest(m_pImpl.get(), BuildLrclibGetUrl(Query));
	}

} // namespace QmLyrics

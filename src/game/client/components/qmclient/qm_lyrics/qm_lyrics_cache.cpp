#include "qm_lyrics_cache.h"

#include "qm_lyrics_match.h"

#include <base/hash.h>
#include <base/system.h>

#include <engine/external/json-parser/json.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace QmLyrics
{

	namespace
	{

		void EscapeJsonString(std::string_view In, std::string *pOut)
		{
			pOut->push_back('"');
			for(char C : In)
			{
				switch(C)
				{
				case '"': pOut->append("\\\""); break;
				case '\\': pOut->append("\\\\"); break;
				case '\b': pOut->append("\\b"); break;
				case '\f': pOut->append("\\f"); break;
				case '\n': pOut->append("\\n"); break;
				case '\r': pOut->append("\\r"); break;
				case '\t': pOut->append("\\t"); break;
				default:
					if((unsigned char)C < 0x20)
					{
						char aBuf[8];
						str_format(aBuf, sizeof(aBuf), "\\u%04x", (unsigned)(unsigned char)C);
						pOut->append(aBuf);
					}
					else
					{
						pOut->push_back(C);
					}
				}
			}
			pOut->push_back('"');
		}

		const char *JsonString(const json_value *pVal, const char *pDefault = "")
		{
			if(pVal == nullptr || pVal->type != json_string)
				return pDefault;
			return pVal->u.string.ptr;
		}

		int64_t JsonInt(const json_value *pVal, int64_t Default = 0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_integer)
				return pVal->u.integer;
			if(pVal->type == json_double)
				return (int64_t)pVal->u.dbl;
			return Default;
		}

		double JsonDouble(const json_value *pVal, double Default = 0.0)
		{
			if(pVal == nullptr)
				return Default;
			if(pVal->type == json_double)
				return pVal->u.dbl;
			if(pVal->type == json_integer)
				return (double)pVal->u.integer;
			return Default;
		}

	} // anonymous namespace

	std::string BuildCacheKey(std::string_view Title, std::string_view Artist, std::string_view Album, int DurationSec)
	{
		std::string Out;
		Out.reserve(Title.size() + Artist.size() + Album.size() + 32);
		Out.append(NormalizeForMatch(Title));
		Out.push_back('|');
		Out.append(NormalizeForMatch(Artist));
		Out.push_back('|');
		Out.append(NormalizeForMatch(Album));
		Out.push_back('|');
		char aBuf[16];
		str_format(aBuf, sizeof(aBuf), "%d", DurationSec);
		Out.append(aBuf);
		return Out;
	}

	std::string FileNameForKey(std::string_view Key)
	{
		const SHA256_DIGEST Digest = sha256(Key.data(), Key.size());
		char aHex[SHA256_MAXSTRSIZE];
		sha256_str(Digest, aHex, sizeof(aHex));
		// 前 16 hex 字符 + ".json"
		char aOut[32];
		str_format(aOut, sizeof(aOut), "%.16s.json", aHex);
		return aOut;
	}

	std::vector<std::string> CCacheIndex::Upsert(const SCacheEntry &Entry, int MaxEntries)
	{
		std::vector<std::string> vEvicted;
		auto It = m_mEntries.find(Entry.m_Key);
		if(It != m_mEntries.end())
		{
			// 替换：旧文件名若不同则淘汰
			if(It->second.m_FileName != Entry.m_FileName)
				vEvicted.push_back(It->second.m_FileName);
			It->second = Entry;
		}
		else
		{
			m_mEntries.emplace(Entry.m_Key, Entry);
		}

		if(MaxEntries > 0 && (int)m_mEntries.size() > MaxEntries)
		{
			// 按 LastUsedAt 升序淘汰至 MaxEntries
			std::vector<std::pair<int64_t, std::string>> vSorted;
			vSorted.reserve(m_mEntries.size());
			for(const auto &Pair : m_mEntries)
				vSorted.emplace_back(Pair.second.m_LastUsedAt, Pair.first);
			std::sort(vSorted.begin(), vSorted.end(), [](const auto &A, const auto &B) {
				return A.first < B.first;
			});
			const int Excess = (int)m_mEntries.size() - MaxEntries;
			for(int i = 0; i < Excess; ++i)
			{
				auto It2 = m_mEntries.find(vSorted[i].second);
				if(It2 != m_mEntries.end())
				{
					vEvicted.push_back(It2->second.m_FileName);
					m_mEntries.erase(It2);
				}
			}
		}
		return vEvicted;
	}

	const SCacheEntry *CCacheIndex::Lookup(std::string_view Key, int64_t NowUnixSec)
	{
		auto It = m_mEntries.find(std::string(Key));
		if(It == m_mEntries.end())
			return nullptr;
		It->second.m_LastUsedAt = NowUnixSec;
		return &It->second;
	}

	std::vector<std::string> CCacheIndex::EvictExpired(int TtlDays, int64_t NowUnixSec)
	{
		std::vector<std::string> vEvicted;
		if(TtlDays <= 0)
			return vEvicted;
		const int64_t TtlSec = (int64_t)TtlDays * 86400;
		for(auto It = m_mEntries.begin(); It != m_mEntries.end();)
		{
			if(NowUnixSec - It->second.m_StoredAt > TtlSec)
			{
				vEvicted.push_back(It->second.m_FileName);
				It = m_mEntries.erase(It);
			}
			else
			{
				++It;
			}
		}
		return vEvicted;
	}

	std::string CCacheIndex::ToJson() const
	{
		std::string Out;
		Out.reserve(m_mEntries.size() * 128);
		Out.append("{\"entries\":[");
		bool First = true;
		for(const auto &Pair : m_mEntries)
		{
			if(!First)
				Out.push_back(',');
			First = false;
			Out.push_back('{');
			Out.append("\"key\":");
			EscapeJsonString(Pair.second.m_Key, &Out);
			Out.append(",\"file\":");
			EscapeJsonString(Pair.second.m_FileName, &Out);
			Out.append(",\"source\":");
			EscapeJsonString(Pair.second.m_Source, &Out);
			char aBuf[64];
			str_format(aBuf, sizeof(aBuf), ",\"score\":%.2f", Pair.second.m_Score);
			Out.append(aBuf);
			str_format(aBuf, sizeof(aBuf), ",\"used\":%lld", (long long)Pair.second.m_LastUsedAt);
			Out.append(aBuf);
			str_format(aBuf, sizeof(aBuf), ",\"stored\":%lld", (long long)Pair.second.m_StoredAt);
			Out.append(aBuf);
			Out.push_back('}');
		}
		Out.append("]}");
		return Out;
	}

	bool CCacheIndex::FromJson(std::string_view Json, char *pErr, size_t ErrSize)
	{
		if(pErr != nullptr && ErrSize > 0)
			pErr[0] = '\0';
		if(Json.empty())
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "empty input", ErrSize);
			return false;
		}
		json_settings Settings{};
		char aJsonErr[json_error_max];
		json_value *pRoot = json_parse_ex(&Settings, Json.data(), Json.size(), aJsonErr);
		if(pRoot == nullptr)
		{
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, aJsonErr, ErrSize);
			return false;
		}
		if(pRoot->type != json_object)
		{
			json_value_free(pRoot);
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "root not object", ErrSize);
			return false;
		}

		std::unordered_map<std::string, SCacheEntry> mNew;
		const json_value &Entries = (*pRoot)["entries"];
		if(Entries.type != json_array)
		{
			json_value_free(pRoot);
			if(pErr != nullptr && ErrSize > 0)
				str_copy(pErr, "entries missing/not array", ErrSize);
			return false;
		}

		for(unsigned int i = 0; i < Entries.u.array.length; ++i)
		{
			const json_value *pItem = Entries.u.array.values[i];
			if(pItem == nullptr || pItem->type != json_object)
				continue;
			SCacheEntry Entry;
			Entry.m_Key = JsonString(&(*pItem)["key"]);
			Entry.m_FileName = JsonString(&(*pItem)["file"]);
			Entry.m_Source = JsonString(&(*pItem)["source"]);
			Entry.m_Score = (float)JsonDouble(&(*pItem)["score"]);
			Entry.m_LastUsedAt = JsonInt(&(*pItem)["used"]);
			Entry.m_StoredAt = JsonInt(&(*pItem)["stored"]);
			if(Entry.m_Key.empty() || Entry.m_FileName.empty())
				continue;
			mNew.emplace(Entry.m_Key, std::move(Entry));
		}

		json_value_free(pRoot);
		m_mEntries = std::move(mNew);
		return true;
	}

} // namespace QmLyrics

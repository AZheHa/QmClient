#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_LYRICS_COMPONENT_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_LYRICS_COMPONENT_H

#include "lyrics/lyric_model.h"

#include <game/client/component.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class CHttpRequest;
typedef struct _json_value json_value;

class CLyrics : public CComponent
{
public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnUpdate() override;

	bool HasLyrics() const { return m_State == EState::READY; }
	bool GetCurrentLine(char *pBuf, size_t BufSize, int64_t PositionMs) const;
	bool GetNextLine(char *pBuf, size_t BufSize, int64_t PositionMs) const;

private:
	struct STrackInfo
	{
		char m_aSourceAppId[128] = {};
		char m_aTitle[128] = {};
		char m_aArtist[128] = {};
		char m_aAlbum[128] = {};
		int m_DurationSec = 0;
	};

	enum class EState
	{
		IDLE,
		REQUESTING,
		READY,
		ERROR,
	};

	enum class EEndpoint
	{
		QQ_SEARCH,
		QQ_LYRIC,
		QQ_LYRIC_OLD,
		NETEASE_SEARCH,
		NETEASE_SEARCH_LEGACY,
		NETEASE_LYRIC,
		LRCLIB_CACHED,
		LRCLIB_FULL,
		LRCLIB_SEARCH,
	};

	enum class ESource
	{
		QQ,
		NETEASE,
		LRCLIB,
	};

	static const char *EndpointName(EEndpoint Endpoint);
	static const char *SourceName(ESource Source);
	void ResetState();
	void ClearLyrics();
	void CancelRequest();
	ESource PreferredSource() const;
	void StartSource(ESource Source);
	void StartNextSource(ESource FailedSource);
	void StartRequest(EEndpoint Endpoint);
	void StartNextFallback(const char *pReason);
	void HandleRequestDone();
	bool ParseQqSearchResponse(const json_value *pObj, char *pErr, size_t ErrSize);
	bool ParseQqLyricResponse(CHttpRequest *pRequest, bool OldEndpoint, char *pErr, size_t ErrSize);
	bool ParseNeteaseSearchResponse(const json_value *pObj, char *pErr, size_t ErrSize);
	bool ParseNeteaseLyricResponse(const json_value *pObj, char *pErr, size_t ErrSize);
	bool ParseLyricsResponse(const json_value *pObj, char *pErr, size_t ErrSize);
	bool ParseSearchResponse(const json_value *pObj, char *pErr, size_t ErrSize);
	bool ApplyLyrics(const std::string &Plain, const std::string &Synced, bool Instrumental, char *pErr, size_t ErrSize);
	bool ApplyLines(std::vector<CLyricLine> &&vLines, char *pErr, size_t ErrSize);
	std::string BuildSignature(const STrackInfo &Info) const;
	int64_t GetDisplayPositionMs(int64_t PositionMs) const;
	bool IsValidTrack(const STrackInfo &Info) const;

	std::shared_ptr<CHttpRequest> m_pRequest;
	EEndpoint m_RequestEndpoint = EEndpoint::LRCLIB_CACHED;
	EState m_State = EState::IDLE;
	bool m_IsSynced = false;

	STrackInfo m_Track;
	std::string m_LastSignature;
	std::string m_RequestSignature;
	std::string m_QqSongId;
	std::string m_QqSongMid;
	std::string m_NeteaseSongId;
	std::vector<CLyricLine> m_Lines;
	std::string m_PlainLine;
	int64_t m_LastMediaPositionMs = -1;
	int64_t m_DisplayPositionBaseMs = 0;
	int64_t m_DisplayPositionBaseTick = 0;
	bool m_DisplayPositionEstimating = false;
	char m_aLastError[128] = {};
};

#endif

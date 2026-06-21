#include "qm_lyrics.h"

#include "qm_lyrics_cache.h"
#include "qm_lyrics_match.h"
#include "qm_lyrics_parser_lrc.h"
#include "qm_lyrics_parser_ttml.h"
#include "qm_lyrics_source_lrclib.h"

#include <base/color.h>
#include <base/system.h>

#include <engine/client.h>
#include <engine/graphics.h>
#include <engine/http.h>
#include <engine/shared/config.h>
#include <engine/shared/http.h>
#include <engine/textrender.h>

#include <game/client/components/hud_editor.h>
#include <game/client/components/system_media_controls.h>
#include <game/client/gameclient.h>
#include <game/client/ui_rect.h>
#include <game/generated/protocol.h>

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>

namespace
{

	constexpr float HUD_WIDTH = 240.0f;
	constexpr float HUD_PADDING_X = 8.0f;
	constexpr float HUD_PADDING_Y = 6.0f;

	QmLyrics::SMatchQuery BuildQueryFromMedia(const CSystemMediaControls::SState &State)
	{
		QmLyrics::SMatchQuery Q;
		Q.m_Title = State.m_aTitle;
		Q.m_Artist = State.m_aArtist;
		Q.m_Album = State.m_aAlbum;
		Q.m_DurationSec = State.m_DurationMs > 0 ? (int)(State.m_DurationMs / 1000) : 0;
		return Q;
	}

} // namespace

struct CQmLyrics::SImpl
{
	enum class EState
	{
		IDLE,
		FETCHING_GET,
		READY,
		NO_RESULT,
	};

	EState m_State = EState::IDLE;
	std::string m_LastTrackKey;
	QmLyrics::SMatchQuery m_PendingQuery;
	std::shared_ptr<CHttpRequest> m_pRequest;
	QmLyrics::SLyricsTrack m_Track;
	QmLyrics::CClockInterpolator m_Clock;
	QmLyrics::CCacheIndex m_Cache;
	int m_ActiveLineIndex = -1;
	int64_t m_LastSmtcPositionMs = -1;
	IHttp *m_pHttp = nullptr;
};

CQmLyrics::CQmLyrics() :
	m_pImpl(std::make_unique<SImpl>())
{
}

CQmLyrics::~CQmLyrics() = default;

void CQmLyrics::OnInit()
{
	m_pImpl->m_pHttp = Kernel()->RequestInterface<IHttp>();
}

void CQmLyrics::OnShutdown()
{
	if(m_pImpl->m_pRequest)
		m_pImpl->m_pRequest->Abort();
	m_pImpl->m_pRequest.reset();
}

void CQmLyrics::OnReset()
{
	if(m_pImpl->m_pRequest)
		m_pImpl->m_pRequest->Abort();
	m_pImpl->m_pRequest.reset();
	m_pImpl->m_Track.m_vLines.clear();
	m_pImpl->m_LastTrackKey.clear();
	m_pImpl->m_State = SImpl::EState::IDLE;
	m_pImpl->m_Clock.Reset();
	m_pImpl->m_ActiveLineIndex = -1;
}

namespace
{

	void DispatchLrclib(std::shared_ptr<CHttpRequest> *ppOut, IHttp *pHttp, const std::string &Url, int TimeoutMs)
	{
		*ppOut = std::make_shared<CHttpRequest>(Url.c_str());
		(*ppOut)->Timeout(CTimeout{TimeoutMs, TimeoutMs, 500, 5});
		(*ppOut)->LogProgress(HTTPLOG::FAILURE);
		(*ppOut)->HeaderString("User-Agent", "QmClient (https://github.com/Q1menG)");
		pHttp->Run(*ppOut);
	}

} // namespace

void CQmLyrics::TickStateMachine()
{
	if(!g_Config.m_QmLyrics || !g_Config.m_QmSmtcEnable)
		return;

	CSystemMediaControls::SState MediaState{};
	const bool HasMedia = GameClient()->m_SystemMediaControls.GetStateSnapshot(MediaState);
	if(!HasMedia)
	{
		m_pImpl->m_State = SImpl::EState::IDLE;
		return;
	}

	// 时钟同步
	if(MediaState.m_PositionMs != m_pImpl->m_LastSmtcPositionMs)
	{
		m_pImpl->m_Clock.Anchor(MediaState.m_PositionMs, time_get(), 1.0);
		m_pImpl->m_LastSmtcPositionMs = MediaState.m_PositionMs;
	}
	m_pImpl->m_Clock.SetPlaying(MediaState.m_Playing, time_get());
	m_pImpl->m_Clock.SetOffsetMs(g_Config.m_QmLyricsOffsetMs);
	m_pImpl->m_Clock.SetDriftCorrectMs(g_Config.m_QmLyricsDriftCorrectMs);

	// 曲目变化检测
	const std::string TrackKey = QmLyrics::BuildCacheKey(MediaState.m_aTitle, MediaState.m_aArtist, MediaState.m_aAlbum, (int)(MediaState.m_DurationMs / 1000));
	if(TrackKey != m_pImpl->m_LastTrackKey)
	{
		m_pImpl->m_LastTrackKey = TrackKey;
		m_pImpl->m_Track.m_vLines.clear();
		m_pImpl->m_ActiveLineIndex = -1;
		if(m_pImpl->m_pRequest)
		{
			m_pImpl->m_pRequest->Abort();
			m_pImpl->m_pRequest.reset();
		}
		m_pImpl->m_State = SImpl::EState::IDLE;
		m_pImpl->m_PendingQuery = BuildQueryFromMedia(MediaState);

		// 触发抓取
		if(g_Config.m_QmLyricsAutoFetch && m_pImpl->m_pHttp != nullptr)
		{
			const std::string Url = QmLyrics::BuildLrclibGetUrl(m_pImpl->m_PendingQuery);
			DispatchLrclib(&m_pImpl->m_pRequest, m_pImpl->m_pHttp, Url, g_Config.m_QmLyricsHttpTimeoutMs);
			m_pImpl->m_State = SImpl::EState::FETCHING_GET;
		}
	}

	// 处理 in-flight 请求
	if(m_pImpl->m_pRequest && m_pImpl->m_pRequest->Done())
	{
		const EHttpState ReqState = m_pImpl->m_pRequest->State();
		if(ReqState == EHttpState::DONE && m_pImpl->m_pRequest->StatusCode() == 200)
		{
			unsigned char *pBody = nullptr;
			size_t BodyLen = 0;
			m_pImpl->m_pRequest->Result(&pBody, &BodyLen);
			if(pBody != nullptr && BodyLen > 0)
			{
				auto vC = QmLyrics::ParseLrclibGetResponse((const char *)pBody, BodyLen);
				if(!vC.empty())
				{
					const float Threshold = (float)g_Config.m_QmLyricsMatchThreshold;
					float BestScore = -1.0f;
					const QmLyrics::SSourceCandidate *pBest = nullptr;
					for(const QmLyrics::SSourceCandidate &C : vC)
					{
						const float S = QmLyrics::Score(m_pImpl->m_PendingQuery, C.m_Metadata);
						if(S > BestScore)
						{
							BestScore = S;
							pBest = &C;
						}
					}
					if(pBest != nullptr && BestScore >= Threshold)
					{
						QmLyrics::SLyricsTrack Track;
						const bool Parsed = pBest->m_FormatHint == QmLyrics::EFormat::TTML ?
									    QmLyrics::ParseTtml(pBest->m_RawText.c_str(), pBest->m_RawText.size(), &Track) :
									    QmLyrics::ParseLrc(pBest->m_RawText.c_str(), pBest->m_RawText.size(), &Track);
						if(Parsed)
						{
							m_pImpl->m_Track = std::move(Track);
							m_pImpl->m_State = SImpl::EState::READY;

							const int64_t NowSec = time_timestamp();
							QmLyrics::SCacheEntry E;
							E.m_Key = TrackKey;
							E.m_FileName = QmLyrics::FileNameForKey(TrackKey);
							E.m_Source = "lrclib";
							E.m_Score = BestScore;
							E.m_StoredAt = NowSec;
							E.m_LastUsedAt = NowSec;
							m_pImpl->m_Cache.Upsert(E, 1000);
						}
						else
						{
							m_pImpl->m_State = SImpl::EState::NO_RESULT;
						}
					}
					else
					{
						m_pImpl->m_State = SImpl::EState::NO_RESULT;
					}
				}
				else
				{
					m_pImpl->m_State = SImpl::EState::NO_RESULT;
				}
			}
			else
			{
				m_pImpl->m_State = SImpl::EState::NO_RESULT;
			}
		}
		else
		{
			m_pImpl->m_State = SImpl::EState::NO_RESULT;
		}
		m_pImpl->m_pRequest.reset();
	}

	// 二分活动行
	if(m_pImpl->m_State == SImpl::EState::READY && !m_pImpl->m_Track.m_vLines.empty())
	{
		const int64_t Now = m_pImpl->m_Clock.Now(time_get(), time_freq());
		int Lo = 0;
		int Hi = (int)m_pImpl->m_Track.m_vLines.size() - 1;
		int Active = -1;
		while(Lo <= Hi)
		{
			const int Mid = (Lo + Hi) / 2;
			const QmLyrics::SLyricsLine &L = m_pImpl->m_Track.m_vLines[Mid];
			if(Now < L.m_StartMs)
				Hi = Mid - 1;
			else if(Now >= L.m_EndMs)
				Lo = Mid + 1;
			else
			{
				Active = Mid;
				break;
			}
		}
		m_pImpl->m_ActiveLineIndex = Active;
	}
}

void CQmLyrics::RenderHud()
{
	if(!g_Config.m_QmLyrics)
		return;
	if(m_pImpl->m_State != SImpl::EState::READY || m_pImpl->m_Track.m_vLines.empty())
		return;

	const int Active = m_pImpl->m_ActiveLineIndex;
	if(Active < 0)
		return;

	const int Above = std::clamp(g_Config.m_QmLyricsLinesAbove, 0, 6);
	const int Below = std::clamp(g_Config.m_QmLyricsLinesBelow, 0, 6);
	const float FontActive = (float)g_Config.m_QmLyricsFontSize;
	const float FontOther = (float)g_Config.m_QmLyricsFontSizeOther;
	const float LineGap = (float)g_Config.m_QmLyricsLineSpacing;
	const float Opacity = std::clamp(g_Config.m_QmLyricsOpacity / 100.0f, 0.0f, 1.0f);
	const float InactiveMinOpacity = std::clamp(g_Config.m_QmLyricsInactiveOpacity / 100.0f, 0.0f, 1.0f);
	const float ScaleActive = std::clamp(g_Config.m_QmLyricsScaleActive / 100.0f, 1.0f, 2.0f);
	const float ScaleFalloff = std::clamp(g_Config.m_QmLyricsScaleFalloff / 100.0f, 0.0f, 0.2f);
	const float FadePerLine = std::clamp(g_Config.m_QmLyricsFadePerLine / 100.0f, 0.0f, 0.4f);

	const float TotalHeight = (Above + Below + 1) * (FontActive + LineGap);
	const float ScreenW = (float)Graphics()->ScreenWidth();
	const float ScreenH = (float)Graphics()->ScreenHeight();
	const float DefaultX = ScreenW * 0.5f - HUD_WIDTH * 0.5f;
	const float DefaultY = ScreenH * 0.7f - TotalHeight * 0.5f;

	const CUIRect DefaultRect = {DefaultX, DefaultY, HUD_WIDTH, TotalHeight};
	const QmHudEditor::SEdgeMargin Margin = QmHudEditor::SEdgeMargin::Uniform((float)g_Config.m_QmLyricsEdgeMargin);
	const auto Scope = GameClient()->m_HudEditor.BeginTransform(EHudEditorElement::Lyrics, DefaultRect, DefaultRect, Margin);
	const CUIRect &Rect = Scope.m_TargetRect;

	ColorRGBA Played = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmLyricsColorPlayed, true));
	ColorRGBA Unplayed = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmLyricsColorUnplayed, true));

	const unsigned int PrevFlags = TextRender()->GetRenderFlags();
	const ColorRGBA PrevTextColor = TextRender()->GetTextColor();
	TextRender()->SetRenderFlags(ETextRenderFlags::TEXT_RENDER_FLAG_NO_PIXEL_ALIGNMENT);

	const int Total = (int)m_pImpl->m_Track.m_vLines.size();
	const int FirstIdx = std::max(0, Active - Above);
	const int LastIdx = std::min(Total - 1, Active + Below);
	float Y = Rect.y + HUD_PADDING_Y;
	for(int i = FirstIdx; i <= LastIdx; ++i)
	{
		const int Distance = std::abs(i - Active);
		const float Scale = (i == Active) ? ScaleActive : std::max(0.5f, 1.0f - (float)Distance * ScaleFalloff);
		const float Alpha = (i == Active) ? Opacity : std::max(InactiveMinOpacity, Opacity * (1.0f - (float)Distance * FadePerLine));
		const float FontSize = (i == Active ? FontActive : FontOther) * Scale;

		ColorRGBA Color = (i == Active) ? Played : Unplayed;
		Color.a = Alpha;
		TextRender()->TextColor(Color);

		CTextCursor Cursor;
		Cursor.m_FontSize = FontSize;
		Cursor.m_LineWidth = Rect.w - HUD_PADDING_X * 2.0f;
		Cursor.m_Flags = TEXTFLAG_RENDER | TEXTFLAG_ELLIPSIS_AT_END;
		Cursor.SetPosition(vec2(Rect.x + HUD_PADDING_X, Y));
		TextRender()->TextEx(&Cursor, m_pImpl->m_Track.m_vLines[i].m_RawText.c_str());

		Y += FontSize + LineGap;
	}

	TextRender()->TextColor(PrevTextColor);
	TextRender()->SetRenderFlags(PrevFlags);

	GameClient()->m_HudEditor.EndTransform(Scope);
}

void CQmLyrics::OnRender()
{
	TickStateMachine();
	RenderHud();
}

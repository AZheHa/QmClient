#include "qm_lyrics_clock.h"

#include <algorithm>
#include <cstdlib>

namespace QmLyrics
{

	CClockInterpolator::CClockInterpolator() = default;

	void CClockInterpolator::Reset()
	{
		m_AnchorPositionMs = 0;
		m_AnchorWallTick = 0;
		m_Rate = 1.0;
		m_Playing = false;
		m_LastActualMs = 0;
		m_LastTargetMs = 0;
		m_HasState = false;
	}

	void CClockInterpolator::Anchor(int64_t AnchorPositionMs, int64_t AnchorWallTick, double Rate)
	{
		m_AnchorPositionMs = AnchorPositionMs;
		m_AnchorWallTick = AnchorWallTick;
		m_Rate = Rate;
	}

	void CClockInterpolator::SetPlaying(bool Playing, int64_t NowWallTick)
	{
		if(Playing == m_Playing)
			return;
		if(Playing)
		{
			// 从暂停恢复：刷新 anchor 到当前时刻，position 不变。
			m_AnchorWallTick = NowWallTick;
		}
		else
		{
			// 暂停瞬间：把当前推算的 position 锁住为新 anchor。
			// 这样后续 NowWallTick 推进时 (Tick - AnchorWallTick) 仍是 0。
			// 我们不在这里调 Now（会要 TickFreq）；调用方暂停时不会调 Now 也是安全的，
			// 因为 m_Playing=false 时 Now 直接返回 m_AnchorPositionMs。
		}
		m_Playing = Playing;
	}

	int64_t CClockInterpolator::Now(int64_t NowWallTick, int64_t TickFreq)
	{
		int64_t TargetMs = m_AnchorPositionMs;
		if(m_Playing && TickFreq > 0)
		{
			const int64_t DeltaTicks = NowWallTick - m_AnchorWallTick;
			const int64_t DeltaMs = (int64_t)((double)DeltaTicks * 1000.0 * m_Rate / (double)TickFreq);
			TargetMs += DeltaMs;
		}
		TargetMs += m_OffsetMs;

		if(!m_HasState)
		{
			m_LastActualMs = TargetMs;
			m_LastTargetMs = TargetMs;
			m_HasState = true;
			return m_LastActualMs;
		}

		const int64_t Diff = std::abs(TargetMs - m_LastActualMs);
		if(Diff > m_DriftCorrectMs)
		{
			// 硬切
			m_LastActualMs = TargetMs;
		}
		else
		{
			// 平滑：actual = lerp(actual, target, 0.2)
			const int64_t Delta = TargetMs - m_LastActualMs;
			m_LastActualMs += Delta / 5; // 0.2 = 1/5
		}
		m_LastTargetMs = TargetMs;
		return m_LastActualMs;
	}

} // namespace QmLyrics

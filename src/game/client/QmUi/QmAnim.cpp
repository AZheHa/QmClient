// 请抬头享受阳光｜日子很好 我很我---------致咩子
/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QmAnim.h"

#include "QmMotion.h"

#include <engine/shared/config.h>

#include <algorithm>
#include <cmath>

void CUiV2AnimationRuntime::Reset()
{
	m_TimeSec = 0.0f;
	m_CompletedEvents.clear();
	m_NextTrackId = 1;
	m_Values.clear();
	m_ActiveTracks.clear();
	m_QueuedTracks.clear();
}

static float SolveBezierY(float TargetX, const SUiBezier &Bezier)
{
	auto SampleX = [&](float t) {
		const float OneMinusT = 1.0f - t;
		return 3.0f * OneMinusT * OneMinusT * t * Bezier.m_X1 + 3.0f * OneMinusT * t * t * Bezier.m_X2 + t * t * t;
	};
	auto SampleXPrime = [&](float t) {
		const float OneMinusT = 1.0f - t;
		return 3.0f * OneMinusT * OneMinusT * Bezier.m_X1 + 6.0f * OneMinusT * t * (Bezier.m_X2 - Bezier.m_X1) + 3.0f * t * t * (1.0f - Bezier.m_X2);
	};

	float t = TargetX;
	for(int i = 0; i < 8; ++i)
	{
		const float CurrentX = SampleX(t);
		const float Err = CurrentX - TargetX;
		if(std::abs(Err) < 1e-5f)
			break;
		const float Slope = SampleXPrime(t);
		if(std::abs(Slope) < 1e-6f)
			break;
		t -= Err / Slope;
		t = std::clamp(t, 0.0f, 1.0f);
	}

	const float OneMinusT = 1.0f - t;
	return 3.0f * OneMinusT * OneMinusT * t * Bezier.m_Y1 + 3.0f * OneMinusT * t * t * Bezier.m_Y2 + t * t * t;
}

float CUiV2AnimationRuntime::ApplyEasing(float t, const SUiAnimTransition &Transition) const
{
	const float Clamped = std::clamp(t, 0.0f, 1.0f);
	switch(Transition.m_Easing)
	{
	case EEasing::LINEAR:
		return Clamped;
	case EEasing::EASE_IN:
		return Clamped * Clamped;
	case EEasing::EASE_OUT:
		return Clamped * (2.0f - Clamped);
	case EEasing::EASE_IN_OUT:
		return Clamped < 0.5f ? 2.0f * Clamped * Clamped : -1.0f + (4.0f - 2.0f * Clamped) * Clamped;
	case EEasing::EASE_OUT_QUART:
	{
		const float OneMinusT = 1.0f - Clamped;
		return 1.0f - OneMinusT * OneMinusT * OneMinusT * OneMinusT;
	}
	case EEasing::EASE_OUT_BACK:
	{
		constexpr float C1 = 1.70158f;
		constexpr float C3 = C1 + 1.0f;
		const float Shifted = Clamped - 1.0f;
		return 1.0f + C3 * Shifted * Shifted * Shifted + C1 * Shifted * Shifted;
	}
	case EEasing::EASE_IN_OUT_CUBIC:
		return Clamped < 0.5f ? 4.0f * Clamped * Clamped * Clamped : 1.0f - std::pow(-2.0f * Clamped + 2.0f, 3.0f) / 2.0f;
	case EEasing::CUBIC_BEZIER:
		return SolveBezierY(Clamped, Transition.m_Bezier);
	}
	return Clamped;
}

float CUiV2AnimationRuntime::TrackProgress(const SActiveTrack &Track) const
{
	const float Duration = std::max(0.0f, Track.m_Transition.m_DurationSec);
	const float Delay = std::max(0.0f, Track.m_Transition.m_DelaySec);
	const float LocalElapsed = std::max(0.0f, Track.m_ElapsedSec - Delay);
	if(Duration <= 0.0f)
		return LocalElapsed > 0.0f ? 1.0f : 0.0f;
	return std::clamp(LocalElapsed / Duration, 0.0f, 1.0f);
}

bool CUiV2AnimationRuntime::StartTrack(const STrackKey &Key, const SUiAnimRequest &Request, float StartValue)
{
	SActiveTrack Track;
	Track.m_Start = StartValue;
	Track.m_Target = Request.m_Target;
	Track.m_Current = StartValue;
	Track.m_ElapsedSec = 0.0f;
	Track.m_Velocity = 0.0f;
	Track.m_RestTimerSec = 0.0f;
	Track.m_Transition = Request.m_Transition;
	if(Track.m_Transition.m_DurationSec < 0.0f)
		Track.m_Transition.m_DurationSec = 0.0f;
	if(Track.m_Transition.m_DelaySec < 0.0f)
		Track.m_Transition.m_DelaySec = 0.0f;
	Track.m_TrackId = Request.m_TrackId != 0 ? Request.m_TrackId : m_NextTrackId++;

	const bool IsSpring = Track.m_Transition.m_Driver == EUiAnimDriver::SPRING;
	if(!IsSpring && Track.m_Transition.m_DurationSec <= 0.0f && Track.m_Transition.m_DelaySec <= 0.0f)
	{
		m_Values[Key] = Track.m_Target;
		m_CompletedEvents.push_back({Key.m_NodeKey, Key.m_Property, Track.m_TrackId});
		return false;
	}

	if(IsSpring && std::abs(Track.m_Current - Track.m_Target) < Track.m_Transition.m_Spring.m_RestEpsilon)
	{
		m_Values[Key] = Track.m_Target;
		m_CompletedEvents.push_back({Key.m_NodeKey, Key.m_Property, Track.m_TrackId});
		return false;
	}

	m_ActiveTracks[Key] = Track;
	m_Values[Key] = StartValue;
	return true;
}

void CUiV2AnimationRuntime::StartQueuedTracks(const STrackKey &Key, float StartValue)
{
	float CurrentStartValue = StartValue;
	while(true)
	{
		auto ItQueue = m_QueuedTracks.find(Key);
		if(ItQueue == m_QueuedTracks.end() || ItQueue->second.empty())
			return;

		SUiAnimRequest Next = ItQueue->second.front();
		ItQueue->second.pop_front();
		if(ItQueue->second.empty())
			m_QueuedTracks.erase(ItQueue);

		if(StartTrack(Key, Next, CurrentStartValue))
			return;

		CurrentStartValue = Next.m_Target;
	}
}

void CUiV2AnimationRuntime::CompleteTrack(const STrackKey &Key, const SActiveTrack &Track)
{
	const float Target = Track.m_Target;
	const uint32_t TrackId = Track.m_TrackId;
	m_Values[Key] = Target;
	m_CompletedEvents.push_back({Key.m_NodeKey, Key.m_Property, TrackId});
	m_ActiveTracks.erase(Key);
	StartQueuedTracks(Key, Target);
}

float CUiV2AnimationRuntime::CurrentValueFor(const STrackKey &Key, float DefaultValue) const
{
	const auto ItActive = m_ActiveTracks.find(Key);
	if(ItActive != m_ActiveTracks.end())
		return ItActive->second.m_Current;

	const auto ItValue = m_Values.find(Key);
	if(ItValue != m_Values.end())
		return ItValue->second;

	return DefaultValue;
}

void CUiV2AnimationRuntime::SetValue(uint64_t NodeKey, EUiAnimProperty Property, float Value)
{
	const STrackKey Key{NodeKey, Property};
	m_Values[Key] = Value;
	m_ActiveTracks.erase(Key);
	m_QueuedTracks.erase(Key);
}

float CUiV2AnimationRuntime::GetValue(uint64_t NodeKey, EUiAnimProperty Property, float DefaultValue) const
{
	const STrackKey Key{NodeKey, Property};
	return CurrentValueFor(Key, DefaultValue);
}

bool CUiV2AnimationRuntime::RequestAnimation(const SUiAnimRequest &Request)
{
	SUiAnimRequest EffectiveRequest = Request;
	if(Request.m_Transition.m_RespectMotionLevel)
		EffectiveRequest.m_Transition = qm_motion::ApplyMotionLevel(Request.m_Transition, g_Config.m_QmUiMotionLevel);

	const STrackKey Key{EffectiveRequest.m_NodeKey, EffectiveRequest.m_Property};
	const float BaseValue = CurrentValueFor(Key, 0.0f);
	auto ItActive = m_ActiveTracks.find(Key);
	if(ItActive == m_ActiveTracks.end())
	{
		StartTrack(Key, EffectiveRequest, BaseValue);
		return true;
	}

	SActiveTrack &Active = ItActive->second;
	switch(EffectiveRequest.m_Transition.m_Interrupt)
	{
	case EUiAnimInterruptPolicy::REPLACE:
	{
		m_QueuedTracks.erase(Key);
		StartTrack(Key, EffectiveRequest, Active.m_Current);
		return true;
	}
	case EUiAnimInterruptPolicy::QUEUE:
	{
		m_QueuedTracks[Key].push_back(EffectiveRequest);
		return true;
	}
	case EUiAnimInterruptPolicy::KEEP_HIGHER_PRIORITY:
	{
		if(Active.m_Transition.m_Priority > EffectiveRequest.m_Transition.m_Priority)
			return false;
		m_QueuedTracks.erase(Key);
		StartTrack(Key, EffectiveRequest, Active.m_Current);
		return true;
	}
	case EUiAnimInterruptPolicy::MERGE_TARGET:
	{
		if(Active.m_Transition.m_Priority > EffectiveRequest.m_Transition.m_Priority)
			return false;
		const bool RequestIsTween = EffectiveRequest.m_Transition.m_Driver == EUiAnimDriver::TWEEN;
		if(RequestIsTween && EffectiveRequest.m_Transition.m_DurationSec <= 0.0f && EffectiveRequest.m_Transition.m_DelaySec <= 0.0f)
		{
			const uint32_t TrackId = EffectiveRequest.m_TrackId != 0 ? EffectiveRequest.m_TrackId : Active.m_TrackId;
			m_Values[Key] = EffectiveRequest.m_Target;
			m_CompletedEvents.push_back({Key.m_NodeKey, Key.m_Property, TrackId});
			m_ActiveTracks.erase(Key);
			StartQueuedTracks(Key, EffectiveRequest.m_Target);
			return true;
		}

		if(Active.m_Transition.m_Driver == EUiAnimDriver::SPRING)
		{
			Active.m_Target = EffectiveRequest.m_Target;
			Active.m_Transition.m_Priority = EffectiveRequest.m_Transition.m_Priority;
			if(!RequestIsTween)
				Active.m_Transition.m_Spring = EffectiveRequest.m_Transition.m_Spring;
			Active.m_RestTimerSec = 0.0f;
			Active.m_TrackId = EffectiveRequest.m_TrackId != 0 ? EffectiveRequest.m_TrackId : Active.m_TrackId;
			return true;
		}

		const float Progress = ApplyEasing(TrackProgress(Active), Active.m_Transition);
		const float Current = Active.m_Current;
		const float Denominator = 1.0f - Progress;
		float NewStart = Current;
		if(std::abs(Denominator) > 1e-6f)
		{
			NewStart = (Current - Progress * EffectiveRequest.m_Target) / Denominator;
		}
		Active.m_Start = NewStart;
		Active.m_Target = EffectiveRequest.m_Target;
		Active.m_Transition.m_Priority = EffectiveRequest.m_Transition.m_Priority;
		Active.m_TrackId = EffectiveRequest.m_TrackId != 0 ? EffectiveRequest.m_TrackId : Active.m_TrackId;
		return true;
	}
	}

	return false;
}

void CUiV2AnimationRuntime::AdvanceSpring(SActiveTrack &Track, float Dt) const
{
	const float Delay = std::max(0.0f, Track.m_Transition.m_DelaySec);
	if(Track.m_ElapsedSec < Delay)
		return;

	const SUiSpringConfig &Cfg = Track.m_Transition.m_Spring;
	const float Mass = std::max(Cfg.m_Mass, 1e-4f);
	const float Stiffness = std::max(Cfg.m_Stiffness, 0.0f);
	const float Damping = std::max(Cfg.m_Damping, 0.0f);

	constexpr float KFixedSubStep = 1.0f / 240.0f;
	constexpr int KMaxSubSteps = 8;
	int SubSteps = static_cast<int>(std::ceil(Dt / KFixedSubStep));
	SubSteps = std::clamp(SubSteps, 1, KMaxSubSteps);
	const float SubDt = Dt / static_cast<float>(SubSteps);

	for(int i = 0; i < SubSteps; ++i)
	{
		const float Disp = Track.m_Current - Track.m_Target;
		const float Accel = (-Stiffness * Disp - Damping * Track.m_Velocity) / Mass;
		Track.m_Velocity += Accel * SubDt;
		Track.m_Current += Track.m_Velocity * SubDt;
	}

	const bool AtRest = std::abs(Track.m_Current - Track.m_Target) < Cfg.m_RestEpsilon && std::abs(Track.m_Velocity) < Cfg.m_RestVelocity;
	if(AtRest)
		Track.m_RestTimerSec += Dt;
	else
		Track.m_RestTimerSec = 0.0f;
}

void CUiV2AnimationRuntime::Advance(float Dt)
{
	if(Dt <= 0.0f)
		return;
	const float ClampedDt = std::min(Dt, 1.0f / 15.0f);
	m_TimeSec += ClampedDt;

	constexpr float KSpringRestHoldSec = 0.033f;

	std::deque<STrackKey> vCompleted;
	for(auto &Pair : m_ActiveTracks)
	{
		const STrackKey &Key = Pair.first;
		SActiveTrack &Track = Pair.second;
		Track.m_ElapsedSec += ClampedDt;

		if(Track.m_Transition.m_Driver == EUiAnimDriver::SPRING)
		{
			AdvanceSpring(Track, ClampedDt);
			if(Track.m_RestTimerSec >= KSpringRestHoldSec)
			{
				Track.m_Current = Track.m_Target;
				Track.m_Velocity = 0.0f;
				m_Values[Key] = Track.m_Current;
				vCompleted.push_back(Key);
			}
			else
			{
				m_Values[Key] = Track.m_Current;
			}
		}
		else
		{
			const float RawProgress = TrackProgress(Track);
			const float Progress = ApplyEasing(RawProgress, Track.m_Transition);
			Track.m_Current = Track.m_Start + (Track.m_Target - Track.m_Start) * Progress;
			m_Values[Key] = Track.m_Current;

			if(RawProgress >= 1.0f)
				vCompleted.push_back(Key);
		}
	}

	for(const STrackKey &Key : vCompleted)
	{
		auto ItTrack = m_ActiveTracks.find(Key);
		if(ItTrack != m_ActiveTracks.end())
			CompleteTrack(Key, ItTrack->second);
	}
}

bool CUiV2AnimationRuntime::HasActiveAnimation(uint64_t NodeKey, EUiAnimProperty Property) const
{
	return m_ActiveTracks.contains({NodeKey, Property});
}

int CUiV2AnimationRuntime::ActiveTrackCount() const
{
	return static_cast<int>(m_ActiveTracks.size());
}

int CUiV2AnimationRuntime::QueuedTrackCount() const
{
	int Count = 0;
	for(const auto &Pair : m_QueuedTracks)
		Count += static_cast<int>(Pair.second.size());
	return Count;
}

bool CUiV2AnimationRuntime::PollCompletedEvent(SUiAnimCompleteEvent &EventOut)
{
	if(m_CompletedEvents.empty())
		return false;
	EventOut = m_CompletedEvents.front();
	m_CompletedEvents.pop_front();
	return true;
}

float CUiV2AnimationRuntime::TimeSec() const
{
	return m_TimeSec;
}

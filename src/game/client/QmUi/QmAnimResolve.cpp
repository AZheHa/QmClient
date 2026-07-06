/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "QmAnimResolve.h"

#include <game/client/ui_rect.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace
{
	constexpr float ANIM_EPSILON = 0.0001f;

	uint64_t HashAnimNode(uint64_t Value)
	{
		// Mix bits to keep generated animation keys stable and well distributed.
		Value ^= Value >> 33;
		Value *= 0xff51afd7ed558ccdULL;
		Value ^= Value >> 33;
		Value *= 0xc4ceb9fe1a85ec53ULL;
		Value ^= Value >> 33;
		return Value;
	}

	uint64_t PropertyTargetCacheKey(uint64_t NodeKey, EUiAnimProperty Property)
	{
		return HashAnimNode(NodeKey ^ (0x9e3779b97f4a7c15ULL * (static_cast<uint64_t>(Property) + 1ULL)));
	}
} // namespace

uint64_t BuildUiAnimNodeKey(const uint64_t ScopeHash, const uint64_t Id)
{
	return HashAnimNode((ScopeHash << 32) ^ HashAnimNode(Id));
}

float ResolveUiAnimValue(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, EUiAnimProperty Property, float Target, float DurationSec, EEasing Easing)
{
	struct SAnimTargetState
	{
		float m_Target = 0.0f;
		uint64_t m_LastUseCounter = 0;
	};

	static std::unordered_map<uint64_t, SAnimTargetState> s_aLastTargets;
	static uint64_t s_UseCounter = 0;
	constexpr size_t TargetCacheSoftLimit = 4096;
	constexpr uint64_t TargetCachePruneInterval = 1024;
	constexpr uint64_t TargetCacheStaleWindow = 8192;
	const uint64_t LastTargetKey = NodeKey ^ (static_cast<uint64_t>(Property) << 61);
	const float CurrentValue = AnimRuntime.GetValue(NodeKey, Property, Target);
	const uint64_t CurrentUseCounter = ++s_UseCounter;

	if(s_aLastTargets.empty())
		s_aLastTargets.reserve(TargetCacheSoftLimit);

	if((CurrentUseCounter % TargetCachePruneInterval) == 0 && s_aLastTargets.size() > TargetCacheSoftLimit)
	{
		for(auto It = s_aLastTargets.begin(); It != s_aLastTargets.end();)
		{
			if(CurrentUseCounter - It->second.m_LastUseCounter > TargetCacheStaleWindow)
				It = s_aLastTargets.erase(It);
			else
				++It;
		}
		if(s_aLastTargets.size() > TargetCacheSoftLimit * 2)
			s_aLastTargets.clear();
	}

	auto [ItLastTarget, Inserted] = s_aLastTargets.try_emplace(LastTargetKey, SAnimTargetState{Target, CurrentUseCounter});
	const bool HasLastTarget = !Inserted;
	const float LastTarget = ItLastTarget->second.m_Target;
	const bool TargetChanged = !HasLastTarget || std::abs(Target - LastTarget) > ANIM_EPSILON;
	const bool NeedsSync = !AnimRuntime.HasActiveAnimation(NodeKey, Property) && std::abs(Target - CurrentValue) > ANIM_EPSILON;
	if(TargetChanged || NeedsSync)
	{
		SUiAnimRequest Request;
		Request.m_NodeKey = NodeKey;
		Request.m_Property = Property;
		Request.m_Target = Target;
		Request.m_Transition.m_DurationSec = DurationSec;
		Request.m_Transition.m_DelaySec = 0.0f;
		Request.m_Transition.m_Priority = 1;
		Request.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
		Request.m_Transition.m_Easing = Easing;
		AnimRuntime.RequestAnimation(Request);
		ItLastTarget->second.m_Target = Target;
	}
	ItLastTarget->second.m_LastUseCounter = CurrentUseCounter;

	return AnimRuntime.GetValue(NodeKey, Property, Target);
}

CUIRect ResolveUiAnimValueRect(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, const CUIRect &Target, float DurationSec, EEasing Easing)
{
	CUIRect Out;
	Out.x = ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::POS_X, Target.x, DurationSec, Easing);
	Out.y = ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::POS_Y, Target.y, DurationSec, Easing);
	Out.w = ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::WIDTH, Target.w, DurationSec, Easing);
	Out.h = ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::HEIGHT, Target.h, DurationSec, Easing);
	return Out;
}

ColorRGBA ResolveUiAnimValueColor(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, const ColorRGBA &Target, float DurationSec, EEasing Easing)
{
	ColorRGBA Out;
	Out.r = ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::COLOR_R, Target.r, DurationSec, Easing);
	Out.g = ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::COLOR_G, Target.g, DurationSec, Easing);
	Out.b = ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::COLOR_B, Target.b, DurationSec, Easing);
	Out.a = ResolveUiAnimValue(AnimRuntime, NodeKey, EUiAnimProperty::COLOR_A, Target.a, DurationSec, Easing);
	return Out;
}

float ResolveUiPresentationStateValue(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, EUiAnimProperty Property, float Target, const SUiSpringConfig &Spring, int Priority, float Epsilon)
{
	struct SPresentationTargetState
	{
		float m_Target = 0.0f;
		uint64_t m_LastUseCounter = 0;
	};

	static std::unordered_map<uint64_t, SPresentationTargetState> s_aLastTargets;
	static uint64_t s_UseCounter = 0;
	constexpr size_t TargetCacheSoftLimit = 4096;
	constexpr uint64_t TargetCachePruneInterval = 1024;
	constexpr uint64_t TargetCacheStaleWindow = 8192;
	const uint64_t LastTargetKey = PropertyTargetCacheKey(NodeKey, Property);
	const float CurrentValue = AnimRuntime.GetValue(NodeKey, Property, Target);
	const uint64_t CurrentUseCounter = ++s_UseCounter;

	if(s_aLastTargets.empty())
		s_aLastTargets.reserve(TargetCacheSoftLimit);

	if((CurrentUseCounter % TargetCachePruneInterval) == 0 && s_aLastTargets.size() > TargetCacheSoftLimit)
	{
		for(auto It = s_aLastTargets.begin(); It != s_aLastTargets.end();)
		{
			if(CurrentUseCounter - It->second.m_LastUseCounter > TargetCacheStaleWindow)
				It = s_aLastTargets.erase(It);
			else
				++It;
		}
		if(s_aLastTargets.size() > TargetCacheSoftLimit * 2)
			s_aLastTargets.clear();
	}

	auto [ItLastTarget, Inserted] = s_aLastTargets.try_emplace(LastTargetKey, SPresentationTargetState{Target, CurrentUseCounter});
	const bool HasLastTarget = !Inserted;
	const float LastTarget = ItLastTarget->second.m_Target;
	const bool TargetChanged = !HasLastTarget || std::abs(Target - LastTarget) > std::max(Epsilon, ANIM_EPSILON);
	const bool NeedsSync = !AnimRuntime.HasActiveAnimation(NodeKey, Property) && std::abs(Target - CurrentValue) > std::max(Epsilon, ANIM_EPSILON);
	if(TargetChanged || NeedsSync)
	{
		SUiAnimRequest Request;
		Request.m_NodeKey = NodeKey;
		Request.m_Property = Property;
		Request.m_Target = Target;
		Request.m_Transition.m_Priority = Priority;
		Request.m_Transition.m_Interrupt = EUiAnimInterruptPolicy::MERGE_TARGET;
		Request.m_Transition.m_Driver = EUiAnimDriver::SPRING;
		Request.m_Transition.m_Spring = Spring;
		Request.m_Transition.m_RespectMotionLevel = false;
		AnimRuntime.RequestAnimation(Request);
		ItLastTarget->second.m_Target = Target;
	}
	ItLastTarget->second.m_LastUseCounter = CurrentUseCounter;

	return AnimRuntime.GetValue(NodeKey, Property, Target);
}

void SetUiPresentationStateValue(CUiV2AnimationRuntime &AnimRuntime, uint64_t NodeKey, EUiAnimProperty Property, float Value)
{
	AnimRuntime.SetValue(NodeKey, Property, Value);
}

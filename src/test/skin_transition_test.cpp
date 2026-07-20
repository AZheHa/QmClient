// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <game/client/render.h>

#include <gtest/gtest.h>

TEST(SkinTransition, DurationMillisecondsMapToExactSeconds)
{
	EXPECT_FLOAT_EQ(SkinChangeTransitionDurationSeconds(500), 0.5f);
	EXPECT_FLOAT_EQ(SkinChangeTransitionDurationSeconds(0), 0.0f);
}

TEST(SkinTransition, ProgressUsesConfiguredDurationAndZeroMeansDisabled)
{
	EXPECT_FLOAT_EQ(ResolveSkinChangeTransitionProgress(0.25f, 500), 0.5f);
	EXPECT_FLOAT_EQ(ResolveSkinChangeTransitionProgress(0.0f, 0), 1.0f);
	EXPECT_FLOAT_EQ(ResolveSkinChangeTransitionProgress(0.25f, 0), 1.0f);
}

TEST(SkinTransition, ProgressEndsExactlyAtConfiguredDuration)
{
	EXPECT_LT(ResolveSkinChangeTransitionProgress(0.499f, 500), 1.0f);
	EXPECT_FLOAT_EQ(ResolveSkinChangeTransitionProgress(0.5f, 500), 1.0f);
	EXPECT_FLOAT_EQ(ResolveSkinChangeTransitionProgress(0.501f, 500), 1.0f);
}

TEST(SkinTransition, AppearanceChangesOnlyAnimateWhenTheSkinChanges)
{
	EXPECT_EQ(ResolveSkinChangeTransitionAction(false, false, false), ESkinChangeTransitionAction::KEEP);
	EXPECT_EQ(ResolveSkinChangeTransitionAction(true, false, false), ESkinChangeTransitionAction::KEEP);
	EXPECT_EQ(ResolveSkinChangeTransitionAction(true, true, true), ESkinChangeTransitionAction::START);
	EXPECT_EQ(ResolveSkinChangeTransitionAction(true, false, true), ESkinChangeTransitionAction::CANCEL);
}

TEST(SkinTransition, BlendAtStartShowsCurrentSkinImmediately)
{
	const SSkinChangeTransitionBlend Blend = ComputeSkinChangeTransitionBlend(0.0f, SKIN_CHANGE_TRANSITION_GHOST_POP);
	EXPECT_FLOAT_EQ(Blend.m_PreviousAlpha, 1.0f);
	EXPECT_GT(Blend.m_CurrentAlpha, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousBodyScale.x, 1.0f);
	EXPECT_LT(Blend.m_CurrentBodyScale.x, 1.0f);
	EXPECT_LT(Blend.m_CurrentFeetScale.x, 1.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousAngleOffset, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentAngleOffset, 0.0f);
}

TEST(SkinTransition, BlendAtEndUsesOnlyCurrentSkin)
{
	const SSkinChangeTransitionBlend Blend = ComputeSkinChangeTransitionBlend(1.0f, SKIN_CHANGE_TRANSITION_GHOST_POP);
	EXPECT_FLOAT_EQ(Blend.m_PreviousAlpha, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentAlpha, 1.0f);
	EXPECT_NEAR(Blend.m_CurrentBodyScale.x, 1.0f, 0.0001f);
	EXPECT_NEAR(Blend.m_CurrentFeetScale.x, 1.0f, 0.0001f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousAngleOffset, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentAngleOffset, 0.0f);
}

TEST(SkinTransition, BlendMidpointFavorsCurrentSkinAndKeepsPop)
{
	const SSkinChangeTransitionBlend Blend = ComputeSkinChangeTransitionBlend(0.5f, SKIN_CHANGE_TRANSITION_GHOST_POP);
	EXPECT_LT(Blend.m_PreviousAlpha, 1.0f);
	EXPECT_GT(Blend.m_PreviousAlpha, 0.0f);
	EXPECT_GT(Blend.m_CurrentAlpha, 0.5f);
	EXPECT_LT(Blend.m_PreviousBodyScale.x, 1.0f);
	EXPECT_GT(Blend.m_CurrentBodyScale.x, 1.0f);
	EXPECT_GT(Blend.m_CurrentFeetScale.x, 1.0f);
}

TEST(SkinTransition, FadeScaleTypeOnlyTweaksAlphaAndScale)
{
	const SSkinChangeTransitionBlend Blend = ComputeSkinChangeTransitionBlend(0.25f, SKIN_CHANGE_TRANSITION_FADE_SCALE);
	EXPECT_GT(Blend.m_PreviousAlpha, 0.0f);
	EXPECT_GT(Blend.m_CurrentAlpha, 0.0f);
	EXPECT_LT(Blend.m_CurrentBodyScale.x, 1.0f);
	EXPECT_LT(Blend.m_CurrentFeetScale.x, 1.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousAngleOffset, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentAngleOffset, 0.0f);
}

TEST(SkinTransition, SlideLeftTypeAppliesHorizontalOffsets)
{
	const SSkinChangeTransitionBlend Blend = ComputeSkinChangeTransitionBlend(0.25f, SKIN_CHANGE_TRANSITION_SLIDE_LEFT);
	EXPECT_GT(Blend.m_PreviousAlpha, 0.0f);
	EXPECT_GT(Blend.m_CurrentAlpha, 0.0f);
	EXPECT_LT(Blend.m_PreviousPosOffset.x, 0.0f);
	EXPECT_GT(Blend.m_CurrentPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousPosOffset.y, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentPosOffset.y, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousAngleOffset, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentAngleOffset, 0.0f);
}

TEST(SkinTransition, SpinPopTypeAppliesAngleOffsets)
{
	const SSkinChangeTransitionBlend Blend = ComputeSkinChangeTransitionBlend(0.25f, SKIN_CHANGE_TRANSITION_SPIN_POP);
	EXPECT_GT(Blend.m_PreviousAlpha, 0.0f);
	EXPECT_GT(Blend.m_CurrentAlpha, 0.0f);
	EXPECT_LT(Blend.m_PreviousAngleOffset, 0.0f);
	EXPECT_GT(Blend.m_CurrentAngleOffset, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentPosOffset.x, 0.0f);
}

TEST(SkinTransition, ThemeSwitchTypeUsesVerticalMotion)
{
	const SSkinChangeTransitionBlend Blend = ComputeSkinChangeTransitionBlend(0.25f, SKIN_CHANGE_TRANSITION_THEME_SWITCH);
	EXPECT_GT(Blend.m_PreviousAlpha, 0.0f);
	EXPECT_GT(Blend.m_CurrentAlpha, 0.0f);
	EXPECT_LT(Blend.m_PreviousPosOffset.y, 0.0f);
	EXPECT_GT(Blend.m_CurrentPosOffset.y, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_PreviousPosOffset.x, 0.0f);
	EXPECT_FLOAT_EQ(Blend.m_CurrentPosOffset.x, 0.0f);
}

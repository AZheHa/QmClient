#include <base/system.h>

#include <game/client/components/chat.h>
#include <game/client/components/tclient/fast_practice.h>

#include <gtest/gtest.h>
#include <test/test.h>

#include <iterator>
#include <string>

namespace
{
	int64_t TestTicks(float Seconds)
	{
		return (int64_t)(Seconds * time_freq());
	}

	std::string SourceFunctionBody(const std::string &Source, const std::string &Signature)
	{
		const size_t FunctionStart = Source.find(Signature);
		EXPECT_NE(FunctionStart, std::string::npos) << Signature;
		const size_t BodyStart = Source.find("{", FunctionStart);
		EXPECT_NE(BodyStart, std::string::npos) << Signature;
		int Depth = 0;
		for(size_t Index = BodyStart; Index < Source.size(); ++Index)
		{
			if(Source[Index] == '{')
				++Depth;
			else if(Source[Index] == '}')
			{
				--Depth;
				if(Depth == 0)
					return Source.substr(BodyStart, Index - BodyStart);
			}
		}
		ADD_FAILURE() << Signature;
		return {};
	}
}

TEST(QmChatPresentation, NewLineEntersThenBecomesVisible)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(10.0f);

	CChat::BeginLinePresentation(Presentation, Start, false);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::ENTERING);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_LT(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetY, 0.0f);
	EXPECT_LT(Presentation.m_RenderAlpha, 1.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.31f), 0.10f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_NEAR(Presentation.m_RenderOffsetX, 0.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderOffsetY, 0.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmChatPresentation, InactiveOldLineKeepsFullOpacity)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(20.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.31f), 0.10f, false, false, 0, 0.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.05f), 0.05f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmChatPresentation, InactiveExpiredLineFadesAndCollapses)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(30.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.1f), 0.20f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::EXITING);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 0.5f, 0.001f);
	EXPECT_NEAR(Presentation.m_LayoutVisibility, 1.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderOffsetX, -12.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderOffsetY, 0.0f, 0.001f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.3f), 0.20f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::COLLAPSED);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 0.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_LayoutVisibility, 0.0f, 0.001f);
}

TEST(QmChatPresentation, DisabledExtraAnimationsUseImmediateVisibilityStates)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(35.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.01f), 0.01f, false, false, 0, 0.0f, false);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_EntryProgress, 1.0f);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 1.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.1f), 0.20f, false, false, 0, 0.0f, false);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::COLLAPSED);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 0.0f);
}

TEST(QmChatPresentation, DisabledExtraAnimationsRecallHistoryImmediately)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(38.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(30.0f), 0.10f, true, false, Start + TestTicks(30.0f), 0.2f, false);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 1.0f);
}

TEST(QmChatPresentation, ReenablingExtraAnimationsDoesNotReplaySettledStates)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(39.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.01f), 0.01f, false, false, 0, 0.0f, false);
	ASSERT_TRUE(Presentation.m_AnimationsSuppressed);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(0.02f), 0.01f, false, false, 0, 0.0f, true);
	EXPECT_FALSE(Presentation.m_AnimationsSuppressed);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 1.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.1f), 0.20f, false, false, 0, 0.0f, false);
	ASSERT_TRUE(Presentation.m_AnimationsSuppressed);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(14.11f), 0.01f, false, false, 0, 0.0f, true);
	EXPECT_TRUE(Presentation.m_AnimationsSuppressed);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::COLLAPSED);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 0.0f);
}

TEST(QmChatPresentation, ReenablingExtraAnimationsDoesNotReplayExpandedHistory)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(39.0f);
	const int64_t OpenTick = Start + TestTicks(30.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, OpenTick, 0.10f, true, false, OpenTick, 0.2f, false);
	ASSERT_TRUE(Presentation.m_AnimationsSuppressed);
	CChat::UpdateLinePresentation(Presentation, Start, OpenTick + TestTicks(0.01f), 0.01f, true, false, OpenTick, 0.2f, true);
	EXPECT_TRUE(Presentation.m_AnimationsSuppressed);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_RenderOffsetX, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 1.0f);
}

TEST(QmChatPresentation, InputKeepsOldLineOpaque)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(40.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.20f), 0.18f, true, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_NEAR(Presentation.m_LayoutVisibility, 1.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderOffsetX, 0.0f, 0.001f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmChatPresentation, ClosingInputKeepsOldLineVisible)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(50.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.70f), 0.18f, true, false, 0, 0.0f);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.72f), 0.02f, false, false, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmChatPresentation, ForceVisibleLineDoesNotAutoDecay)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(60.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);

	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(30.0f), 0.10f, false, true, 0, 0.0f);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 1.0f);
	EXPECT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);
}

TEST(QmLocalSaveJoinHint, UsesExpiringEchoMessages)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/tclient.cpp");
	const std::string Body = SourceFunctionBody(Source, "void CTClient::MaybeShowLocalSaveJoinHint()");

	EXPECT_NE(Body.find("GameClient()->Echo(aMessage);"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->Echo(PlayersLine.c_str());"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->Echo(CodesLine.c_str());"), std::string::npos);
	EXPECT_EQ(Body.find("GameClient()->Echo(aMessage, true);"), std::string::npos);
	EXPECT_EQ(Body.find("GameClient()->Echo(PlayersLine.c_str(), true);"), std::string::npos);
	EXPECT_EQ(Body.find("GameClient()->Echo(CodesLine.c_str(), true);"), std::string::npos);
}

TEST(QmChatPresentation, ResetAndTimeRollbackKeepFiniteFreshState)
{
	CChat::SPresentationState Presentation;
	const int64_t Start = TestTicks(70.0f);
	CChat::BeginLinePresentation(Presentation, Start, false);
	CChat::UpdateLinePresentation(Presentation, Start, Start + TestTicks(5.50f), 0.20f, false, false, 0, 0.0f);
	ASSERT_EQ(Presentation.m_State, CChat::EPresentationState::VISIBLE);
	ASSERT_NEAR(Presentation.m_RenderAlpha, 1.0f, 0.001f);

	CChat::ResetPresentationState(Presentation);
	EXPECT_EQ(Presentation.m_State, CChat::EPresentationState::COLLAPSED);
	EXPECT_FLOAT_EQ(Presentation.m_LayoutVisibility, 0.0f);
	EXPECT_FLOAT_EQ(Presentation.m_RenderAlpha, 0.0f);

	CChat::BeginLinePresentation(Presentation, Start, false);
	CChat::UpdateLinePresentation(Presentation, Start, Start - TestTicks(1.0f), -1.0f, false, false, 0, 0.0f);
	EXPECT_TRUE(std::isfinite(Presentation.m_RenderAlpha));
	EXPECT_TRUE(std::isfinite(Presentation.m_RenderOffsetX));
	EXPECT_TRUE(std::isfinite(Presentation.m_RenderOffsetY));
}

TEST(QmChatPresentation, SmoothYApproachesTargetWithoutOvershoot)
{
	float Y = 200.0f;
	for(int i = 0; i < 16; ++i)
	{
		const float NextY = CChat::SmoothPresentationY(Y, 120.0f, 1.0f / 60.0f);
		EXPECT_TRUE(std::isfinite(NextY));
		EXPECT_LE(NextY, Y);
		EXPECT_GE(NextY, 120.0f);
		Y = NextY;
	}

	for(int i = 0; i < 16; ++i)
	{
		const float NextY = CChat::SmoothPresentationY(Y, 180.0f, 1.0f / 30.0f);
		EXPECT_TRUE(std::isfinite(NextY));
		EXPECT_GE(NextY, Y);
		EXPECT_LE(NextY, 180.0f);
		Y = NextY;
	}
}

TEST(QmWindowModes, WindowedFullscreenRemainsARegularBorderedWindow)
{
	const std::string Backend = ReadTestSourceFile("src/engine/client/backend_sdl.cpp");
	const std::string SetWindowParams = SourceFunctionBody(Backend, "void CGraphicsBackend_SDL_GL::SetWindowParams(");
	const size_t WindowedFullscreenStart = SetWindowParams.find("else // Windowed fullscreen");
	const size_t WindowedStart = SetWindowParams.find("else // Windowed", WindowedFullscreenStart + 1);
	ASSERT_NE(WindowedFullscreenStart, std::string::npos);
	ASSERT_NE(WindowedStart, std::string::npos);
	const std::string WindowedFullscreen = SetWindowParams.substr(WindowedFullscreenStart, WindowedStart - WindowedFullscreenStart);

	EXPECT_NE(WindowedFullscreen.find("SDL_SetWindowFullscreen(m_pWindow, 0);"), std::string::npos);
	EXPECT_NE(WindowedFullscreen.find("SDL_SetWindowBordered(m_pWindow, SDL_TRUE);"), std::string::npos);
	EXPECT_NE(WindowedFullscreen.find("SDL_SetWindowResizable(m_pWindow, SDL_FALSE);"), std::string::npos);
}

TEST(QmWindowModes, StartupDoesNotMarkWindowedFullscreenAsBorderless)
{
	const std::string Backend = ReadTestSourceFile("src/engine/client/backend_sdl.cpp");
	const std::string Graphics = ReadTestSourceFile("src/engine/client/graphics_threaded.cpp");
	const std::string IssueInit = SourceFunctionBody(Graphics, "int CGraphics_Threaded::IssueInit()");

	EXPECT_EQ(IssueInit.find("else // Windowed fullscreen"), std::string::npos);
	EXPECT_NE(IssueInit.find("if(IsExclusiveFullscreen)"), std::string::npos);
	EXPECT_NE(IssueInit.find("else if(IsDesktopFullscreen)"), std::string::npos);
	EXPECT_NE(IssueInit.find("else if(IsPurelyWindowed)"), std::string::npos);
	EXPECT_NE(Backend.find("const bool IsWindowedFullscreen = g_Config.m_GfxFullscreen == 3;"), std::string::npos);
	EXPECT_NE(Backend.find("if(IsWindowedFullscreen || (IsFullscreen && !SupportedResolution)"), std::string::npos);
}

TEST(QmWindowModes, GraphicsMenuMapsAllFiveModesToDistinctBackendStates)
{
	const std::string Menus = ReadTestSourceFile("src/game/client/components/menus_settings.cpp");
	const std::string RenderSettingsGraphics = SourceFunctionBody(Menus, "void CMenus::RenderSettingsGraphics(");

	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(0, false);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(0, true);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(3, false);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(2, false);"), std::string::npos);
	EXPECT_NE(RenderSettingsGraphics.find("Graphics()->SetWindowParams(1, false);"), std::string::npos);
}

TEST(QmChatInteractions, ClampBacklogLine)
{
	EXPECT_EQ(CChat::ClampBacklogLine(-3, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(0, 10, 4), 0);
	EXPECT_EQ(CChat::ClampBacklogLine(6, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(7, 10, 4), 6);
	EXPECT_EQ(CChat::ClampBacklogLine(20, 10, 4), 6);
}

TEST(QmChatInteractions, ScrollbarValueToBacklogLine)
{
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(1.0f, 12), 0);
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(0.0f, 12), 12);
	EXPECT_EQ(CChat::ScrollbarValueToBacklogLine(0.5f, 12), 6);
}

TEST(QmChatInteractions, BacklogLineToScrollbarValue)
{
	EXPECT_FLOAT_EQ(CChat::BacklogLineToScrollbarValue(0, 12), 1.0f);
	EXPECT_FLOAT_EQ(CChat::BacklogLineToScrollbarValue(12, 12), 0.0f);
	EXPECT_FLOAT_EQ(CChat::BacklogLineToScrollbarValue(6, 12), 0.5f);
	EXPECT_FLOAT_EQ(CChat::BacklogLineToScrollbarValue(20, 12), 0.0f);
}

TEST(QmFastPracticeCommands, TeleCursorTargetMatchesPracticeCursorWorldConversion)
{
	const vec2 CharacterPos(100.0f, 200.0f);
	const vec2 Target(400.0f, 0.0f);
	const vec2 Result = CFastPractice::PracticeTeleCursorTarget(CharacterPos, Target, 2.0f, 100, 50);

	EXPECT_FLOAT_EQ(Result.x, 750.0f);
	EXPECT_FLOAT_EQ(Result.y, 200.0f);
}

TEST(QmFastPracticeCommands, TeleportDefaultsToAimingOrSpectatingPosition)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const size_t CommandBlock = Source.find("if(Cmd == \"tp\" || Cmd == \"teleport\" || Cmd == \"tc\" || Cmd == \"telecursor\")");
	ASSERT_NE(CommandBlock, std::string::npos);
	const size_t TelecursorBranch = Source.find("if(Cmd == \"tc\" || Cmd == \"telecursor\")", CommandBlock);
	ASSERT_NE(TelecursorBranch, std::string::npos);
	const std::string DefaultTargetBlock = Source.substr(CommandBlock, TelecursorBranch - CommandBlock);

	EXPECT_NE(DefaultTargetBlock.find("vec2 Target = GameClient()->m_Controls.m_aTargetPos[g_Config.m_ClDummy];"), std::string::npos);
	EXPECT_EQ(DefaultTargetBlock.find("PracticeTeleCursorTarget"), std::string::npos);
}

TEST(QmFastPracticeCommands, SpectatorCommandKeepsPracticeStateOnSnapshotMiss)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const std::string Body = SourceFunctionBody(Source, "bool CFastPractice::ConsumeSpectatorCommand()");

	EXPECT_EQ(Body.find("Disable();"), std::string::npos);
	EXPECT_NE(Body.find("m_PracticeWorldInitialized = false;"), std::string::npos);
	EXPECT_NE(Body.find("GameClient()->m_PredictedDummyId = -1;"), std::string::npos);
}

TEST(QmFastPracticeCommands, PredictionLoopsReuseNormalPreInputAndFreezeSemantics)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/tclient/fast_practice.cpp");
	const std::string OverrideBody = SourceFunctionBody(Source, "bool CFastPractice::OverridePredict()");
	const std::string VisualBody = SourceFunctionBody(Source, "int CFastPractice::ApplyVisualFastInputPrediction(");

	EXPECT_NE(OverrideBody.find("GameClient()->ApplyPreInputs(Tick, true, GameClient()->m_PredictedWorld);"), std::string::npos);
	EXPECT_NE(OverrideBody.find("GameClient()->ApplyPreInputs(Tick, false, GameClient()->m_PredictedWorld);"), std::string::npos);
	EXPECT_NE(OverrideBody.find("g_Config.m_ClPredictFreeze == 2"), std::string::npos);
	EXPECT_NE(VisualBody.find("GameClient()->ApplyPreInputs(Tick, true, VisualWorld);"), std::string::npos);
	EXPECT_NE(VisualBody.find("GameClient()->ApplyPreInputs(Tick, false, VisualWorld);"), std::string::npos);
	EXPECT_NE(VisualBody.find("VisualWorld.m_WorldConfig.m_PredictEvents = false;"), std::string::npos);
}

TEST(QmChatInteractions, ClickDragThreshold)
{
	EXPECT_TRUE(CChat::IsCopyClickDrag(vec2(10.0f, 10.0f), vec2(12.0f, 12.0f)));
	EXPECT_FALSE(CChat::IsCopyClickDrag(vec2(10.0f, 10.0f), vec2(30.0f, 10.0f)));
}

TEST(QmChatInteractions, ChatInputClipPaddingDoesNotExpandContentScrollArea)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string Body = SourceFunctionBody(Source, "void CChat::OnRender()");

	EXPECT_NE(Body.find("const float InputContentHeight = 2.25f * InputCursor.m_FontSize;"), std::string::npos);
	EXPECT_NE(Body.find("const float InputClipPaddingTop = maximum(1.0f, InputCursor.m_FontSize * 0.18f);"), std::string::npos);
	EXPECT_NE(Body.find("const float InputClipPaddingBottom = maximum(1.0f, InputCursor.m_FontSize * 0.10f);"), std::string::npos);
	EXPECT_NE(Body.find("const CUIRect InputContentRect"), std::string::npos);
	EXPECT_NE(Body.find("const CUIRect InputClippingRect"), std::string::npos);
	EXPECT_NE(Body.find("InputContentRect.y + InputClipPaddingTop - ScrollOffset"), std::string::npos);
	EXPECT_NE(Body.find("m_Input.GetCaretPosition().y - InputClipPaddingTop - ScrollOffsetChange"), std::string::npos);
	EXPECT_NE(Body.find("CaretPositionY < InputContentRect.y"), std::string::npos);
	EXPECT_NE(Body.find("InputContentRect.y + InputContentRect.h"), std::string::npos);
	EXPECT_NE(Body.find("Graphics()->ClipEnable((int)(InputClippingRect.x * XScale)"), std::string::npos);
	EXPECT_EQ(Body.find("CaretPositionY < InputClippingRect.y"), std::string::npos);
}

TEST(QmChatInteractions, LiveDirectorBlocksOnlyPauseCommand)
{
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("/pause"));
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("   /pause"));
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("/pause "));
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("/pause 1"));
	EXPECT_TRUE(CChat::ShouldBlockLiveDirectorChatCommand("/PAUSE"));

	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand(nullptr));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand(""));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand("please /pause"));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand("/paused"));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand("/team 1"));
	EXPECT_FALSE(CChat::ShouldBlockLiveDirectorChatCommand("hello"));
}

TEST(QmChatInteractions, AppendsBlockWordsWithSeparator)
{
	char aList[32] = "";

	EXPECT_TRUE(CChat::AppendBlockWordToList(aList, sizeof(aList), "spam"));
	EXPECT_STREQ(aList, "spam");

	EXPECT_TRUE(CChat::AppendBlockWordToList(aList, sizeof(aList), "eggs"));
	EXPECT_STREQ(aList, "spam;eggs");
}

TEST(QmChatInteractions, DoesNotAppendEmptyOrFullBlockWords)
{
	char aList[8] = "filled";

	EXPECT_FALSE(CChat::AppendBlockWordToList(aList, sizeof(aList), ""));
	EXPECT_STREQ(aList, "filled");

	EXPECT_FALSE(CChat::AppendBlockWordToList(aList, sizeof(aList), "x"));
	EXPECT_STREQ(aList, "filled");
}

TEST(QmChatInteractions, BuildsEscapedWhisperCommand)
{
	char aCommand[128];

	EXPECT_TRUE(CChat::BuildWhisperCommand(aCommand, sizeof(aCommand), "Name \"A\"", "hello"));
	EXPECT_STREQ(aCommand, "/w \"Name \\\"A\\\"\" hello");
}

TEST(QmChatInteractions, BuildsEscapedSpectateCommand)
{
	char aCommand[128];

	EXPECT_TRUE(CChat::BuildSpectateCommand(aCommand, sizeof(aCommand), "Name \"A\""));
	EXPECT_STREQ(aCommand, "say /spec \"Name \\\"A\\\"\"");
}

TEST(QmChatInteractions, ChatLineMenuKeepsSpectateAction)
{
	const std::string Header = ReadTestSourceFile("src/game/client/components/chat.h");
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");

	EXPECT_NE(Header.find("CButtonContainer m_SpectateButton;"), std::string::npos);
	EXPECT_NE(Header.find("void SpectateChatLine(const CChatLinePopupContext &Context);"), std::string::npos);
	EXPECT_NE(Source.find("DoEntry(&pPopupContext->m_SpectateButton, FontIcons::FONT_ICON_EYE, Localize(\"Spectate\")"), std::string::npos);
	EXPECT_NE(Source.find("GameClient()->m_Spectator.Spectate(Context.m_ClientId);"), std::string::npos);
	EXPECT_NE(Source.find("Console()->ExecuteLine(aCommand);"), std::string::npos);
}

TEST(QmChatInteractions, ReusesKnownServerMessageClassWithoutReanalysis)
{
	const auto Class = CChat::ResolveLineServerMessageClass(-1, "DDraceNetwork Version: 18.9", QmHudNotifications::EServerMessageClass::Prompt);
	EXPECT_EQ(Class, QmHudNotifications::EServerMessageClass::Prompt);
}

TEST(QmChatInteractions, FallsBackToLegacyServerMessageClassificationWhenUnknown)
{
	const auto Class = CChat::ResolveLineServerMessageClass(-1, "DDraceNetwork Version: 18.9");
	EXPECT_EQ(Class, QmHudNotifications::EServerMessageClass::BasicInfo);
}

TEST(QmChatInteractions, IgnoresKnownServerClassForNonServerMessages)
{
	const auto Class = CChat::ResolveLineServerMessageClass(3, "DDraceNetwork Version: 18.9", QmHudNotifications::EServerMessageClass::Prompt);
	EXPECT_EQ(Class, QmHudNotifications::EServerMessageClass::None);
}

TEST(QmChatInteractions, ManualVisibleTranslationCandidatesAreOnlyUntranslatedRemotePlayerLines)
{
	int aLocalIds[] = {2, 7};
	EXPECT_TRUE(CChat::IsManualVisibleTranslateCandidate(3, true, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(-1, true, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(-2, true, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(2, true, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(3, false, false, aLocalIds, std::size(aLocalIds)));
	EXPECT_FALSE(CChat::IsManualVisibleTranslateCandidate(3, true, true, aLocalIds, std::size(aLocalIds)));
}

TEST(QmChatInteractions, VisibleTranslationCollectsCandidatesBeforeStartingJobs)
{
	const std::string Source = ReadTestSourceFile("src/game/client/components/chat.cpp");
	const std::string Body = SourceFunctionBody(Source, "bool CChat::TranslateVisibleChatLines()");
	const size_t ScanLoop = Body.find("for(int i = m_BacklogCurLine; i < MAX_LINES; i++)");
	ASSERT_NE(ScanLoop, std::string::npos);
	const size_t CollectIndex = Body.find("aLineIndices[NumLineIndices++] = LineIndex;", ScanLoop);
	ASSERT_NE(CollectIndex, std::string::npos);
	const size_t TranslateLoop = Body.find("for(int i = 0; i < NumLineIndices; i++)", CollectIndex);
	ASSERT_NE(TranslateLoop, std::string::npos);

	EXPECT_EQ(Body.find("GameClient()->m_Translate.Translate", ScanLoop), TranslateLoop + Body.substr(TranslateLoop).find("GameClient()->m_Translate.Translate"));
}

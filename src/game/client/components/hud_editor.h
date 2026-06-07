/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_COMPONENTS_HUD_EDITOR_H
#define GAME_CLIENT_COMPONENTS_HUD_EDITOR_H

#include <game/client/component.h>
#include <game/client/lineinput.h>
#include <game/client/ui_rect.h>

#include <engine/graphics.h>

#include <array>
#include <vector>

enum class EHudEditorElement
{
	HudMain,
	HudPlayerState,
	GameTimer,
	PauseNotification,
	SuddenDeath,
	ScoreHud,
	WarmupTimer,
	DummyActions,
	DummyMiniMap,
	TextInfo,
	SpectatorCount,
	MovementInfo,
	JumpHint,
	MapProgressBar,
	SpectatorHud,
	LocalTime,
	LegacyMediaInfo,
	MediaIsland,
	Voting,
	Chat,
	VoiceOverlay,
	InputOverlay,
	HudNotifications,

	Count,
};

class CHudEditor : public CComponent
{
public:
	struct STransformScope
	{
		bool m_Applied = false;
		float m_ScreenX0 = 0.0f;
		float m_ScreenY0 = 0.0f;
		float m_ScreenX1 = 0.0f;
		float m_ScreenY1 = 0.0f;
		CUIRect m_TargetRect{};
		CUIRect m_VisibleRect{};
		int m_Corners = IGraphics::CORNER_ALL;
		bool m_AnchoredLeft = false;
		bool m_AnchoredRight = false;
		bool m_AnchoredTop = false;
		bool m_AnchoredBottom = false;
	};

	CHudEditor();
	int Sizeof() const override { return sizeof(*this); }

	void OnRender() override;
	void OnUpdate() override;
	void OnReset() override;
	void OnRelease() override;
	void OnStateChange(int NewState, int OldState) override;
	bool OnCursorMove(float x, float y, IInput::ECursorType CursorType) override;
	bool OnInput(const IInput::CEvent &Event) override;

	void SetActive(bool Active);
	bool IsActive() const { return m_Active; }
	void UpdateVisibleRect(EHudEditorElement Element, const CUIRect &RenderedRect);

	STransformScope PreviewTransform(EHudEditorElement Element, const CUIRect &DefaultRect, bool Scalable = true);
	STransformScope PreviewTransform(EHudEditorElement Element, const CUIRect &TransformRect, const CUIRect &VisibleRect, bool Scalable = true);
	STransformScope BeginTransform(EHudEditorElement Element, const CUIRect &DefaultRect, bool Scalable = true, bool ApplyMapScreen = true);
	STransformScope BeginTransform(EHudEditorElement Element, const CUIRect &TransformRect, const CUIRect &VisibleRect, bool Scalable = true, bool ApplyMapScreen = true);
	void EndTransform(const STransformScope &Scope);

private:
	struct SElementState
	{
		bool m_HasCustom = false;
		int m_PosXPermille = 0;
		int m_PosYPermille = 0;
		int m_ScalePercent = 100;
	};

	struct SVisibleElement
	{
		EHudEditorElement m_Element = EHudEditorElement::HudMain;
		CUIRect m_Rect{};
		float m_BaseWidth = 0.0f;
		float m_BaseHeight = 0.0f;
		float m_StateOffsetX = 0.0f;
		float m_StateOffsetY = 0.0f;
		bool m_Scalable = true;
	};

	static constexpr int ELEMENT_COUNT = static_cast<int>(EHudEditorElement::Count);
	static constexpr int POSITION_SCALE = 10000;
	static constexpr int MIN_SCALE_PERCENT = 25;
	static constexpr int MAX_SCALE_PERCENT = 400;

	bool m_Active = false;
	bool m_DirtyLayout = false;
	bool m_LayoutLoaded = false;
	int m_DraggingElement = -1;
	vec2 m_DragGrabOffset = vec2(0.0f, 0.0f);
	char m_aLayoutCache[2048] = {};
	std::array<SElementState, ELEMENT_COUNT> m_aElementStates{};
	std::vector<SVisibleElement> m_vVisibleElements;
	bool m_InteractionUiActive = false;
	bool m_JumpHintTextEditorActive = false;
	bool m_JumpHintTextEditorNeedsFocus = false;
	CLineInputBuffered<512> m_JumpHintTextInput;

	void ResetRuntimeState();
	void SyncLayoutConfig();
	void ParseLayoutConfig(const char *pConfig);
	void SaveLayoutConfig();
	void ResetLayoutConfig();
	void ClampStateToScreen(SElementState &State, float BaseWidth, float BaseHeight, float StateOffsetX, float StateOffsetY) const;
	SElementState &EnsureState(EHudEditorElement Element);
	const SElementState &State(EHudEditorElement Element) const;
	int FindHoveredVisibleElement() const;
	int FindVisibleElementIndex(EHudEditorElement Element) const;
	void UpdateInteractionUi();
	void OpenJumpHintTextEditor();
	void SaveJumpHintTextEditor();
	void CloseJumpHintTextEditor();
	bool HandleElementDoubleClick(EHudEditorElement Element);
	bool DoJumpHintTextArea(CLineInput *pLineInput, const CUIRect *pRect, float FontSize);
	void RenderJumpHintTextEditor(const CUIRect &Screen);
	bool ComputeTransformPlacement(EHudEditorElement Element, const CUIRect &TransformRect, const CUIRect &VisibleRect, bool Scalable, STransformScope &Scope, SVisibleElement *pVisible);
	static const char *ElementToken(EHudEditorElement Element);
	static int ElementFromToken(const char *pToken);
};

#endif

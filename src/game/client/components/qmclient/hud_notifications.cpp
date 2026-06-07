#include "hud_notifications.h"
#include <base/color.h>
#include <base/system.h>

#include <engine/graphics.h>
#include <engine/shared/config.h>

#include <game/client/gameclient.h>
#include <game/localization.h>

#include <algorithm>

namespace
{
	float EaseOutCubic(float t)
	{
		t = std::clamp(t, 0.0f, 1.0f);
		return 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
	}

	ColorRGBA ApplyAlpha(ColorRGBA Color, float Alpha)
	{
		Color.a *= std::clamp(Alpha, 0.0f, 1.0f);
		return Color;
	}
} // namespace

int CQmHudNotifications::BuildVisibleNotificationList(bool Preview, const SNotification *(&apVisible)[8], SNotification &PreviewNotification)
{
	int NumVisible = 0;
	if(Preview)
	{
		PreviewNotification.m_Kind = EKind::SoloEnter;
		str_copy(PreviewNotification.m_aText, Localize("你现在处于单人区域"));
		PreviewNotification.m_StartTime = time_get();
		apVisible[NumVisible++] = &PreviewNotification;
	}
	else
	{
		const int MaxVisible = QmHudNotifications::ClampVisibleCount(g_Config.m_QmHudNotificationsMaxVisible);
		for(int SourceIndex = (int)m_vNotifications.size() - 1; SourceIndex >= 0 && NumVisible < MaxVisible; --SourceIndex)
			apVisible[NumVisible++] = &m_vNotifications[SourceIndex];
	}
	return NumVisible;
}

CUIRect CQmHudNotifications::MeasureVisibleRect(const CUIRect &BaseRect, bool Preview, QmHudNotifications::EHorizontalFlow Flow)
{
	const float FontSize = (float)QmHudNotifications::ClampTextSize(g_Config.m_QmHudNotificationsTextSize);
	const float PaddingX = QmHudNotifications::PaddingX(FontSize);
	const float PaddingY = QmHudNotifications::PaddingY(FontSize);
	const float MinBoxWidth = QmHudNotifications::MinBoxWidth(FontSize);
	const float Gap = 4.0f * QmHudNotifications::SmallTextScale(FontSize);
	const float TextMaxWidth = maximum(1.0f, BaseRect.w - PaddingX * 2.0f);

	SNotification PreviewNotification;
	const SNotification *apVisible[8] = {};
	const int NumVisible = BuildVisibleNotificationList(Preview, apVisible, PreviewNotification);
	if(NumVisible == 0)
		return {BaseRect.x, BaseRect.y, 0.0f, 0.0f};

	float MaxWidth = 0.0f;
	float UsedHeight = 0.0f;
	float MaxSlideOffset = 0.0f;
	for(int i = 0; i < NumVisible; ++i)
	{
		const STextBoundingBox TextBox = TextRender()->TextBoundingBox(FontSize, apVisible[i]->m_aText, -1, TextMaxWidth);
		const float BoxW = minimum(BaseRect.w, maximum(MinBoxWidth, TextBox.m_W + PaddingX * 2.0f));
		const float BoxH = maximum(FontSize + PaddingY * 2.0f, TextBox.m_H + PaddingY * 2.0f);
		MaxWidth = maximum(MaxWidth, BoxW);
		UsedHeight += BoxH + (i + 1 < NumVisible ? Gap : 0.0f);
		if(std::clamp(g_Config.m_QmHudNotificationsAnimType, 0, 2) == 0 && QmHudNotifications::ClampAnimationMs(g_Config.m_QmHudNotificationsAnimMs) > 0)
			MaxSlideOffset = maximum(MaxSlideOffset, 32.0f);
	}

	return QmHudNotifications::NotificationVisibleRect(BaseRect, MaxWidth, UsedHeight, Flow, MaxSlideOffset);
}

void CQmHudNotifications::OnReset()
{
	m_vNotifications.clear();
	m_HasLastSolo = false;
	m_LastSolo = false;
	m_PendingCompatPrompt = QmHudNotifications::ESoloPrompt::None;
	m_PendingCompatUntil = 0;
}

void CQmHudNotifications::OnRelease()
{
	OnReset();
}

void CQmHudNotifications::OnNewSnapshot()
{
	bool Solo = false;
	if(!LocalSoloState(Solo))
	{
		m_HasLastSolo = false;
		return;
	}

	if(!m_HasLastSolo)
	{
		m_HasLastSolo = true;
		m_LastSolo = Solo;
		return;
	}

	if(Solo == m_LastSolo)
		return;

	const auto Prompt = Solo ? QmHudNotifications::ESoloPrompt::Enter : QmHudNotifications::ESoloPrompt::Leave;
	if(g_Config.m_QmHudNotificationsCompatSolo)
	{
		m_PendingCompatPrompt = Prompt;
		m_PendingCompatUntil = time_get() + time_freq() / 2;
	}
	m_LastSolo = Solo;
}

void CQmHudNotifications::OnRender()
{
	const bool Preview = GameClient()->m_HudEditor.IsActive() && m_vNotifications.empty();
	if(!Preview && m_vNotifications.empty())
		return;
	if(!Preview && Client()->State() != IClient::STATE_ONLINE && Client()->State() != IClient::STATE_DEMOPLAYBACK)
		return;

	const int HoldMs = QmHudNotifications::ClampHoldMs(g_Config.m_QmHudNotificationsHoldMs);
	const int AnimMs = QmHudNotifications::ClampAnimationMs(g_Config.m_QmHudNotificationsAnimMs);
	const int64_t Now = time_get();
	const int64_t Lifetime = (int64_t)(HoldMs + 2 * AnimMs) * time_freq() / 1000;
	while(!m_vNotifications.empty() && Now - m_vNotifications.front().m_StartTime > Lifetime)
		m_vNotifications.pop_front();
	if(!Preview && m_vNotifications.empty())
		return;

	const float Height = 300.0f;
	const float Width = Height * Graphics()->ScreenAspect();
	float SavedScreenX0 = 0.0f;
	float SavedScreenY0 = 0.0f;
	float SavedScreenX1 = 0.0f;
	float SavedScreenY1 = 0.0f;
	Graphics()->GetScreen(&SavedScreenX0, &SavedScreenY0, &SavedScreenX1, &SavedScreenY1);
	Graphics()->MapScreen(0.0f, 0.0f, Width, Height);

	CUIRect DefaultRect;
	DefaultRect.w = 172.0f;
	DefaultRect.h = 68.0f;
	DefaultRect.x = Width - DefaultRect.w - 8.0f;
	DefaultRect.y = 96.0f;

	const auto InitialPreviewScope = GameClient()->m_HudEditor.PreviewTransform(EHudEditorElement::HudNotifications, DefaultRect);
	const auto InitialFlow = QmHudNotifications::ResolveHorizontalFlow(
		InitialPreviewScope.m_AnchoredLeft,
		InitialPreviewScope.m_AnchoredRight,
		InitialPreviewScope.m_VisibleRect.x + InitialPreviewScope.m_VisibleRect.w * 0.5f,
		Width * 0.5f);
	const CUIRect InitialVisibleRect = MeasureVisibleRect(DefaultRect, Preview, InitialFlow);
	const auto PreviewScope = GameClient()->m_HudEditor.PreviewTransform(EHudEditorElement::HudNotifications, DefaultRect, InitialVisibleRect);
	const auto Flow = QmHudNotifications::ResolveHorizontalFlow(
		PreviewScope.m_AnchoredLeft,
		PreviewScope.m_AnchoredRight,
		PreviewScope.m_VisibleRect.x + PreviewScope.m_VisibleRect.w * 0.5f,
		Width * 0.5f);
	const CUIRect VisibleRect = MeasureVisibleRect(DefaultRect, Preview, Flow);
	const auto HudEditorScope = GameClient()->m_HudEditor.BeginTransform(EHudEditorElement::HudNotifications, DefaultRect, VisibleRect);
	RenderNotifications(HudEditorScope.m_TargetRect, HudEditorScope.m_VisibleRect, Preview, Flow);
	GameClient()->m_HudEditor.EndTransform(HudEditorScope);
	Graphics()->MapScreen(SavedScreenX0, SavedScreenY0, SavedScreenX1, SavedScreenY1);
}

bool CQmHudNotifications::QueueEcho(const char *pMessage, unsigned EchoColor)
{
	if(!g_Config.m_QmHudNotificationsEcho)
		return false;
	if(pMessage == nullptr || pMessage[0] == '\0')
		return false;

	// 队列里只保存已经解析好的文本和颜色，渲染阶段不再依赖后续配置状态。
	const QmHudNotifications::SEchoNotificationPayload Payload = QmHudNotifications::BuildEchoNotificationPayload(pMessage, EchoColor);
	if(Payload.m_aText[0] == '\0')
		return false;
	Queue(EKind::Echo, Payload.m_aText, Payload.m_Color, true);
	return true;
}

bool CQmHudNotifications::ShouldSuppressServerChat(const char *pMessage)
{
	return HandleServerChat(pMessage, g_Config.m_QmHudNotificationsSystem != 0, false, false);
}

bool CQmHudNotifications::ShouldConsumeHiddenServerChat(const char *pMessage, bool HideBasicInfo, bool HidePrompt)
{
	if(!HideBasicInfo && !HidePrompt)
		return false;
	if(g_Config.m_QmHudNotificationsCompatSolo && m_PendingCompatPrompt != QmHudNotifications::ESoloPrompt::None && time_get() > m_PendingCompatUntil)
		m_PendingCompatPrompt = QmHudNotifications::ESoloPrompt::None;

	const QmHudNotifications::SServerMessageAnalysis Analysis = QmHudNotifications::AnalyzeServerMessage(pMessage, m_PendingCompatPrompt);
	const QmHudNotifications::SServerMessageEntryDecision Decision = QmHudNotifications::DecideServerMessageEntry(Analysis, g_Config.m_QmHudNotificationsSystem != 0, HideBasicInfo, HidePrompt);
	if(Decision.m_ClearPendingCompatPrompt)
		m_PendingCompatPrompt = QmHudNotifications::ESoloPrompt::None;
	return Decision.m_ConsumeHiddenMessage;
}

bool CQmHudNotifications::LocalSoloState(bool &Solo) const
{
	const int LocalId = GameClient()->m_Snap.m_LocalClientId;
	if(LocalId < 0 || LocalId >= MAX_CLIENTS)
		return false;
	if(!GameClient()->m_aClients[LocalId].m_Active)
		return false;
	Solo = GameClient()->m_aClients[LocalId].m_Solo;
	return true;
}

void CQmHudNotifications::RenderNotifications(const CUIRect &BaseRect, const CUIRect &VisibleRect, bool Preview, QmHudNotifications::EHorizontalFlow Flow)
{
	const int HoldMs = QmHudNotifications::ClampHoldMs(g_Config.m_QmHudNotificationsHoldMs);
	const int AnimMs = QmHudNotifications::ClampAnimationMs(g_Config.m_QmHudNotificationsAnimMs);
	const int AnimType = std::clamp(g_Config.m_QmHudNotificationsAnimType, 0, 2);
	const float FontSize = (float)QmHudNotifications::ClampTextSize(g_Config.m_QmHudNotificationsTextSize);
	const float PaddingX = QmHudNotifications::PaddingX(FontSize);
	const float PaddingY = QmHudNotifications::PaddingY(FontSize);
	const float MinBoxWidth = QmHudNotifications::MinBoxWidth(FontSize);
	const float Gap = 4.0f * QmHudNotifications::SmallTextScale(FontSize);
	const float TextMaxWidth = maximum(1.0f, BaseRect.w - PaddingX * 2.0f);
	const int64_t Now = time_get();

	SNotification PreviewNotification;
	const SNotification *apVisible[8] = {};
	const int NumVisible = BuildVisibleNotificationList(Preview, apVisible, PreviewNotification);

	if(NumVisible == 0)
		return;

	ColorRGBA BgColor = color_cast<ColorRGBA>(ColorHSLA(g_Config.m_QmHudNotificationsBgColor, true));
	float Y = BaseRect.y;
	float UsedHeight = 0.0f;

	for(int i = 0; i < NumVisible; ++i)
	{
		const SNotification &Notification = *apVisible[i];
		const int ElapsedMs = Preview ? AnimMs : (int)((Now - Notification.m_StartTime) * 1000 / time_freq());
		float Alpha = 1.0f;
		float OffsetX = 0.0f;
		if(AnimType != 2 && AnimMs > 0)
		{
			if(ElapsedMs < AnimMs)
				Alpha = EaseOutCubic(ElapsedMs / (float)AnimMs);
			else if(ElapsedMs > AnimMs + HoldMs)
				Alpha = 1.0f - EaseOutCubic((ElapsedMs - AnimMs - HoldMs) / (float)AnimMs);
			if(AnimType == 0)
				OffsetX = (1.0f - Alpha) * 32.0f;
		}

		const STextBoundingBox TextBox = TextRender()->TextBoundingBox(FontSize, Notification.m_aText, -1, TextMaxWidth);
		const float BoxW = minimum(BaseRect.w, maximum(MinBoxWidth, TextBox.m_W + PaddingX * 2.0f));
		const float BoxH = maximum(FontSize + PaddingY * 2.0f, TextBox.m_H + PaddingY * 2.0f);
		CUIRect Box = {QmHudNotifications::NotificationBoxX(BaseRect, BoxW, Flow, OffsetX), Y, BoxW, BoxH};
		const float CornerRadius = minimum(6.0f, BoxH / 2.0f);
		Box.Draw(ApplyAlpha(BgColor, Alpha), IGraphics::CORNER_ALL, CornerRadius);

		const QmHudNotifications::ETextSource TextSource = Notification.m_Kind == EKind::Echo ? QmHudNotifications::ETextSource::Echo : QmHudNotifications::ETextSource::System;
		const unsigned EchoColor = Notification.m_HasEchoColor ? Notification.m_EchoColor : g_Config.m_ClMessageClientColor;
		const QmHudNotifications::STextColorConfig TextColorConfig = QmHudNotifications::TextColorConfig(TextSource, g_Config.m_QmHudNotificationsEchoInheritColor, g_Config.m_QmHudNotificationsTextColor, g_Config.m_QmHudNotificationsEchoTextColor, EchoColor);
		const ColorRGBA TextColor = color_cast<ColorRGBA>(ColorHSLA(TextColorConfig.m_Color, TextColorConfig.m_HasAlpha));
		TextRender()->TextColor(ApplyAlpha(TextColor, Alpha));
		TextRender()->Text(Box.x + PaddingX, Box.y + PaddingY, FontSize, Notification.m_aText, TextMaxWidth);

		Y += BoxH + Gap;
		UsedHeight += BoxH + (i + 1 < NumVisible ? Gap : 0.0f);
	}

	TextRender()->TextColor(TextRender()->DefaultTextColor());
	GameClient()->m_HudEditor.UpdateVisibleRect(EHudEditorElement::HudNotifications, VisibleRect);
}

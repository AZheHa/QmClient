#include "hud_notification_catalog.h"

#include <iterator>

namespace QmHudNotifications
{
	namespace
	{
		const SMessageMetadata s_aMessageMetadata[] = {
			{EServerMessageRoute::None, EServerMessageClass::None, EServerMessageDomain::Unknown, false, ""},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你现在会收到私聊消息"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你将不再收到私聊消息"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你现在可以看到本服所有 tee，不受距离限制"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你将不再看到本服所有 tee"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::SwapRescue, false, "本服务器未开启救援功能，而你所在的队伍也没有开启 /practice。注意：练习模式下无法获得排名。"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "未知表情。输入 /emote 查看帮助"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Status, false, "你的超时保护码已设置"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Team, false, "队伍存档已在进行中"},
		};

		const SMessageMetadata s_aDynamicMessageMetadata[] = {
			{EServerMessageRoute::None, EServerMessageClass::None, EServerMessageDomain::Unknown, false, ""},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::Team, false, "'%s' 加入了 %s 队"},
			{EServerMessageRoute::System, EServerMessageClass::Prompt, EServerMessageDomain::SwapRescue, false, "你已向 %s 发出交换请求。输入 /cancelswap 可取消"},
		};

		static_assert(std::size(s_aMessageMetadata) == static_cast<size_t>(EMessageKey::Count), "EMessageKey metadata table out of sync");
		static_assert(std::size(s_aDynamicMessageMetadata) == static_cast<size_t>(EDynamicMessageKey::Count), "EDynamicMessageKey metadata table out of sync");
	}

	const SMessageMetadata *FindMessageMetadata(EMessageKey Key)
	{
		const int Index = static_cast<int>(Key);
		if(Index < 0 || Index >= static_cast<int>(std::size(s_aMessageMetadata)))
			return nullptr;
		return &s_aMessageMetadata[Index];
	}

	const char *CanonicalMessageText(EMessageKey Key)
	{
		const auto *pMeta = FindMessageMetadata(Key);
		return pMeta != nullptr ? pMeta->m_pCanonicalText : "";
	}

	const SMessageMetadata *FindMessageMetadata(EDynamicMessageKey Key)
	{
		const int Index = static_cast<int>(Key);
		if(Index < 0 || Index >= static_cast<int>(std::size(s_aDynamicMessageMetadata)))
			return nullptr;
		return &s_aDynamicMessageMetadata[Index];
	}
} // namespace QmHudNotifications

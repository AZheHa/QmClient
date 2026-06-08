#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATIONS_HUD_NOTIFICATION_CATALOG_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_HUD_NOTIFICATIONS_HUD_NOTIFICATION_CATALOG_H

#include "hud_notification_rules.h"

namespace QmHudNotifications
{
	struct SMessageMetadata
	{
		EServerMessageRoute m_Route;
		EServerMessageClass m_Class;
		EServerMessageDomain m_Domain;
		bool m_ExcludeFromNotifications;
		const char *m_pCanonicalText;
	};

	const SMessageMetadata *FindMessageMetadata(EMessageKey Key);
	const SMessageMetadata *FindMessageMetadata(EDynamicMessageKey Key);
	const char *CanonicalMessageText(EMessageKey Key);
} // namespace QmHudNotifications

#endif

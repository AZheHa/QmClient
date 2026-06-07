#ifndef GAME_CLIENT_COMPONENTS_TCLIENT_SWAP_COUNTDOWN_MESSAGE_H
#define GAME_CLIENT_COMPONENTS_TCLIENT_SWAP_COUNTDOWN_MESSAGE_H

#include <generated/protocol.h>

enum class ESwapCountdownMessageAction
{
	None,
	Start,
	Cancel,
	Complete,
};

bool ParseSwapCountdownMessage(const char *pText, ESwapCountdownMessageAction &Action, char *pRequester, int RequesterSize);

#endif

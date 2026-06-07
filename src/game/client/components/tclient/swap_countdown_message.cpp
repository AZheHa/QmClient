#include "swap_countdown_message.h"

#include <base/math.h>
#include <base/str.h>

namespace
{
	bool ExtractRequesterName(const char *pText, const char *pMarker, char *pOut, int OutSize)
	{
		if(OutSize <= 0)
			return false;
		pOut[0] = '\0';
		if(pText == nullptr || pMarker == nullptr)
			return false;

		const char *pMarkerPos = str_find_nocase(pText, pMarker);
		if(pMarkerPos == nullptr || pMarkerPos == pText)
			return false;

		const int NameLength = minimum<int>(pMarkerPos - pText, OutSize - 1);
		str_truncate(pOut, OutSize, pText, NameLength);
		return pOut[0] != '\0';
	}
}

bool ParseSwapCountdownMessage(const char *pText, ESwapCountdownMessageAction &Action, char *pRequester, int RequesterSize)
{
	Action = ESwapCountdownMessageAction::None;
	if(RequesterSize > 0)
		pRequester[0] = '\0';

	if(pText == nullptr)
		return false;

	if(str_find_nocase(pText, "has requested to swap with you"))
	{
		Action = ESwapCountdownMessageAction::Start;
		return ExtractRequesterName(pText, " has requested to swap with you", pRequester, RequesterSize);
	}
	if(str_find_nocase(pText, "请求与你交换位置"))
	{
		Action = ESwapCountdownMessageAction::Start;
		return ExtractRequesterName(pText, " 请求与你交换位置", pRequester, RequesterSize);
	}
	if(str_find_nocase(pText, "has canceled swap with you"))
	{
		Action = ESwapCountdownMessageAction::Cancel;
		return ExtractRequesterName(pText, " has canceled swap with you", pRequester, RequesterSize);
	}
	if(str_find_nocase(pText, "已取消与你的交换"))
	{
		Action = ESwapCountdownMessageAction::Cancel;
		return ExtractRequesterName(pText, " 已取消与你的交换", pRequester, RequesterSize);
	}
	if(str_find_nocase(pText, "has swapped with") || str_find_nocase(pText, "已完成交换"))
	{
		Action = ESwapCountdownMessageAction::Complete;
		return true;
	}

	return false;
}

/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#include "qm_command_router.h"

#include "components/chat.h"
#include "components/controls.h"
#include "gameclient.h"

#include <base/system.h>

#include <engine/client.h>
#include <engine/console.h>
#include <engine/shared/config.h>

#include <generated/protocol.h>

#include <game/gamecore.h>

void CQmCommandRouter::Init(CGameClient *pGameClient)
{
	m_pGameClient = pGameClient;
}

void CQmCommandRouter::OnConsoleInit()
{
	if(m_pGameClient == nullptr)
		return;

	IConsole *pConsole = m_pGameClient->Console();
	if(pConsole == nullptr)
		return;

	pConsole->Register("+dummy_left", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyLeftCommand, "Move inactive tee left");
	pConsole->Register("+dummy_right", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyRightCommand, "Move inactive tee right");
	pConsole->Register("+dummy_jump", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyJumpCommand, "Make inactive tee jump");
	pConsole->Register("+dummy_hook", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyHookCommand, "Make inactive tee hook");
	pConsole->Register("+dummy_fire", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyFireCommand, "Make inactive tee fire");
	pConsole->Register("+dummy_weapon1", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyWeapon1Command, "Switch inactive tee to hammer");
	pConsole->Register("+dummy_weapon2", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyWeapon2Command, "Switch inactive tee to gun");
	pConsole->Register("+dummy_weapon3", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyWeapon3Command, "Switch inactive tee to shotgun");
	pConsole->Register("+dummy_weapon4", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyWeapon4Command, "Switch inactive tee to grenade");
	pConsole->Register("+dummy_weapon5", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyWeapon5Command, "Switch inactive tee to laser");
	pConsole->Register("+dummy_nextweapon", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyNextWeaponCommand, "Switch inactive tee to next weapon");
	pConsole->Register("+dummy_prevweapon", "", CFGFLAG_CLIENT, ConDummyInput, &m_DummyPrevWeaponCommand, "Switch inactive tee to previous weapon");

	pConsole->Register("dummy_say", "r[message]", CFGFLAG_CLIENT, ConDummySay, this, "Say in chat as inactive tee");
	pConsole->Register("dummy_say_team", "r[message]", CFGFLAG_CLIENT, ConDummySayTeam, this, "Say in team chat as inactive tee");
	pConsole->Register("dummy_pause", "?r[player name]", CFGFLAG_CLIENT, ConDummyPause, this, "Send /pause as inactive tee");
	pConsole->Register("dummy_spec", "?r[player name]", CFGFLAG_CLIENT, ConDummySpec, this, "Send /spec as inactive tee");
	pConsole->Register("dummy_team", "?i[team-id]", CFGFLAG_CLIENT, ConDummyTeam, this, "Send /team as inactive tee");
	pConsole->Register("dummy_lock", "?i['0'|'1']", CFGFLAG_CLIENT, ConDummyLock, this, "Send /lock as inactive tee");
	pConsole->Register("dummy_save", "?r[code]", CFGFLAG_CLIENT, ConDummySave, this, "Send /save as inactive tee");
	pConsole->Register("dummy_load", "?r[code]", CFGFLAG_CLIENT, ConDummyLoad, this, "Send /load as inactive tee");
	pConsole->Register("dummy_rescue", "", CFGFLAG_CLIENT, ConDummyRescue, this, "Send /rescue as inactive tee");
}

void CQmCommandRouter::OnDummySwap()
{
	const bool HadPassiveOverride = HasPassiveDummyOverride();
	const bool HadLegacyHeldInput = HasActiveLegacyExclusiveInput();
	m_DummyTransientInputMask = QM_DUMMY_INPUT_NONE;
	m_DummyFire = 0;

	if(m_pGameClient == nullptr)
		return;
	if(!HadPassiveOverride && !HadLegacyHeldInput)
	{
		m_pGameClient->m_QmDummyInputForceSend = false;
		return;
	}

	const int ActiveConn = g_Config.m_ClDummy;
	const int TargetConn = qm_dummy_command::InactiveConn(ActiveConn);
	const int PrevTargetConn = ActiveConn;

	ReleaseDummyInput(m_pGameClient->m_Controls.m_aInputData[PrevTargetConn]);
	m_pGameClient->m_Controls.m_aInputDirectionLeft[PrevTargetConn] = 0;
	m_pGameClient->m_Controls.m_aInputDirectionRight[PrevTargetConn] = 0;

	if(!HadPassiveOverride)
	{
		m_pGameClient->m_QmDummyInputForceSend = false;
		return;
	}

	CNetObj_PlayerInput &TargetInput = m_pGameClient->m_DummyInput;

	m_pGameClient->m_Controls.m_aInputData[TargetConn] = TargetInput;
	m_pGameClient->m_Controls.m_aInputDirectionLeft[TargetConn] = m_DummyLeft;
	m_pGameClient->m_Controls.m_aInputDirectionRight[TargetConn] = m_DummyRight;
	m_pGameClient->m_QmDummyInputForceSend = true;
}

void CQmCommandRouter::ResetDummyInputState()
{
	ClearDummyInputState();

	if(m_pGameClient == nullptr)
		return;

	IClient *pClient = m_pGameClient->Client();
	const bool DummyConnected = pClient != nullptr && pClient->DummyConnected();
	const int TargetConn = qm_dummy_command::InactiveConn(g_Config.m_ClDummy);
	CNetObj_PlayerInput *pTargetInput = &m_pGameClient->m_DummyInput;
	pTargetInput->m_Direction = 0;
	pTargetInput->m_Jump = 0;
	pTargetInput->m_Hook = 0;
	pTargetInput->m_WantedWeapon = 0;
	ReleaseDummyInput(*pTargetInput);
	PrepareDummyInput(*pTargetInput, TargetConn);

	m_pGameClient->m_Controls.m_aInputDirectionLeft[TargetConn] = 0;
	m_pGameClient->m_Controls.m_aInputDirectionRight[TargetConn] = 0;
	m_pGameClient->m_Controls.m_aInputData[TargetConn] = *pTargetInput;
	m_pGameClient->m_QmDummyInputForceSend = TargetConn != g_Config.m_ClDummy && DummyConnected;
}

EDummyInputRoute CQmCommandRouter::DummyInputRoute() const
{
	return qm_dummy_command::DummyInputRouteForLegacyMask(LegacyExclusiveInputMask());
}

SQmDummyInputOwnership CQmCommandRouter::GetLegacyExclusiveInputOwnership() const
{
	if(m_pGameClient == nullptr)
		return {};
	return qm_dummy_command::BuildDummyInputOwnership(ActiveLegacyExclusiveInputMask(), m_DummyTransientInputMask);
}

int CQmCommandRouter::LegacyExclusiveInputMask() const
{
	return GetLegacyExclusiveInputOwnership().m_CombinedMask;
}

bool CQmCommandRouter::HasActiveOrPendingLegacyExclusiveInput() const
{
	return LegacyExclusiveInputMask() != QM_DUMMY_INPUT_NONE;
}

bool CQmCommandRouter::HasActiveLegacyExclusiveInput() const
{
	return m_pGameClient != nullptr && ActiveLegacyExclusiveInputMask() != QM_DUMMY_INPUT_NONE;
}

bool CQmCommandRouter::NeedsDummyInputForceSend() const
{
	return m_pGameClient != nullptr && m_pGameClient->m_QmDummyInputForceSend;
}

bool CQmCommandRouter::HasPendingLegacyExclusiveInput() const
{
	return m_pGameClient != nullptr && m_DummyTransientInputMask != QM_DUMMY_INPUT_NONE;
}

void CQmCommandRouter::ConsumeLegacyExclusiveInputAfterSend()
{
	if(m_pGameClient == nullptr || m_DummyTransientInputMask == QM_DUMMY_INPUT_NONE)
		return;

	CNetObj_PlayerInput &Input = m_pGameClient->m_DummyInput;
	if(qm_dummy_command::HasDummyInputField(m_DummyTransientInputMask, QM_DUMMY_INPUT_WANTED_WEAPON))
		Input.m_WantedWeapon = 0;
	if(qm_dummy_command::HasDummyInputField(m_DummyTransientInputMask, QM_DUMMY_INPUT_NEXT_WEAPON))
		qm_dummy_command::UpdateInputCounter(Input.m_NextWeapon, 0);
	if(qm_dummy_command::HasDummyInputField(m_DummyTransientInputMask, QM_DUMMY_INPUT_PREV_WEAPON))
		qm_dummy_command::UpdateInputCounter(Input.m_PrevWeapon, 0);

	const int TargetConn = qm_dummy_command::InactiveConn(g_Config.m_ClDummy);
	m_pGameClient->m_Controls.m_aInputData[TargetConn] = Input;
	m_DummyTransientInputMask = QM_DUMMY_INPUT_NONE;
}

SQmDummyPassiveOverride CQmCommandRouter::GetPassiveDummyOverride() const
{
	return qm_dummy_command::BuildPassiveDummyOverride(m_DummyLeft, m_DummyRight, m_DummyJump, m_DummyHook);
}

int CQmCommandRouter::PassiveDummyInputMask() const
{
	return qm_dummy_command::PassiveDummyInputMask(GetPassiveDummyOverride());
}

bool CQmCommandRouter::HasPassiveDummyOverride() const
{
	return m_pGameClient != nullptr && qm_dummy_command::HasPassiveDummyOverride(GetPassiveDummyOverride());
}

void CQmCommandRouter::ApplyPassiveDummyOverrides(CNetObj_PlayerInput &Input) const
{
	qm_dummy_command::ApplyPassiveDummyOverride(Input, GetPassiveDummyOverride());
}

int CQmCommandRouter::ConnForTarget(EQmCommandTarget Target) const
{
	switch(Target)
	{
	case EQmCommandTarget::ACTIVE:
		return g_Config.m_ClDummy ? IClient::CONN_DUMMY : IClient::CONN_MAIN;
	case EQmCommandTarget::MAIN:
		return IClient::CONN_MAIN;
	case EQmCommandTarget::DUMMY:
		return qm_dummy_command::InactiveConn(g_Config.m_ClDummy);
	}
	return IClient::CONN_MAIN;
}

bool CQmCommandRouter::EnsureConnAvailable(int Conn, bool Verbose)
{
	if(m_pGameClient == nullptr || m_pGameClient->Client() == nullptr || m_pGameClient->Client()->State() != IClient::STATE_ONLINE)
		return false;
	if(Conn != IClient::CONN_DUMMY)
		return true;
	if(m_pGameClient->Client()->DummyConnected())
		return true;
	if(Verbose)
		ReportDummyUnavailable();
	return false;
}

void CQmCommandRouter::ReportDummyUnavailable()
{
	if(m_pGameClient == nullptr)
		return;

	const int64_t Now = time_get();
	if(m_LastDummyUnavailableLogTime != 0 && Now - m_LastDummyUnavailableLogTime < time_freq())
		return;
	m_LastDummyUnavailableLogTime = Now;
	if(m_pGameClient->Console() != nullptr)
		m_pGameClient->Console()->Print(IConsole::OUTPUT_LEVEL_STANDARD, "qm_dummy", "Dummy command ignored: dummy is not connected.");
}

int CQmCommandRouter::ActiveLegacyExclusiveInputMask() const
{
	int Mask = QM_DUMMY_INPUT_NONE;
	if(m_DummyFire != 0)
		Mask |= QM_DUMMY_INPUT_FIRE;
	return Mask;
}

void CQmCommandRouter::ClearDummyInputState()
{
	m_DummyLeft = 0;
	m_DummyRight = 0;
	m_DummyJump = 0;
	m_DummyHook = 0;
	m_DummyFire = 0;
	m_DummyTransientInputMask = QM_DUMMY_INPUT_NONE;
}

void CQmCommandRouter::AlignManualFireCounter(CNetObj_PlayerInput &Input) const
{
	if(m_pGameClient == nullptr || !g_Config.m_ClDummyHammer)
		return;

	Input.m_Fire = qm_dummy_command::AlignedLegacyFireCounter(m_pGameClient->m_HammerInput.m_Fire);
}

void CQmCommandRouter::ReleaseDummyInput(CNetObj_PlayerInput &Input) const
{
	Input.m_Direction = 0;
	Input.m_Jump = 0;
	Input.m_Hook = 0;
	Input.m_WantedWeapon = 0;
	if((Input.m_Fire & 1) != 0)
		Input.m_Fire++;
	if((Input.m_NextWeapon & 1) != 0)
		Input.m_NextWeapon++;
	if((Input.m_PrevWeapon & 1) != 0)
		Input.m_PrevWeapon++;
	Input.m_Fire &= INPUT_STATE_MASK;
	Input.m_NextWeapon &= INPUT_STATE_MASK;
	Input.m_PrevWeapon &= INPUT_STATE_MASK;
}

void CQmCommandRouter::PrepareDummyInput(CNetObj_PlayerInput &Input, int TargetConn) const
{
	if(m_pGameClient == nullptr)
		return;

	Input.m_PlayerFlags &= ~(PLAYERFLAG_CHATTING | PLAYERFLAG_IN_MENU | PLAYERFLAG_INPUT_ABSOLUTE | PLAYERFLAG_INPUT_MANUAL);
	Input.m_PlayerFlags |= PLAYERFLAG_PLAYING;

	switch(m_pGameClient->m_Controls.m_aMouseInputType[TargetConn])
	{
	case CControls::EMouseInputType::AUTOMATED:
		Input.m_PlayerFlags |= PLAYERFLAG_INPUT_ABSOLUTE;
		break;
	case CControls::EMouseInputType::ABSOLUTE:
		Input.m_PlayerFlags |= PLAYERFLAG_INPUT_ABSOLUTE | PLAYERFLAG_INPUT_MANUAL;
		break;
	case CControls::EMouseInputType::RELATIVE:
		Input.m_PlayerFlags |= PLAYERFLAG_INPUT_MANUAL;
		break;
	}

	vec2 Pos = m_pGameClient->m_Controls.m_aMousePos[TargetConn];
	if(g_Config.m_TcScaleMouseDistance && !m_pGameClient->m_Snap.m_SpecInfo.m_Active)
	{
		const int MaxDistance = g_Config.m_ClDyncam ? g_Config.m_ClDyncamMaxDistance : g_Config.m_ClMouseMaxDistance;
		if(MaxDistance > 5 && MaxDistance < 1000)
			Pos *= 1000.0f / static_cast<float>(MaxDistance);
	}

	Input.m_TargetX = static_cast<int>(Pos.x);
	Input.m_TargetY = static_cast<int>(Pos.y);
	if(Input.m_TargetX == 0 && Input.m_TargetY == 0)
		Input.m_TargetX = 1;
}

void CQmCommandRouter::ApplyDummyInput(EQmDummyInputCommand Command, int State)
{
	const int TargetConn = qm_dummy_command::InactiveConn(g_Config.m_ClDummy);
	if(!EnsureConnAvailable(TargetConn, State != 0))
	{
		ResetDummyInputState();
		return;
	}

	if(qm_dummy_command::IsPassiveDummyInputCommand(Command))
	{
		switch(Command)
		{
		case EQmDummyInputCommand::LEFT:
			m_DummyLeft = State != 0 ? 1 : 0;
			break;
		case EQmDummyInputCommand::RIGHT:
			m_DummyRight = State != 0 ? 1 : 0;
			break;
		case EQmDummyInputCommand::JUMP:
			m_DummyJump = State != 0 ? 1 : 0;
			break;
		case EQmDummyInputCommand::HOOK:
			m_DummyHook = State != 0 ? 1 : 0;
			break;
		default:
			break;
		}

		m_pGameClient->m_Controls.m_aInputDirectionLeft[TargetConn] = m_DummyLeft;
		m_pGameClient->m_Controls.m_aInputDirectionRight[TargetConn] = m_DummyRight;
		m_pGameClient->m_QmDummyInputForceSend = true;
		return;
	}

	CNetObj_PlayerInput &TargetInput = m_pGameClient->m_DummyInput;
	PrepareDummyInput(TargetInput, TargetConn);
	bool ForceSend = true;

	switch(Command)
	{
	case EQmDummyInputCommand::LEFT:
	case EQmDummyInputCommand::RIGHT:
	case EQmDummyInputCommand::JUMP:
	case EQmDummyInputCommand::HOOK:
		break;
	case EQmDummyInputCommand::FIRE:
	{
		const bool WasFireActive = m_DummyFire != 0;
		const bool FireActive = State != 0;
		if(qm_dummy_command::ShouldAlignLegacyFireCounter(WasFireActive, FireActive, m_DummyTransientInputMask))
			AlignManualFireCounter(TargetInput);
		m_DummyFire = FireActive ? 1 : 0;
		qm_dummy_command::UpdateInputCounter(TargetInput.m_Fire, State);
		if(WasFireActive && !FireActive)
			m_DummyTransientInputMask |= QM_DUMMY_INPUT_FIRE;
		break;
	}
	case EQmDummyInputCommand::WEAPON1:
	case EQmDummyInputCommand::WEAPON2:
	case EQmDummyInputCommand::WEAPON3:
	case EQmDummyInputCommand::WEAPON4:
	case EQmDummyInputCommand::WEAPON5:
		if(State != 0)
		{
			TargetInput.m_WantedWeapon = static_cast<int>(Command) - static_cast<int>(EQmDummyInputCommand::WEAPON1) + 1;
			m_DummyTransientInputMask |= qm_dummy_command::DummyInputFieldForCommand(Command);
		}
		else
			ForceSend = false;
		break;
	case EQmDummyInputCommand::NEXT_WEAPON:
		if(State != 0)
		{
			qm_dummy_command::UpdateInputCounter(TargetInput.m_NextWeapon, 1);
			TargetInput.m_WantedWeapon = 0;
			m_DummyTransientInputMask |= qm_dummy_command::DummyInputFieldForCommand(Command);
		}
		else
			ForceSend = false;
		break;
	case EQmDummyInputCommand::PREV_WEAPON:
		if(State != 0)
		{
			qm_dummy_command::UpdateInputCounter(TargetInput.m_PrevWeapon, 1);
			TargetInput.m_WantedWeapon = 0;
			m_DummyTransientInputMask |= qm_dummy_command::DummyInputFieldForCommand(Command);
		}
		else
			ForceSend = false;
		break;
	}

	m_pGameClient->m_Controls.m_aInputData[TargetConn] = TargetInput;
	m_pGameClient->m_Controls.m_aInputDirectionLeft[TargetConn] = m_DummyLeft;
	m_pGameClient->m_Controls.m_aInputDirectionRight[TargetConn] = m_DummyRight;
	if(ForceSend)
		m_pGameClient->m_QmDummyInputForceSend = true;
}

void CQmCommandRouter::SendDummyChat(int Team, const char *pLine)
{
	if(pLine == nullptr)
		return;

	const int Conn = ConnForTarget(EQmCommandTarget::DUMMY);
	if(!EnsureConnAvailable(Conn, true))
		return;
	m_pGameClient->m_Chat.SendChatOnConn(Conn, Team, pLine);
}

void CQmCommandRouter::SendDummySlashCommand(const char *pCommand, const char *pArgs)
{
	char aLine[512];
	qm_dummy_command::BuildSlashCommand(aLine, sizeof(aLine), pCommand, pArgs);
	SendDummyChat(0, aLine);
}

void CQmCommandRouter::ConDummyInput(IConsole::IResult *pResult, void *pUserData)
{
	SQmDummyCommand *pCommand = static_cast<SQmDummyCommand *>(pUserData);
	if(pCommand == nullptr || pCommand->m_pRouter == nullptr)
		return;
	pCommand->m_pRouter->ApplyDummyInput(pCommand->m_Command, pResult->GetInteger(0));
}

void CQmCommandRouter::ConDummySay(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	pRouter->SendDummyChat(0, pResult->GetString(0));
}

void CQmCommandRouter::ConDummySayTeam(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	pRouter->SendDummyChat(1, pResult->GetString(0));
}

void CQmCommandRouter::ConDummyPause(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	pRouter->SendDummySlashCommand("pause", pResult->NumArguments() > 0 ? pResult->GetString(0) : "");
}

void CQmCommandRouter::ConDummySpec(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	pRouter->SendDummySlashCommand("spec", pResult->NumArguments() > 0 ? pResult->GetString(0) : "");
}

void CQmCommandRouter::ConDummyTeam(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	char aArgs[16] = "";
	if(pResult->NumArguments() > 0)
		str_format(aArgs, sizeof(aArgs), "%d", pResult->GetInteger(0));
	pRouter->SendDummySlashCommand("team", aArgs);
}

void CQmCommandRouter::ConDummyLock(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	char aArgs[16] = "";
	if(pResult->NumArguments() > 0)
		str_format(aArgs, sizeof(aArgs), "%d", pResult->GetInteger(0));
	pRouter->SendDummySlashCommand("lock", aArgs);
}

void CQmCommandRouter::ConDummySave(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	pRouter->SendDummySlashCommand("save", pResult->NumArguments() > 0 ? pResult->GetString(0) : "");
}

void CQmCommandRouter::ConDummyLoad(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	pRouter->SendDummySlashCommand("load", pResult->NumArguments() > 0 ? pResult->GetString(0) : "");
}

void CQmCommandRouter::ConDummyRescue(IConsole::IResult *pResult, void *pUserData)
{
	CQmCommandRouter *pRouter = static_cast<CQmCommandRouter *>(pUserData);
	if(pRouter == nullptr)
		return;
	pRouter->SendDummySlashCommand("rescue", "");
}

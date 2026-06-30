/* (c) Magnus Auvinen. See licence.txt in the root of the distribution for more information. */
/* If you are missing that file, acquire a complete release at teeworlds.com.                */
#ifndef GAME_CLIENT_QM_COMMAND_ROUTER_H
#define GAME_CLIENT_QM_COMMAND_ROUTER_H

#include <base/system.h>

#include <engine/console.h>

#include <generated/protocol.h>

#include <cstdint>

class CGameClient;

enum class EQmCommandTarget
{
	ACTIVE = 0,
	MAIN,
	DUMMY,
};

enum class EQmDummyInputCommand
{
	LEFT = 0,
	RIGHT,
	JUMP,
	HOOK,
	FIRE,
	WEAPON1,
	WEAPON2,
	WEAPON3,
	WEAPON4,
	WEAPON5,
	NEXT_WEAPON,
	PREV_WEAPON,
};

enum class EDummyInputRoute
{
	OFFICIAL_WITH_PASSIVE_OVERLAY,
	LEGACY_EXCLUSIVE,
};

enum EQmDummyInputField
{
	QM_DUMMY_INPUT_NONE = 0,
	QM_DUMMY_INPUT_DIRECTION = 1 << 0,
	QM_DUMMY_INPUT_JUMP = 1 << 1,
	QM_DUMMY_INPUT_HOOK = 1 << 2,
	QM_DUMMY_INPUT_FIRE = 1 << 3,
	QM_DUMMY_INPUT_WANTED_WEAPON = 1 << 4,
	QM_DUMMY_INPUT_NEXT_WEAPON = 1 << 5,
	QM_DUMMY_INPUT_PREV_WEAPON = 1 << 6,
};

struct SQmDummyInputOwnership
{
	int m_HeldMask = QM_DUMMY_INPUT_NONE;
	int m_TransientMask = QM_DUMMY_INPUT_NONE;
	int m_CombinedMask = QM_DUMMY_INPUT_NONE;
};

struct SQmDummyPassiveOverride
{
	bool m_DirectionActive = false;
	int m_Direction = 0;
	bool m_JumpActive = false;
	int m_Jump = 0;
	bool m_HookActive = false;
	int m_Hook = 0;
};

struct SQmDummyLegacyState
{
	int m_DummyFire = 0;
	int m_TransientInputMask = QM_DUMMY_INPUT_NONE;
	bool m_ForceSend = false;
};

struct SQmDummyCommand
{
	class CQmCommandRouter *m_pRouter = nullptr;
	EQmDummyInputCommand m_Command = EQmDummyInputCommand::LEFT;
};

namespace qm_dummy_command
{
	inline void UpdateInputCounter(int &Value, int State)
	{
		if((Value & 1) != (State != 0 ? 1 : 0))
			Value++;
		Value &= INPUT_STATE_MASK;
	}

	inline bool HasHeldInput(const CNetObj_PlayerInput &Input)
	{
		return Input.m_Direction != 0 ||
		       Input.m_Jump != 0 ||
		       Input.m_Hook != 0 ||
		       (Input.m_Fire & 1) != 0 ||
		       (Input.m_NextWeapon & 1) != 0 ||
		       (Input.m_PrevWeapon & 1) != 0;
	}

	inline int InactiveConn(int ActiveConn)
	{
		return ActiveConn ^ 1;
	}

	inline SQmDummyPassiveOverride BuildPassiveDummyOverride(int Left, int Right, int Jump, int Hook)
	{
		SQmDummyPassiveOverride Override;
		if(Left != 0 || Right != 0)
		{
			Override.m_DirectionActive = true;
			Override.m_Direction = Right - Left;
		}
		if(Jump != 0)
		{
			Override.m_JumpActive = true;
			Override.m_Jump = 1;
		}
		if(Hook != 0)
		{
			Override.m_HookActive = true;
			Override.m_Hook = 1;
		}
		return Override;
	}

	inline int PassiveDummyInputMask(const SQmDummyPassiveOverride &Override)
	{
		int Mask = QM_DUMMY_INPUT_NONE;
		if(Override.m_DirectionActive)
			Mask |= QM_DUMMY_INPUT_DIRECTION;
		if(Override.m_JumpActive)
			Mask |= QM_DUMMY_INPUT_JUMP;
		if(Override.m_HookActive)
			Mask |= QM_DUMMY_INPUT_HOOK;
		return Mask;
	}

	inline bool HasPassiveDummyOverride(const SQmDummyPassiveOverride &Override)
	{
		return PassiveDummyInputMask(Override) != QM_DUMMY_INPUT_NONE;
	}

	inline bool IsPassiveDummyInputCommand(EQmDummyInputCommand Command)
	{
		return Command == EQmDummyInputCommand::LEFT ||
		       Command == EQmDummyInputCommand::RIGHT ||
		       Command == EQmDummyInputCommand::JUMP ||
		       Command == EQmDummyInputCommand::HOOK;
	}

	inline void ApplyPassiveDummyOverride(CNetObj_PlayerInput &Input, const SQmDummyPassiveOverride &Override, bool ApplyDirection = true)
	{
		if(ApplyDirection && Override.m_DirectionActive)
			Input.m_Direction = Override.m_Direction;
		if(Override.m_JumpActive)
			Input.m_Jump = Override.m_Jump;
		if(Override.m_HookActive)
			Input.m_Hook = Override.m_Hook;
	}

	inline SQmDummyInputOwnership BuildDummyInputOwnership(int HeldMask, int TransientMask)
	{
		return {HeldMask, TransientMask, HeldMask | TransientMask};
	}

	inline bool HasDummyInputField(int OwnershipMask, int FieldMask)
	{
		return (OwnershipMask & FieldMask) != 0;
	}

	inline int DummyInputFieldForCommand(EQmDummyInputCommand Command)
	{
		switch(Command)
		{
		case EQmDummyInputCommand::LEFT:
		case EQmDummyInputCommand::RIGHT:
			return QM_DUMMY_INPUT_DIRECTION;
		case EQmDummyInputCommand::JUMP:
			return QM_DUMMY_INPUT_JUMP;
		case EQmDummyInputCommand::HOOK:
			return QM_DUMMY_INPUT_HOOK;
		case EQmDummyInputCommand::FIRE:
			return QM_DUMMY_INPUT_FIRE;
		case EQmDummyInputCommand::WEAPON1:
		case EQmDummyInputCommand::WEAPON2:
		case EQmDummyInputCommand::WEAPON3:
		case EQmDummyInputCommand::WEAPON4:
		case EQmDummyInputCommand::WEAPON5:
			return QM_DUMMY_INPUT_WANTED_WEAPON;
		case EQmDummyInputCommand::NEXT_WEAPON:
			return QM_DUMMY_INPUT_NEXT_WEAPON;
		case EQmDummyInputCommand::PREV_WEAPON:
			return QM_DUMMY_INPUT_PREV_WEAPON;
		}
		return QM_DUMMY_INPUT_NONE;
	}

	inline bool IsTransientDummyInputCommand(EQmDummyInputCommand Command)
	{
		return Command == EQmDummyInputCommand::WEAPON1 ||
		       Command == EQmDummyInputCommand::WEAPON2 ||
		       Command == EQmDummyInputCommand::WEAPON3 ||
		       Command == EQmDummyInputCommand::WEAPON4 ||
		       Command == EQmDummyInputCommand::WEAPON5 ||
		       Command == EQmDummyInputCommand::NEXT_WEAPON ||
		       Command == EQmDummyInputCommand::PREV_WEAPON;
	}

	inline int AlignedLegacyFireCounter(int HammerFire)
	{
		return ((HammerFire + 1) & ~1) & INPUT_STATE_MASK;
	}

	inline void AlignLegacyFireWithHammer(CNetObj_PlayerInput &Input, int HammerFire, bool DummyHammer)
	{
		if(DummyHammer)
			Input.m_Fire = HammerFire & INPUT_STATE_MASK;
	}

	inline void PrepareLegacyFirePress(CNetObj_PlayerInput &Input, int HammerFire, bool DummyHammer)
	{
		if(DummyHammer)
			Input.m_Fire = AlignedLegacyFireCounter(HammerFire);
	}

	inline void SyncHammerFireAfterLegacyInput(CNetObj_PlayerInput &HammerInput, const CNetObj_PlayerInput &LegacyInput, bool DummyHammer)
	{
		if(DummyHammer)
			HammerInput.m_Fire = LegacyInput.m_Fire & INPUT_STATE_MASK;
	}

	inline bool ShouldPrepareLegacyFirePress(bool WasFireActive, bool FireActive, int PendingMask)
	{
		return !WasFireActive && FireActive && !HasDummyInputField(PendingMask, QM_DUMMY_INPUT_FIRE);
	}

	inline void ReleaseDummyInput(CNetObj_PlayerInput &Input)
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

	inline SQmDummyLegacyState ClearLegacyInputImmediately(CNetObj_PlayerInput &Input)
	{
		ReleaseDummyInput(Input);
		return {};
	}

	inline SQmDummyLegacyState CancelRuntimeLegacyInputAndFlushRelease(int DummyFire, int TransientInputMask, bool HadPassiveOverride, CNetObj_PlayerInput &Input)
	{
		const bool HadHeldFire = DummyFire != 0;
		const bool HadPendingFire = HasDummyInputField(TransientInputMask, QM_DUMMY_INPUT_FIRE);
		if(HadHeldFire)
			UpdateInputCounter(Input.m_Fire, 0);

		SQmDummyLegacyState State;
		if(HadHeldFire || HadPendingFire)
			State.m_TransientInputMask = QM_DUMMY_INPUT_FIRE;
		State.m_ForceSend = HadPassiveOverride || State.m_TransientInputMask != QM_DUMMY_INPUT_NONE;
		return State;
	}

	inline EDummyInputRoute DummyInputRouteForLegacyMask(int LegacyMask)
	{
		return LegacyMask != QM_DUMMY_INPUT_NONE ? EDummyInputRoute::LEGACY_EXCLUSIVE : EDummyInputRoute::OFFICIAL_WITH_PASSIVE_OVERLAY;
	}

	inline bool ReleaseOfficialDummyHammerInput(CNetObj_PlayerInput &DummyInput, const CNetObj_PlayerInput &HammerInput, unsigned int &DummyFire)
	{
		if(DummyFire == 0)
			return false;

		DummyInput.m_Fire = (HammerInput.m_Fire + 1) & ~1;
		DummyFire = 0;
		return true;
	}

	inline bool ShouldSendOfficialDirectDummyInput(bool Force, bool ForceSend, bool HasPassiveOverride, bool ReleasedOfficialHammer, const CNetObj_PlayerInput &Input)
	{
		return Force || ForceSend || HasPassiveOverride || ReleasedOfficialHammer || HasHeldInput(Input);
	}

	inline bool AdvanceOfficialDummyHammerCadence(unsigned int &DummyFire)
	{
		if(DummyFire % 25 != 0)
		{
			DummyFire++;
			return false;
		}
		DummyFire++;
		return true;
	}

	inline void PrepareOfficialDummyHammerInput(CNetObj_PlayerInput &HammerInput)
	{
		HammerInput.m_Fire = (HammerInput.m_Fire + 1) | 1;
		HammerInput.m_WantedWeapon = WEAPON_HAMMER + 1;
	}

	inline void BuildSlashCommand(char *pBuf, int BufSize, const char *pCommand, const char *pArgs)
	{
		if(pBuf == nullptr || BufSize <= 0)
			return;

		if(pCommand == nullptr || pCommand[0] == '\0')
		{
			pBuf[0] = '\0';
			return;
		}

		if(pArgs != nullptr && pArgs[0] != '\0')
			str_format(pBuf, BufSize, "/%s %s", pCommand, pArgs);
		else
			str_format(pBuf, BufSize, "/%s", pCommand);
	}
} // namespace qm_dummy_command

class CQmCommandRouter
{
public:
	void Init(CGameClient *pGameClient);
	void OnConsoleInit();
	void OnDummySwap();
	void ClearDummyInputStateImmediately();
	void CancelRuntimeDummyInputAndFlushRelease();
	EDummyInputRoute DummyInputRoute() const;
	SQmDummyInputOwnership GetLegacyExclusiveInputOwnership() const;
	int LegacyExclusiveInputMask() const;
	bool HasActiveOrPendingLegacyExclusiveInput() const;
	bool HasActiveLegacyExclusiveInput() const;
	bool NeedsDummyInputForceSend() const;
	bool HasPendingLegacyExclusiveInput() const;
	void ConsumeLegacyExclusiveInputAfterSend();
	SQmDummyPassiveOverride GetPassiveDummyOverride() const;
	int PassiveDummyInputMask() const;
	bool HasPassiveDummyOverride() const;
	void ApplyPassiveDummyOverrides(CNetObj_PlayerInput &Input, bool ApplyDirection = true) const;

private:
	static void ConDummyInput(IConsole::IResult *pResult, void *pUserData);
	static void ConDummySay(IConsole::IResult *pResult, void *pUserData);
	static void ConDummySayTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyPause(IConsole::IResult *pResult, void *pUserData);
	static void ConDummySpec(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyTeam(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyLock(IConsole::IResult *pResult, void *pUserData);
	static void ConDummySave(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyLoad(IConsole::IResult *pResult, void *pUserData);
	static void ConDummyRescue(IConsole::IResult *pResult, void *pUserData);

	int ConnForTarget(EQmCommandTarget Target) const;
	bool EnsureConnAvailable(int Conn, bool Verbose);
	void ReportDummyUnavailable();
	int ActiveLegacyExclusiveInputMask() const;
	void ClearDummyInputState();
	void AlignLegacyFireWithHammer(CNetObj_PlayerInput &Input) const;
	void PrepareLegacyFirePress(CNetObj_PlayerInput &Input) const;
	void ReleaseDummyInput(CNetObj_PlayerInput &Input) const;
	void PrepareDummyInput(CNetObj_PlayerInput &Input, int TargetConn) const;
	void ApplyDummyInput(EQmDummyInputCommand Command, int State);
	void SendDummyChat(int Team, const char *pLine);
	void SendDummySlashCommand(const char *pCommand, const char *pArgs);

	CGameClient *m_pGameClient = nullptr;
	int m_DummyLeft = 0;
	int m_DummyRight = 0;
	int m_DummyJump = 0;
	int m_DummyHook = 0;
	int m_DummyFire = 0;
	int m_DummyTransientInputMask = QM_DUMMY_INPUT_NONE;
	int64_t m_LastDummyUnavailableLogTime = 0;

	SQmDummyCommand m_DummyLeftCommand{this, EQmDummyInputCommand::LEFT};
	SQmDummyCommand m_DummyRightCommand{this, EQmDummyInputCommand::RIGHT};
	SQmDummyCommand m_DummyJumpCommand{this, EQmDummyInputCommand::JUMP};
	SQmDummyCommand m_DummyHookCommand{this, EQmDummyInputCommand::HOOK};
	SQmDummyCommand m_DummyFireCommand{this, EQmDummyInputCommand::FIRE};
	SQmDummyCommand m_DummyWeapon1Command{this, EQmDummyInputCommand::WEAPON1};
	SQmDummyCommand m_DummyWeapon2Command{this, EQmDummyInputCommand::WEAPON2};
	SQmDummyCommand m_DummyWeapon3Command{this, EQmDummyInputCommand::WEAPON3};
	SQmDummyCommand m_DummyWeapon4Command{this, EQmDummyInputCommand::WEAPON4};
	SQmDummyCommand m_DummyWeapon5Command{this, EQmDummyInputCommand::WEAPON5};
	SQmDummyCommand m_DummyNextWeaponCommand{this, EQmDummyInputCommand::NEXT_WEAPON};
	SQmDummyCommand m_DummyPrevWeaponCommand{this, EQmDummyInputCommand::PREV_WEAPON};
};

#endif

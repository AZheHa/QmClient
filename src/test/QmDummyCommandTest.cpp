#include "test.h"

#include <engine/client.h>

#include <game/client/qm_command_router.h>
#include <game/gamecore.h>

#include <gtest/gtest.h>

TEST(QmDummyCommand, BuildsSlashCommands)
{
	char aLine[64];
	qm_dummy_command::BuildSlashCommand(aLine, sizeof(aLine), "save", "abc123");
	EXPECT_STREQ(aLine, "/save abc123");

	qm_dummy_command::BuildSlashCommand(aLine, sizeof(aLine), "rescue", "");
	EXPECT_STREQ(aLine, "/rescue");

	qm_dummy_command::BuildSlashCommand(aLine, sizeof(aLine), nullptr, "ignored");
	EXPECT_STREQ(aLine, "");
}

TEST(QmDummyCommand, UpdatesInputCounterLikeDDNetControls)
{
	int Value = 0;
	qm_dummy_command::UpdateInputCounter(Value, 1);
	EXPECT_EQ(Value, 1);

	qm_dummy_command::UpdateInputCounter(Value, 1);
	EXPECT_EQ(Value, 1);

	qm_dummy_command::UpdateInputCounter(Value, 0);
	EXPECT_EQ(Value, 2);

	qm_dummy_command::UpdateInputCounter(Value, 0);
	EXPECT_EQ(Value, 2);
}

TEST(QmDummyCommand, LegacyFireReleaseDoesNotRealignToHammerCounter)
{
	int DirectFire = qm_dummy_command::AlignedLegacyFireCounter(1);
	EXPECT_EQ(DirectFire, 2);

	qm_dummy_command::UpdateInputCounter(DirectFire, 1);
	EXPECT_EQ(DirectFire, 3);

	qm_dummy_command::UpdateInputCounter(DirectFire, 0);
	EXPECT_EQ(DirectFire, 4);
}

TEST(QmDummyCommand, LegacyFirePressDuringPendingReleaseKeepsDirectCounter)
{
	int DirectFire = qm_dummy_command::AlignedLegacyFireCounter(1);
	qm_dummy_command::UpdateInputCounter(DirectFire, 1);
	qm_dummy_command::UpdateInputCounter(DirectFire, 0);
	EXPECT_EQ(DirectFire, 4);

	EXPECT_FALSE(qm_dummy_command::ShouldPrepareLegacyFirePress(false, true, QM_DUMMY_INPUT_FIRE));
	qm_dummy_command::UpdateInputCounter(DirectFire, 1);
	EXPECT_EQ(DirectFire, 5);
}

TEST(QmDummyCommand, LegacyWeaponAlignsFireToHammerWithoutPress)
{
	CNetObj_PlayerInput DirectInput = {};
	DirectInput.m_Fire = 0;

	qm_dummy_command::AlignLegacyFireWithHammer(DirectInput, 1, true);
	DirectInput.m_WantedWeapon = 1;

	EXPECT_EQ(DirectInput.m_Fire, 1);
	EXPECT_EQ(DirectInput.m_WantedWeapon, 1);
}

TEST(QmDummyCommand, LegacyNextWeaponAlignsFireToCurrentHammerCounter)
{
	CNetObj_PlayerInput DirectInput = {};
	DirectInput.m_Fire = 2;

	qm_dummy_command::AlignLegacyFireWithHammer(DirectInput, 7, true);
	qm_dummy_command::UpdateInputCounter(DirectInput.m_NextWeapon, 1);

	EXPECT_EQ(DirectInput.m_Fire, 7);
	EXPECT_EQ(DirectInput.m_NextWeapon, 1);
}

TEST(QmDummyCommand, LegacyFireReleaseSyncsHammerBeforeNextHammerPress)
{
	CNetObj_PlayerInput DirectInput = {};
	qm_dummy_command::PrepareLegacyFirePress(DirectInput, 1, true);
	qm_dummy_command::UpdateInputCounter(DirectInput.m_Fire, 1);
	EXPECT_EQ(DirectInput.m_Fire, 3);

	qm_dummy_command::UpdateInputCounter(DirectInput.m_Fire, 0);
	EXPECT_EQ(DirectInput.m_Fire, 4);

	CNetObj_PlayerInput HammerInput = {};
	HammerInput.m_Fire = 1;
	qm_dummy_command::SyncHammerFireAfterLegacyInput(HammerInput, DirectInput, true);
	EXPECT_EQ(HammerInput.m_Fire, 4);

	HammerInput.m_Fire = (HammerInput.m_Fire + 1) | 1;
	EXPECT_EQ(HammerInput.m_Fire, 5);
}

TEST(QmDummyCommand, LegacyWeaponSendDoesNotChangeHammerFire)
{
	CNetObj_PlayerInput HammerInput = {};
	HammerInput.m_Fire = 9;
	CNetObj_PlayerInput DirectInput = {};

	qm_dummy_command::AlignLegacyFireWithHammer(DirectInput, HammerInput.m_Fire, true);
	DirectInput.m_WantedWeapon = 1;
	qm_dummy_command::SyncHammerFireAfterLegacyInput(HammerInput, DirectInput, true);

	EXPECT_EQ(DirectInput.m_Fire, 9);
	EXPECT_EQ(HammerInput.m_Fire, 9);
}

TEST(QmDummyCommand, RuntimeCancelFlushesHeldLegacyFireRelease)
{
	CNetObj_PlayerInput DirectInput = {};
	DirectInput.m_Fire = 3;

	const SQmDummyLegacyState State = qm_dummy_command::CancelRuntimeLegacyInputAndFlushRelease(
		1,
		QM_DUMMY_INPUT_NONE,
		false,
		DirectInput);

	EXPECT_EQ(DirectInput.m_Fire, 4);
	EXPECT_EQ(State.m_DummyFire, 0);
	EXPECT_EQ(State.m_TransientInputMask, QM_DUMMY_INPUT_FIRE);
	EXPECT_TRUE(State.m_ForceSend);
	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(State.m_TransientInputMask), EDummyInputRoute::LEGACY_EXCLUSIVE);

	const SQmDummyInputOwnership AfterReleaseSend = qm_dummy_command::BuildDummyInputOwnership(State.m_DummyFire, QM_DUMMY_INPUT_NONE);
	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(AfterReleaseSend.m_CombinedMask), EDummyInputRoute::OFFICIAL_WITH_PASSIVE_OVERLAY);
}

TEST(QmDummyCommand, RuntimeCancelKeepsPendingLegacyFireRelease)
{
	CNetObj_PlayerInput DirectInput = {};
	DirectInput.m_Fire = 4;

	const SQmDummyLegacyState State = qm_dummy_command::CancelRuntimeLegacyInputAndFlushRelease(
		0,
		QM_DUMMY_INPUT_FIRE | QM_DUMMY_INPUT_WANTED_WEAPON,
		false,
		DirectInput);

	EXPECT_EQ(DirectInput.m_Fire, 4);
	EXPECT_EQ(State.m_DummyFire, 0);
	EXPECT_EQ(State.m_TransientInputMask, QM_DUMMY_INPUT_FIRE);
	EXPECT_TRUE(State.m_ForceSend);
}

TEST(QmDummyCommand, RuntimeCancelPassiveOnlyForcesOfficialClear)
{
	CNetObj_PlayerInput DirectInput = {};
	DirectInput.m_Fire = 17;

	const SQmDummyLegacyState State = qm_dummy_command::CancelRuntimeLegacyInputAndFlushRelease(
		0,
		QM_DUMMY_INPUT_NONE,
		true,
		DirectInput);

	EXPECT_EQ(State.m_DummyFire, 0);
	EXPECT_EQ(State.m_TransientInputMask, QM_DUMMY_INPUT_NONE);
	EXPECT_TRUE(State.m_ForceSend);
	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(State.m_TransientInputMask), EDummyInputRoute::OFFICIAL_WITH_PASSIVE_OVERLAY);
}

TEST(QmDummyCommand, ImmediateClearDoesNotRequestReleaseFlush)
{
	CNetObj_PlayerInput DirectInput = {};
	DirectInput.m_Direction = 1;
	DirectInput.m_Jump = 1;
	DirectInput.m_Hook = 1;
	DirectInput.m_WantedWeapon = 2;
	DirectInput.m_Fire = 3;
	DirectInput.m_NextWeapon = 5;
	DirectInput.m_PrevWeapon = 7;

	const SQmDummyLegacyState State = qm_dummy_command::ClearLegacyInputImmediately(DirectInput);

	EXPECT_EQ(State.m_DummyFire, 0);
	EXPECT_EQ(State.m_TransientInputMask, QM_DUMMY_INPUT_NONE);
	EXPECT_FALSE(State.m_ForceSend);
	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(State.m_TransientInputMask), EDummyInputRoute::OFFICIAL_WITH_PASSIVE_OVERLAY);
	EXPECT_EQ(DirectInput.m_Direction, 0);
	EXPECT_EQ(DirectInput.m_Jump, 0);
	EXPECT_EQ(DirectInput.m_Hook, 0);
	EXPECT_EQ(DirectInput.m_WantedWeapon, 0);
	EXPECT_EQ(DirectInput.m_Fire, 4);
	EXPECT_EQ(DirectInput.m_NextWeapon, 6);
	EXPECT_EQ(DirectInput.m_PrevWeapon, 8);
}

TEST(QmDummyCommand, HeldInputIgnoresOneShotWeaponSwitch)
{
	CNetObj_PlayerInput Input = {};
	EXPECT_FALSE(qm_dummy_command::HasHeldInput(Input));

	Input.m_WantedWeapon = 1;
	EXPECT_FALSE(qm_dummy_command::HasHeldInput(Input));

	Input.m_Fire = 1;
	EXPECT_TRUE(qm_dummy_command::HasHeldInput(Input));

	Input.m_Fire = 2;
	Input.m_NextWeapon = 1;
	EXPECT_TRUE(qm_dummy_command::HasHeldInput(Input));
}

TEST(QmDummyCommand, InactiveConnIsRelativeToActiveConn)
{
	EXPECT_EQ(qm_dummy_command::InactiveConn(IClient::CONN_MAIN), IClient::CONN_DUMMY);
	EXPECT_EQ(qm_dummy_command::InactiveConn(IClient::CONN_DUMMY), IClient::CONN_MAIN);
}

static CNetObj_PlayerInput QmDummyCommandBaseInput()
{
	CNetObj_PlayerInput Input = {};
	Input.m_Direction = 1;
	Input.m_TargetX = 100;
	Input.m_TargetY = 200;
	Input.m_Jump = 0;
	Input.m_Fire = 17;
	Input.m_Hook = 0;
	Input.m_PlayerFlags = PLAYERFLAG_PLAYING | PLAYERFLAG_INPUT_MANUAL;
	Input.m_WantedWeapon = 2;
	Input.m_NextWeapon = 4;
	Input.m_PrevWeapon = 6;
	return Input;
}

TEST(QmDummyCommand, MapsDummyCommandsToInputFields)
{
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::LEFT), QM_DUMMY_INPUT_DIRECTION);
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::RIGHT), QM_DUMMY_INPUT_DIRECTION);
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::JUMP), QM_DUMMY_INPUT_JUMP);
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::HOOK), QM_DUMMY_INPUT_HOOK);
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::FIRE), QM_DUMMY_INPUT_FIRE);
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::WEAPON1), QM_DUMMY_INPUT_WANTED_WEAPON);
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::WEAPON5), QM_DUMMY_INPUT_WANTED_WEAPON);
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::NEXT_WEAPON), QM_DUMMY_INPUT_NEXT_WEAPON);
	EXPECT_EQ(qm_dummy_command::DummyInputFieldForCommand(EQmDummyInputCommand::PREV_WEAPON), QM_DUMMY_INPUT_PREV_WEAPON);
}

TEST(QmDummyCommand, BuildsPassiveOverrideOnlyForDirectionJumpHook)
{
	const SQmDummyPassiveOverride Override = qm_dummy_command::BuildPassiveDummyOverride(1, 0, 1, 1);

	EXPECT_TRUE(Override.m_DirectionActive);
	EXPECT_EQ(Override.m_Direction, -1);
	EXPECT_TRUE(Override.m_JumpActive);
	EXPECT_EQ(Override.m_Jump, 1);
	EXPECT_TRUE(Override.m_HookActive);
	EXPECT_EQ(Override.m_Hook, 1);
	EXPECT_EQ(qm_dummy_command::PassiveDummyInputMask(Override), QM_DUMMY_INPUT_DIRECTION | QM_DUMMY_INPUT_JUMP | QM_DUMMY_INPUT_HOOK);
	EXPECT_FALSE(qm_dummy_command::HasDummyInputField(qm_dummy_command::PassiveDummyInputMask(Override), QM_DUMMY_INPUT_FIRE));
	EXPECT_FALSE(qm_dummy_command::HasDummyInputField(qm_dummy_command::PassiveDummyInputMask(Override), QM_DUMMY_INPUT_WANTED_WEAPON));
	EXPECT_FALSE(qm_dummy_command::HasDummyInputField(qm_dummy_command::PassiveDummyInputMask(Override), QM_DUMMY_INPUT_NEXT_WEAPON));
	EXPECT_FALSE(qm_dummy_command::HasDummyInputField(qm_dummy_command::PassiveDummyInputMask(Override), QM_DUMMY_INPUT_PREV_WEAPON));
}

TEST(QmDummyCommand, PassiveDirectionOverridesOnlyDirection)
{
	const CNetObj_PlayerInput BaseInput = QmDummyCommandBaseInput();
	CNetObj_PlayerInput FinalInput = BaseInput;
	const SQmDummyPassiveOverride Override = qm_dummy_command::BuildPassiveDummyOverride(1, 0, 0, 0);

	qm_dummy_command::ApplyPassiveDummyOverride(FinalInput, Override);

	EXPECT_EQ(FinalInput.m_Direction, -1);
	EXPECT_EQ(FinalInput.m_Jump, BaseInput.m_Jump);
	EXPECT_EQ(FinalInput.m_Hook, BaseInput.m_Hook);
	EXPECT_EQ(FinalInput.m_Fire, BaseInput.m_Fire);
	EXPECT_EQ(FinalInput.m_WantedWeapon, BaseInput.m_WantedWeapon);
	EXPECT_EQ(FinalInput.m_NextWeapon, BaseInput.m_NextWeapon);
	EXPECT_EQ(FinalInput.m_PrevWeapon, BaseInput.m_PrevWeapon);
	EXPECT_EQ(FinalInput.m_TargetX, BaseInput.m_TargetX);
	EXPECT_EQ(FinalInput.m_TargetY, BaseInput.m_TargetY);
	EXPECT_EQ(FinalInput.m_PlayerFlags, BaseInput.m_PlayerFlags);
}

TEST(QmDummyCommand, PassiveJumpOverridesOnlyJump)
{
	const CNetObj_PlayerInput BaseInput = QmDummyCommandBaseInput();
	CNetObj_PlayerInput FinalInput = BaseInput;
	const SQmDummyPassiveOverride Override = qm_dummy_command::BuildPassiveDummyOverride(0, 0, 1, 0);

	qm_dummy_command::ApplyPassiveDummyOverride(FinalInput, Override);

	EXPECT_EQ(FinalInput.m_Jump, 1);
	EXPECT_EQ(FinalInput.m_Direction, BaseInput.m_Direction);
	EXPECT_EQ(FinalInput.m_Hook, BaseInput.m_Hook);
	EXPECT_EQ(FinalInput.m_Fire, BaseInput.m_Fire);
	EXPECT_EQ(FinalInput.m_WantedWeapon, BaseInput.m_WantedWeapon);
	EXPECT_EQ(FinalInput.m_TargetX, BaseInput.m_TargetX);
	EXPECT_EQ(FinalInput.m_TargetY, BaseInput.m_TargetY);
}

TEST(QmDummyCommand, PassiveHookOverridesOnlyHook)
{
	const CNetObj_PlayerInput BaseInput = QmDummyCommandBaseInput();
	CNetObj_PlayerInput FinalInput = BaseInput;
	const SQmDummyPassiveOverride Override = qm_dummy_command::BuildPassiveDummyOverride(0, 0, 0, 1);

	qm_dummy_command::ApplyPassiveDummyOverride(FinalInput, Override);

	EXPECT_EQ(FinalInput.m_Hook, 1);
	EXPECT_EQ(FinalInput.m_Direction, BaseInput.m_Direction);
	EXPECT_EQ(FinalInput.m_Jump, BaseInput.m_Jump);
	EXPECT_EQ(FinalInput.m_Fire, BaseInput.m_Fire);
	EXPECT_EQ(FinalInput.m_WantedWeapon, BaseInput.m_WantedWeapon);
	EXPECT_EQ(FinalInput.m_TargetX, BaseInput.m_TargetX);
	EXPECT_EQ(FinalInput.m_TargetY, BaseInput.m_TargetY);
}

TEST(QmDummyCommand, InactivePassiveOverrideKeepsBaseInput)
{
	const CNetObj_PlayerInput BaseInput = QmDummyCommandBaseInput();
	CNetObj_PlayerInput FinalInput = BaseInput;
	const SQmDummyPassiveOverride Override = qm_dummy_command::BuildPassiveDummyOverride(0, 0, 0, 0);

	qm_dummy_command::ApplyPassiveDummyOverride(FinalInput, Override);

	EXPECT_EQ(FinalInput.m_Direction, BaseInput.m_Direction);
	EXPECT_EQ(FinalInput.m_Jump, BaseInput.m_Jump);
	EXPECT_EQ(FinalInput.m_Hook, BaseInput.m_Hook);
	EXPECT_EQ(FinalInput.m_Fire, BaseInput.m_Fire);
	EXPECT_EQ(FinalInput.m_WantedWeapon, BaseInput.m_WantedWeapon);
	EXPECT_EQ(FinalInput.m_NextWeapon, BaseInput.m_NextWeapon);
	EXPECT_EQ(FinalInput.m_PrevWeapon, BaseInput.m_PrevWeapon);
	EXPECT_EQ(FinalInput.m_TargetX, BaseInput.m_TargetX);
	EXPECT_EQ(FinalInput.m_TargetY, BaseInput.m_TargetY);
	EXPECT_EQ(FinalInput.m_PlayerFlags, BaseInput.m_PlayerFlags);
}

TEST(QmDummyCommand, PassiveOverridePreservesHammerFireCounter)
{
	const CNetObj_PlayerInput HammerInput = QmDummyCommandBaseInput();
	CNetObj_PlayerInput FinalInput = HammerInput;
	const SQmDummyPassiveOverride Override = qm_dummy_command::BuildPassiveDummyOverride(1, 0, 1, 1);

	qm_dummy_command::ApplyPassiveDummyOverride(FinalInput, Override);

	EXPECT_EQ(FinalInput.m_Direction, -1);
	EXPECT_EQ(FinalInput.m_Jump, 1);
	EXPECT_EQ(FinalInput.m_Hook, 1);
	EXPECT_EQ(FinalInput.m_Fire, HammerInput.m_Fire);
	EXPECT_EQ(FinalInput.m_WantedWeapon, HammerInput.m_WantedWeapon);
	EXPECT_EQ(FinalInput.m_NextWeapon, HammerInput.m_NextWeapon);
	EXPECT_EQ(FinalInput.m_PrevWeapon, HammerInput.m_PrevWeapon);
	EXPECT_EQ(FinalInput.m_TargetX, HammerInput.m_TargetX);
	EXPECT_EQ(FinalInput.m_TargetY, HammerInput.m_TargetY);
}

TEST(QmDummyCommand, PassiveOverlayPreservesOfficialHammerFields)
{
	CNetObj_PlayerInput HammerInput = {};
	HammerInput.m_Fire = 5;
	HammerInput.m_WantedWeapon = WEAPON_HAMMER + 1;
	HammerInput.m_TargetX = 320;
	HammerInput.m_TargetY = -160;
	HammerInput.m_NextWeapon = 8;
	HammerInput.m_PrevWeapon = 10;

	CNetObj_PlayerInput FinalInput = HammerInput;
	const SQmDummyPassiveOverride Override = qm_dummy_command::BuildPassiveDummyOverride(1, 0, 1, 1);
	qm_dummy_command::ApplyPassiveDummyOverride(FinalInput, Override);

	EXPECT_EQ(FinalInput.m_Direction, -1);
	EXPECT_EQ(FinalInput.m_Jump, 1);
	EXPECT_EQ(FinalInput.m_Hook, 1);
	EXPECT_EQ(FinalInput.m_Fire, HammerInput.m_Fire);
	EXPECT_EQ(FinalInput.m_WantedWeapon, HammerInput.m_WantedWeapon);
	EXPECT_EQ(FinalInput.m_NextWeapon, HammerInput.m_NextWeapon);
	EXPECT_EQ(FinalInput.m_PrevWeapon, HammerInput.m_PrevWeapon);
	EXPECT_EQ(FinalInput.m_TargetX, HammerInput.m_TargetX);
	EXPECT_EQ(FinalInput.m_TargetY, HammerInput.m_TargetY);
}

TEST(QmDummyCommand, DistinguishesHeldAndTransientDummyCommands)
{
	EXPECT_FALSE(qm_dummy_command::IsTransientDummyInputCommand(EQmDummyInputCommand::LEFT));
	EXPECT_FALSE(qm_dummy_command::IsTransientDummyInputCommand(EQmDummyInputCommand::FIRE));
	EXPECT_TRUE(qm_dummy_command::IsTransientDummyInputCommand(EQmDummyInputCommand::WEAPON1));
	EXPECT_TRUE(qm_dummy_command::IsTransientDummyInputCommand(EQmDummyInputCommand::WEAPON5));
	EXPECT_TRUE(qm_dummy_command::IsTransientDummyInputCommand(EQmDummyInputCommand::NEXT_WEAPON));
	EXPECT_TRUE(qm_dummy_command::IsTransientDummyInputCommand(EQmDummyInputCommand::PREV_WEAPON));
}

TEST(QmDummyCommand, RoutesPassiveOnlyInputThroughOfficialOverlay)
{
	const SQmDummyPassiveOverride Override = qm_dummy_command::BuildPassiveDummyOverride(1, 0, 1, 1);

	EXPECT_TRUE(qm_dummy_command::HasPassiveDummyOverride(Override));
	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(QM_DUMMY_INPUT_NONE), EDummyInputRoute::OFFICIAL_WITH_PASSIVE_OVERLAY);
}

TEST(QmDummyCommand, RoutesHeldFireThroughLegacyExclusive)
{
	const SQmDummyInputOwnership Ownership = qm_dummy_command::BuildDummyInputOwnership(QM_DUMMY_INPUT_FIRE, QM_DUMMY_INPUT_NONE);

	EXPECT_EQ(Ownership.m_CombinedMask, QM_DUMMY_INPUT_FIRE);
	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(Ownership.m_CombinedMask), EDummyInputRoute::LEGACY_EXCLUSIVE);
}

TEST(QmDummyCommand, KeepsFireReleasePendingInLegacyExclusive)
{
	const SQmDummyInputOwnership Ownership = qm_dummy_command::BuildDummyInputOwnership(QM_DUMMY_INPUT_NONE, QM_DUMMY_INPUT_FIRE);

	EXPECT_EQ(Ownership.m_HeldMask, QM_DUMMY_INPUT_NONE);
	EXPECT_EQ(Ownership.m_TransientMask, QM_DUMMY_INPUT_FIRE);
	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(Ownership.m_CombinedMask), EDummyInputRoute::LEGACY_EXCLUSIVE);
}

TEST(QmDummyCommand, KeepsWeaponRequestPendingInLegacyExclusive)
{
	const SQmDummyInputOwnership PendingWeapon = qm_dummy_command::BuildDummyInputOwnership(QM_DUMMY_INPUT_NONE, QM_DUMMY_INPUT_WANTED_WEAPON);
	const SQmDummyInputOwnership SubmittedWeapon = qm_dummy_command::BuildDummyInputOwnership(QM_DUMMY_INPUT_NONE, QM_DUMMY_INPUT_NONE);

	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(PendingWeapon.m_CombinedMask), EDummyInputRoute::LEGACY_EXCLUSIVE);
	EXPECT_EQ(qm_dummy_command::DummyInputRouteForLegacyMask(SubmittedWeapon.m_CombinedMask), EDummyInputRoute::OFFICIAL_WITH_PASSIVE_OVERLAY);
}

TEST(QmDummyCommand, BuildsLegacyExclusiveOwnershipMask)
{
	const SQmDummyInputOwnership Ownership = qm_dummy_command::BuildDummyInputOwnership(
		QM_DUMMY_INPUT_FIRE,
		QM_DUMMY_INPUT_WANTED_WEAPON | QM_DUMMY_INPUT_NEXT_WEAPON);

	EXPECT_EQ(Ownership.m_HeldMask, QM_DUMMY_INPUT_FIRE);
	EXPECT_EQ(Ownership.m_TransientMask, QM_DUMMY_INPUT_WANTED_WEAPON | QM_DUMMY_INPUT_NEXT_WEAPON);
	EXPECT_EQ(Ownership.m_CombinedMask, QM_DUMMY_INPUT_FIRE | QM_DUMMY_INPUT_WANTED_WEAPON | QM_DUMMY_INPUT_NEXT_WEAPON);
	EXPECT_TRUE(qm_dummy_command::HasDummyInputField(Ownership.m_CombinedMask, QM_DUMMY_INPUT_FIRE));
	EXPECT_TRUE(qm_dummy_command::HasDummyInputField(Ownership.m_CombinedMask, QM_DUMMY_INPUT_WANTED_WEAPON));
	EXPECT_TRUE(qm_dummy_command::HasDummyInputField(Ownership.m_CombinedMask, QM_DUMMY_INPUT_NEXT_WEAPON));
	EXPECT_FALSE(qm_dummy_command::HasDummyInputField(Ownership.m_CombinedMask, QM_DUMMY_INPUT_DIRECTION));
	EXPECT_FALSE(qm_dummy_command::HasDummyInputField(Ownership.m_CombinedMask, QM_DUMMY_INPUT_HOOK));
}

TEST(QmDummyCommand, HammerCadenceIgnoresForceSendBeforeOfficialTick)
{
	unsigned int DummyFire = 7;
	CNetObj_PlayerInput HammerInput = {};
	HammerInput.m_Fire = 5;
	const bool QmForceSend = true;

	EXPECT_TRUE(QmForceSend);
	EXPECT_FALSE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
	EXPECT_EQ(DummyFire, 8u);
	EXPECT_EQ(HammerInput.m_Fire, 5);
}

TEST(QmDummyCommand, FirstHammerTickProducesSinglePress)
{
	unsigned int DummyFire = 0;
	CNetObj_PlayerInput HammerInput = {};

	EXPECT_TRUE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
	qm_dummy_command::PrepareOfficialDummyHammerInput(HammerInput);
	EXPECT_EQ(DummyFire, 1u);
	EXPECT_EQ(HammerInput.m_Fire, 1);

	CInputCount Counts = CountInput(0, HammerInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 1);
	EXPECT_EQ(Counts.m_Releases, 0);

	for(int i = 0; i < 24; i++)
	{
		EXPECT_FALSE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
		EXPECT_EQ(HammerInput.m_Fire, 1);
	}
	EXPECT_EQ(DummyFire, 25u);
}

TEST(QmDummyCommand, HammerExitConvertsOddFireToDirectRelease)
{
	unsigned int DummyFire = 8;
	CNetObj_PlayerInput DummyInput = {};
	CNetObj_PlayerInput HammerInput = {};
	HammerInput.m_Fire = 5;

	EXPECT_TRUE(qm_dummy_command::ReleaseOfficialDummyHammerInput(DummyInput, HammerInput, DummyFire));

	EXPECT_EQ(DummyInput.m_Fire, 6);
	EXPECT_EQ(DummyFire, 0u);
	const CInputCount Counts = CountInput(5, DummyInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 0);
	EXPECT_EQ(Counts.m_Releases, 1);
}

TEST(QmDummyCommand, HammerRestartContinuesAfterDirectRelease)
{
	unsigned int DummyFire = 8;
	CNetObj_PlayerInput DummyInput = {};
	CNetObj_PlayerInput HammerInput = {};
	HammerInput.m_Fire = 5;

	EXPECT_TRUE(qm_dummy_command::ReleaseOfficialDummyHammerInput(DummyInput, HammerInput, DummyFire));
	EXPECT_EQ(DummyInput.m_Fire, 6);
	EXPECT_EQ(DummyFire, 0u);

	EXPECT_TRUE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
	qm_dummy_command::PrepareOfficialDummyHammerInput(HammerInput);
	EXPECT_EQ(HammerInput.m_Fire, 7);

	CInputCount Counts = CountInput(5, DummyInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 0);
	EXPECT_EQ(Counts.m_Releases, 1);
	Counts = CountInput(DummyInput.m_Fire, HammerInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 1);
	EXPECT_EQ(Counts.m_Releases, 0);
}

TEST(QmDummyCommand, OfficialDirectRouteSendsHammerReleaseWithoutForce)
{
	unsigned int DummyFire = 8;
	CNetObj_PlayerInput DummyInput = {};
	CNetObj_PlayerInput HammerInput = {};
	HammerInput.m_Fire = 5;

	const bool ReleasedOfficialHammer = qm_dummy_command::ReleaseOfficialDummyHammerInput(DummyInput, HammerInput, DummyFire);

	EXPECT_TRUE(ReleasedOfficialHammer);
	EXPECT_EQ(DummyInput.m_Fire, 6);
	EXPECT_FALSE(qm_dummy_command::HasHeldInput(DummyInput));
	EXPECT_TRUE(qm_dummy_command::ShouldSendOfficialDirectDummyInput(false, false, false, ReleasedOfficialHammer, DummyInput));
}

TEST(QmDummyCommand, DeepflyBindSequenceKeepsOfficialHammerCadence)
{
	unsigned int DummyFire = 0;
	CNetObj_PlayerInput DummyInput = {};
	CNetObj_PlayerInput HammerInput = {};
	int LastSentFire = 0;

	EXPECT_TRUE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
	qm_dummy_command::PrepareOfficialDummyHammerInput(HammerInput);
	CInputCount Counts = CountInput(LastSentFire, HammerInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 1);
	EXPECT_EQ(Counts.m_Releases, 0);
	LastSentFire = HammerInput.m_Fire;

	for(int i = 0; i < 24; i++)
	{
		EXPECT_FALSE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
		EXPECT_EQ(HammerInput.m_Fire, LastSentFire);
	}

	EXPECT_TRUE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
	qm_dummy_command::PrepareOfficialDummyHammerInput(HammerInput);
	Counts = CountInput(LastSentFire, HammerInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 1);
	LastSentFire = HammerInput.m_Fire;

	EXPECT_TRUE(qm_dummy_command::ReleaseOfficialDummyHammerInput(DummyInput, HammerInput, DummyFire));
	EXPECT_EQ(DummyInput.m_Fire, 4);
	EXPECT_EQ(DummyFire, 0u);
	Counts = CountInput(LastSentFire, DummyInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 0);
	EXPECT_EQ(Counts.m_Releases, 1);
	LastSentFire = DummyInput.m_Fire;

	EXPECT_TRUE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
	qm_dummy_command::PrepareOfficialDummyHammerInput(HammerInput);
	EXPECT_EQ(HammerInput.m_Fire, 5);
	Counts = CountInput(LastSentFire, HammerInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 1);
	EXPECT_EQ(Counts.m_Releases, 0);
	LastSentFire = HammerInput.m_Fire;

	for(int i = 0; i < 24; i++)
	{
		EXPECT_FALSE(qm_dummy_command::AdvanceOfficialDummyHammerCadence(DummyFire));
		EXPECT_EQ(HammerInput.m_Fire, LastSentFire);
	}

	EXPECT_TRUE(qm_dummy_command::ReleaseOfficialDummyHammerInput(DummyInput, HammerInput, DummyFire));
	EXPECT_EQ(DummyInput.m_Fire, 6);
	EXPECT_EQ(DummyFire, 0u);
	Counts = CountInput(LastSentFire, DummyInput.m_Fire);
	EXPECT_EQ(Counts.m_Presses, 0);
	EXPECT_EQ(Counts.m_Releases, 1);
}

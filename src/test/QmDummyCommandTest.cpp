#include "test.h"

#include <engine/client.h>

#include <game/client/qm_command_router.h>

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

TEST(QmDummyCommand, DummyHammerKeepsOfficialPriorityWithoutHeldManualInput)
{
	EXPECT_TRUE(qm_dummy_command::ShouldSendDirectDummyInput(false, false));
	EXPECT_TRUE(qm_dummy_command::ShouldSendDirectDummyInput(false, true));
	EXPECT_FALSE(qm_dummy_command::ShouldSendDirectDummyInput(true, false));
	EXPECT_TRUE(qm_dummy_command::ShouldSendDirectDummyInput(true, true));
}

TEST(QmDummyCommand, DummyHammerIgnoresForceSendForAutomaticInputProtection)
{
	EXPECT_FALSE(qm_dummy_command::ForceSendBlocksAutomaticDummyInput(false, false));
	EXPECT_TRUE(qm_dummy_command::ForceSendBlocksAutomaticDummyInput(false, true));
	EXPECT_FALSE(qm_dummy_command::ForceSendBlocksAutomaticDummyInput(true, false));
	EXPECT_FALSE(qm_dummy_command::ForceSendBlocksAutomaticDummyInput(true, true));
}

// 请抬头享受阳光｜日子很好 我很我---------致咩子
#include <base/system.h>

#include <game/version.h>

#include <gtest/gtest.h>

TEST(GitRevision, ExistsOrNull)
{
	if(GIT_SHORTREV_HASH)
	{
		EXPECT_STRNE(GIT_SHORTREV_HASH, "");
	}
}

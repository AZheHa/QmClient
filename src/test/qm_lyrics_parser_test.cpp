#include <game/client/components/qmclient/lyrics/lyric_parser.h>

#include <algorithm>

#include <gtest/gtest.h>

namespace
{

bool CurrentLineText(const std::vector<CLyricLine> &vLines, int64_t PositionMs, std::string &Out)
{
	auto It = std::upper_bound(vLines.begin(), vLines.end(), PositionMs, [](int64_t Time, const CLyricLine &Line) {
		return Time < Line.m_TimeMs;
	});
	if(It == vLines.begin())
		return false;
	--It;
	Out = It->m_Text;
	return !Out.empty();
}

bool NextLineText(const std::vector<CLyricLine> &vLines, int64_t PositionMs, std::string &Out)
{
	auto It = std::upper_bound(vLines.begin(), vLines.end(), PositionMs, [](int64_t Time, const CLyricLine &Line) {
		return Time < Line.m_TimeMs;
	});
	if(It == vLines.end())
		return false;
	Out = It->m_Text;
	return !Out.empty();
}

} // namespace

TEST(QmLyricsParser, LrcParsesMultipleTimestampsAndSkipsEmptyLines)
{
	std::vector<CLyricLine> vLines;
	char aErr[128] = {};
	ASSERT_TRUE(QmLyrics::ParseLrcLyrics("[00:01.00][00:02.50]Hello\n[00:03.00]\n[00:04.00]World", vLines, aErr, sizeof(aErr))) << aErr;
	ASSERT_EQ(vLines.size(), 3u);
	EXPECT_EQ(vLines[0].m_TimeMs, 1000);
	EXPECT_EQ(vLines[1].m_TimeMs, 2500);
	EXPECT_EQ(vLines[2].m_TimeMs, 4000);
	EXPECT_EQ(vLines[0].m_Text, "Hello");
	EXPECT_EQ(vLines[2].m_Text, "World");
}

TEST(QmLyricsParser, LastLineHasNoNextLine)
{
	std::vector<CLyricLine> vLines;
	char aErr[128] = {};
	ASSERT_TRUE(QmLyrics::ParseLrcLyrics("[00:01.00]One\n[00:02.00]Two", vLines, aErr, sizeof(aErr))) << aErr;
	std::string Text;
	EXPECT_TRUE(CurrentLineText(vLines, 2500, Text));
	EXPECT_EQ(Text, "Two");
	EXPECT_FALSE(NextLineText(vLines, 2500, Text));
}

TEST(QmLyricsParser, QrcParsesSyllablesPunctuationAndSpaces)
{
	std::vector<CLyricLine> vLines;
	char aErr[128] = {};
	ASSERT_TRUE(QmLyrics::ParseQrcLyrics("[13181,5234]This (13181,474)town (13655,275)is (13930,319)cold(14249,618)", vLines, aErr, sizeof(aErr))) << aErr;
	ASSERT_EQ(vLines.size(), 1u);
	EXPECT_EQ(vLines[0].m_TimeMs, 13181);
	EXPECT_EQ(vLines[0].m_DurationMs, 5234);
	EXPECT_EQ(vLines[0].m_Text, "This town is cold");
	ASSERT_EQ(vLines[0].m_vSyllables.size(), 4u);
	EXPECT_EQ(vLines[0].m_vSyllables[0].m_Text, "This ");
	EXPECT_EQ(vLines[0].m_vSyllables[1].m_StartMs, 13655);
	EXPECT_EQ(vLines[0].m_vSyllables[3].m_DurationMs, 618);
}

TEST(QmLyricsParser, BuildsVisibleLineTextFromTimedSyllables)
{
	CLyricLine Line;
	Line.m_TimeMs = 1000;
	Line.m_DurationMs = 2000;
	Line.m_Text = "你好世界";
	Line.m_vSyllables.push_back({1000, 1000, "你好"});
	Line.m_vSyllables.push_back({2000, 1000, "世界"});

	char aVisible[64];
	EXPECT_FALSE(QmLyrics::BuildVisibleLineText(Line, 999, aVisible, sizeof(aVisible)));
	ASSERT_TRUE(QmLyrics::BuildVisibleLineText(Line, 1000, aVisible, sizeof(aVisible)));
	EXPECT_STREQ(aVisible, "你");
	ASSERT_TRUE(QmLyrics::BuildVisibleLineText(Line, 1999, aVisible, sizeof(aVisible)));
	EXPECT_STREQ(aVisible, "你好");
	ASSERT_TRUE(QmLyrics::BuildVisibleLineText(Line, 2000, aVisible, sizeof(aVisible)));
	EXPECT_STREQ(aVisible, "你好世");
	ASSERT_TRUE(QmLyrics::BuildVisibleLineText(Line, 3000, aVisible, sizeof(aVisible)));
	EXPECT_STREQ(aVisible, "你好世界");
}

TEST(QmLyricsParser, YrcParsesCreditLinesAndSkipsMalformedLyricLines)
{
	std::vector<CLyricLine> vLines;
	char aErr[128] = {};
	const char *pYrc =
		"{\"t\":0,\"c\":[{\"tx\":\"作词: \"},{\"tx\":\"Ryan\"}]}\n"
		"malformed line\n"
		"[420,4440](420,1320,0)Lately(1740,0,0), (1740,570,0)I've ";
	ASSERT_TRUE(QmLyrics::ParseYrcLyrics(pYrc, vLines, aErr, sizeof(aErr))) << aErr;
	ASSERT_EQ(vLines.size(), 2u);
	EXPECT_EQ(vLines[0].m_Text, "作词: Ryan");
	EXPECT_EQ(vLines[1].m_TimeMs, 420);
	EXPECT_EQ(vLines[1].m_Text, "Lately, I've");
	EXPECT_GE(vLines[1].m_vSyllables.size(), 3u);
}

TEST(QmLyricsParser, MergesTranslationByTimestamp)
{
	std::vector<CLyricLine> vLines;
	char aErr[128] = {};
	ASSERT_TRUE(QmLyrics::ParseLrcLyrics("[00:01.00]Hello\n[00:02.00]World", vLines, aErr, sizeof(aErr))) << aErr;
	ASSERT_TRUE(QmLyrics::MergeLineTextByTimestamp(vLines, "[00:01.10]你好\n[00:02.00]世界", true, aErr, sizeof(aErr))) << aErr;
	EXPECT_EQ(vLines[0].m_Translation, "你好");
	EXPECT_EQ(vLines[1].m_Translation, "世界");
}

#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_LYRICS_LYRIC_MODEL_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_LYRICS_LYRIC_MODEL_H

#include <cstdint>
#include <string>
#include <vector>

struct CSyllable
{
	int64_t m_StartMs = 0;
	int64_t m_DurationMs = 0;
	std::string m_Text;
};

struct CLyricLine
{
	int64_t m_TimeMs = 0;
	int64_t m_DurationMs = 0;
	std::string m_Text;
	std::string m_Translation;
	std::string m_Romanized;
	std::vector<CSyllable> m_vSyllables;
};

#endif // GAME_CLIENT_COMPONENTS_QMCLIENT_LYRICS_LYRIC_MODEL_H

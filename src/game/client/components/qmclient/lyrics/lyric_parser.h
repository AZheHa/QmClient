#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_LYRICS_LYRIC_PARSER_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_LYRICS_LYRIC_PARSER_H

#include "lyric_model.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace QmLyrics
{

bool ParseLrcLyrics(const std::string &Text, std::vector<CLyricLine> &OutLines, char *pErr, size_t ErrSize);
bool ParseQrcLyrics(const std::string &Text, std::vector<CLyricLine> &OutLines, char *pErr, size_t ErrSize);
bool ParseYrcLyrics(const std::string &Text, std::vector<CLyricLine> &OutLines, char *pErr, size_t ErrSize);
bool MergeLineTextByTimestamp(std::vector<CLyricLine> &vLines, const std::string &TimedText, bool Translation, char *pErr, size_t ErrSize);
bool DecryptQqQrcPayload(const std::string &Payload, std::string &OutText, char *pErr, size_t ErrSize);
std::string BuildNeteaseEapiBody(const char *pUrl, const std::string &JsonPayload, char *pErr, size_t ErrSize);
void SortAndFillDurations(std::vector<CLyricLine> &vLines);
bool BuildVisibleLineText(const CLyricLine &Line, int64_t PositionMs, char *pBuf, size_t BufSize);

} // namespace QmLyrics

#endif // GAME_CLIENT_COMPONENTS_QMCLIENT_LYRICS_LYRIC_PARSER_H

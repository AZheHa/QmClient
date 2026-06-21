#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_MATCH_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_MATCH_H

#include <cstdint>
#include <string>
#include <string_view>

namespace QmLyrics
{

	// 候选歌词来源对照查询的元数据。
	struct SMatchCandidate
	{
		std::string m_Title;
		std::string m_Artist;
		std::string m_Album;
		int m_DurationSec = 0;
	};

	// 查询条件（来自 SMTC 当前播放）。
	struct SMatchQuery
	{
		std::string m_Title;
		std::string m_Artist;
		std::string m_Album; // 可空
		int m_DurationSec = 0; // 0 表示未知
	};

	// 归一化字符串：lowercase（ASCII 部分）、删 (feat.xxx)/(cover)/[live]/-remaster*
	// 等修饰、删标点、保留中日韩汉字、ASCII 去重音（café → cafe）。
	// 返回归一化后的字符串；空输入返回空。
	std::string NormalizeForMatch(std::string_view Input);

	// Levenshtein 距离（字符级，UTF-8 安全：以 UTF-8 码点为单位）。
	int LevenshteinDistance(std::string_view A, std::string_view B);

	// 把字符串按空白拆分为 token 集合后比对相似度（0..1）。
	// 行为：A 和 B 归一化 → 分词 → set 交集大小 × 2 / (|A| + |B|)。
	// 适合艺人名顺序不同（"Pink Floyd" vs "Floyd, Pink"）的情况。
	float TokenSetRatio(std::string_view A, std::string_view B);

	// duration 容差评分。
	// 返回 0..1：完全匹配 1.0，差 TolMs 时降到 0；超出线性。
	// TolMs 通常 5000；查询 0 表示未知，返回 0.5（中性）。
	float DurationScore(int QuerySec, int CandidateSec, int TolMs = 5000);

	// 综合评分（0..100）。
	// 默认权重：title=50, artist=30, duration=20。
	// 各字段先归一化后用 LevenshteinDistance / max_length 转 0..1 相似度。
	// 候选/查询中任一字段为空时该字段计入但视作 0。
	struct SMatchWeights
	{
		float m_TitleWeight = 50.0f;
		float m_ArtistWeight = 30.0f;
		float m_DurationWeight = 20.0f;
	};

	float ScoreMatch(const SMatchQuery &Query, const SMatchCandidate &Candidate, const SMatchWeights &Weights = {});

	// 整体评分（0..100）的便捷封装，使用默认 SMatchWeights。
	inline float Score(const SMatchQuery &Q, const SMatchCandidate &C)
	{
		return ScoreMatch(Q, C, SMatchWeights{});
	}

} // namespace QmLyrics

#endif

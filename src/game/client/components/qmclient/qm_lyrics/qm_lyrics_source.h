#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_QM_LYRICS_QM_LYRICS_SOURCE_H

#include "qm_lyrics_match.h"
#include "qm_lyrics_model.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace QmLyrics
{

	// 查询：来自 SMTC 当前播放（已用 SMatchQuery 表示）。
	using SSourceQuery = SMatchQuery;

	// 候选：源返回的一条歌词候选。
	struct SSourceCandidate
	{
		std::string m_RawText; // 原始歌词文本（LRC/eslrc/TTML）
		EFormat m_FormatHint = EFormat::LRC_ENHANCED; // 源对格式的提示
		SMatchCandidate m_Metadata; // 候选自报的 title/artist/album/duration
		float m_SourceScore = 0.0f; // 源自评的置信度，0..1
	};

	// 异步结果回调（运行在调用方线程，源实现需保证线程安全）。
	using FSourceDoneCallback = std::function<void(std::vector<SSourceCandidate>)>;
	using FSourceErrorCallback = std::function<void(const char * /*Error*/)>;

	// 歌词数据源抽象。
	class IQmLyricsSource
	{
	public:
		virtual ~IQmLyricsSource() = default;

		// 源唯一标识，如 "lrclib"、"qq"，用于缓存 source 字段和日志。
		virtual const char *Id() const = 0;

		// 启动一次异步查询。Done 在成功 (即使候选为空) 时回调；Error 在网络/解析失败时回调。
		// 实现应非阻塞。
		virtual void QueryAsync(const SSourceQuery &Query, FSourceDoneCallback Done, FSourceErrorCallback Error) = 0;

		// 取消当前进行中的查询（如有）。允许多次调用。
		virtual void Cancel() = 0;
	};

} // namespace QmLyrics

#endif

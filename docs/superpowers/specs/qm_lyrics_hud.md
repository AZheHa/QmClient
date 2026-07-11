---
title: QmClient 游戏内歌词 HUD 稳定规格
date: 2026-06-21
last_reviewed: 2026-07-11
status: active
scope: Windows SMTC、歌词搜索/缓存/解析和游戏内 HUD；不创建独立窗口，不改协议、物理、预测或 demo 格式
license: clean-room 实现；不得复制 BetterLyrics 或其他不兼容来源的源码、资源、shader 或编译产物
---

# 用户行为

- 歌词只显示在 QmClient 游戏内，可作为独立 HUD 元素或灵动岛内容。
- Windows 上只读系统媒体会话，识别当前标题、艺术家、专辑、时长、播放位置、更新时间、播放速率和暂停状态。
- 非 Windows 平台不得引入 WinRT 依赖或伪造媒体状态；不可用能力应安全不生效。
- 开关歌词时保留仍有效的已加载轨道；取消中的旧搜索不得在重开或换歌后覆盖新状态。
- seek、换歌、切换媒体会话和播放速率变化必须重新锚定时间轴。手动 offset 与文件内 offset 的方向必须一致且可测试。
- 未找到歌词、暂停隐藏、媒体岛内显示和独立 HUD 显示按配置工作，不能同时重复绘制。

# 代码所有权

源码位于 src/game/client/components/qmclient/qm_lyrics/：

- qm_lyrics.{h,cpp}：组件状态机、媒体快照、搜索调度和 HUD 接线。
- qm_lyrics_model.h：统一 track、line、word 时间轴模型。
- qm_lyrics_parser_*：LRC/Enhanced LRC/ESLRC、TTML 及来源格式解析。
- qm_lyrics_source*.{h,cpp}：IQmLyricsSource 及各 provider。
- qm_lyrics_match.*：查询归一化、候选评分和稳定排序。
- qm_lyrics_cache.*：索引、payload、安全文件名和过期处理。
- qm_lyrics_song_search_map.*：歌曲到成功搜索来源/查询的持久映射。
- qm_lyrics_clock.*：SMTC 锚点、插值、暂停和 seek。
- qm_lyrics_render.*：行级滚动、逐字高亮、淡出和布局辅助。

配置真相源是 src/engine/shared/config_variables_qmclient.h 中的 qm_lyrics_* 项；规格不复制默认值表，避免与代码漂移。

# 数据流

1. 读取当前 SMTC snapshot，生成稳定的歌曲 identity 和 SSourceQuery。
2. identity 未变化时只更新时间轴；identity 变化时取消旧 generation。
3. 启用缓存时先查合法且未过期的本地条目。
4. 缓存未命中后，按 source、source order 和 search type 选择顺序或并发搜索。
5. 每个 provider 只返回候选和原始歌词，不直接修改组件状态。
6. 匹配器按全局/来源阈值排序；解析失败时继续尝试同批次后续候选。
7. 成功结果写入安全 payload、索引和搜索映射，再发布统一 SLyricsTrack。
8. 渲染器使用插值后的播放时间定位活动行/词。

# 来源与格式

当前配置为以下来源保留明确枚举：自动、LRCLIB、Kugou、QQ、Netease、AMLL TTML DB、Apple Music、本地媒体文件、本地 LRC、本地 ESLRC、本地 TTML。

- 自动模式只调度已实现且启用的 provider。
- source order、provider threshold 和 ignore-cache 列表必须容忍未知名称、重复项和空配置。
- 并发模式必须稳定处理同分候选，旧请求回调必须受 generation 保护。
- HTTP provider 必须复用 DDNet HTTP 接口；歌词代理只设置在歌词请求上，不能改变客户端其他 HTTP 请求。
- provider 返回的 LRC、逐字歌词、TTML 或来源专用格式必须先转换为统一模型再渲染。

# 缓存合同

- 缓存命中优先于网络。
- payload 文件名必须经过白名单校验，不能包含路径穿越、绝对路径或平台分隔符。
- 主文件名使用安全化的歌曲名与歌手名，冲突和非法字符必须有确定性 fallback。
- 索引损坏、条目过期或 payload 缺失时同时清理无效引用。
- 写索引和 payload 失败不得破坏当前内存轨道，也不得删除仍有效的其他歌曲。
- 旧缓存兼容只允许作为显式迁移 fallback；迁移完成后应删除旧路径，而不是永久维护两套写入格式。

# 时钟合同

- position、last updated 和 playback rate 共同形成锚点。
- playback 状态变化与 timeline 样本更新必须独立建模；恢复播放但 timeline 仍旧时，从暂停冻结位置继续，不能复用暂停前 sample tick。
- 暂停时不继续推进；恢复时从新 snapshot 重新锚定。
- 明确 seek 或超过 drift threshold 的跳变必须立即硬切，不能用长时间平滑追赶。
- 普通 snapshot 抖动可平滑，但不得让歌词在新旧歌曲之间倒退。
- 歌词搜索、缓存读取和解析完成只发布歌词数据，不得创建、重置或重新起算播放器时钟。
- 活动行查找、行间空档、首词前、末词后和无逐字时间戳均需确定性 fallback。

# HUD 与设置

- 独立 HUD 复用 HUD editor transform、贴边 margin、UI scale 和动效等级。
- 行数、字号、间距、透明度、活动行缩放、逐行衰减、滚动时长和颜色只能从 qm_lyrics_* 配置读取。
- karaoke 关闭时按行渲染；开启时按 word 时间推进，首词前和末词后不得重复进行无效测宽。
- 翻译/音译只在轨道实际包含内容且对应开关启用时显示。
- 媒体岛宽度必须限制长歌词，不能遮挡 CheckPoint 等更高优先级 HUD 信息。
- 所有用户可见文本必须走 QmClient i18n 维护源和生成链。

# 安全与兼容边界

- 不复制 BetterLyrics、Lyricify helper 或其他许可证不兼容实现的源码和资源；只允许按公开行为 clean-room 重写。
- 不增加 DLL 热加载、脚本插件或未沙箱化第三方代码。
- 不改变 DDNet 协议、snapshot、物理、预测、demo/skin/map 格式或排名语义。
- 不记录媒体 token、代理认证或完整私人媒体路径到公开日志。
- Apple Music token 等敏感配置必须保持 insensitive 语义。

# 当前活动计划

背景粒子、镜头、歌词 seek/代理/搜索/缓存的本轮收口以 [当前执行计划](../plans/2026-07-11-background-camera-lyrics-module-refactor.md) 为唯一执行计划。计划完成后，稳定行为回写本文并删除已消费计划。

# 验收

自动化至少覆盖：

- LRC/TTML/来源格式解析边界；
- 标题归一化、候选排序、阈值和坏候选 fallback；
- 缓存 key、文件名安全、过期/损坏清理和旧缓存迁移；
- SMTC 插值、暂停、seek、offset 和换歌 generation；
- provider URL/response、取消与 HTTP proxy 隔离；
- 行级/逐字渲染辅助、空档和长行边界。

代码变更按范围运行歌词聚焦测试、全量 C++ 测试、i18n 流水线、game-client 构建和 quick/default gate。

# 仍需人工验证

在 Windows 真机分别使用至少两个提供 SMTC 的播放器，检查 mid-song attach、pause/resume、前后 seek、换歌、无歌词、缓存命中、网络失败、代理开关、独立 HUD/媒体岛切换和非默认 UI scale。没有这份证据时，不得声称端到端歌词体验已完成。

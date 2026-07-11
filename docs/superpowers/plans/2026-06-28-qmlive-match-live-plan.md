---
title: QmLive Match 剩余阶段计划
date: 2026-06-28
last_reviewed: 2026-07-11
status: active
scope: QmLive entry mode、录制生命周期、服务端赛事权威与管理 UI；普通 DDNet 兼容性不可改变
---

# 已有基础

当前 master 已具备客户端 finish ranking、可选 sidecar、标准 demo replay、team filter、overlay 和对应 QmLive 测试。Phase 1 已完成，不在本文重复记录实现过程。

仍存在两个客户端基础缺口：

- recorder 继续复用手动录制槽，缺独立生命周期和临时文件到最终文件的原子重命名；
- owner 不明确的标准事件/声音只能严格抑制，完整 filter 覆盖仍需审计。

# 执行顺序

每一阶段都必须拆成单独任务、单独审查和单独验收，不能一次跨阶段实现。

## Phase 2A：Entry Mode

- 为 Axiom auto-login 增加明确的 QmLive connection-mode gate。
- 提供连接前模式选择、当前模式显示、手动 override 和按地址记忆。
- 普通 QmClient/DDNet 构建与连接路径不得出现 QmLive 副作用。

验收：

- DDNet 模式不触发 Axiom 命令；
- QmLive 模式选择可恢复、可覆盖、错误配置可安全回退；
- 配置只使用 qm_/Qm 前缀。

## Phase 2B：Client UI 与录制

- 在 QmLive-only UI 中提供录制状态、team filter 和 finish card 控制。
- 建立独立 recorder 生命周期、失败清理和原子 finalize。
- 审计 chat、HUD、skins、events、sounds 的 owner-known/unknown filter 路径。

验收：

- 普通 DDNet demo 格式不变；
- sidecar 缺失、损坏或不匹配时标准 demo 仍可播放；
- seek、切换 filter 和录制失败不会留下旧状态或半成品最终文件。

## Phase 3：QmLiveServer Match Authority

本阶段会触及服务端玩法和时序，实施前必须获得明确批准并另写稳定设计。

- 定义 Idle、Preparing、Ready、DelayedStart、Countdown、Running、Finished、Reset 状态转换。
- 定义 Pending、Applied、Rejected、Superseded、Expired 队伍分配状态。
- 所有管理权只复用现有 admin/rcon 权限，不接受客户端自声明。
- race lock、倒计时释放和官方 finish timing 不得绕过 DDRace team safety。

## Phase 4：Admin、Referee 与 Timeline

Phase 3 通过后再规划：

- 搜索、多选、队伍分配、pending 状态和取消/替换；
- referee action、undo 和审计日志；
- match start、finish、penalty、warning、DNF/DNS/DSQ、reset、bookmark timeline；
- sidecar 只能通过向后兼容的可选字段扩展。

# 验证要求

- 客户端阶段：QmLive 聚焦测试、全量 C++ 测试、game-client 构建和 quick/default gate。
- 服务端阶段：状态转换、分配过期/替换、race lock、同 tick GO、finish timing 和 referee undo 单测。
- 人工验收按 [QmLiveClient 验收手册](../../qmlive-acceptance.md) 执行。

# 非目标

- 不改变普通 DDNet 协议、demo 格式、排名语义或物理。
- 不把后续阶段的 UI、服务端权威和 recorder 重构混进同一补丁。
- 不把当前 master 之外的分支实现当成完成证据。

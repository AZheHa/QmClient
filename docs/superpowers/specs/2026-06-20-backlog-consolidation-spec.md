---
title: QmClient 当前功能 backlog
date: 2026-06-20
last_reviewed: 2026-07-11
status: active
scope: 当前 master 尚未完成或只部分完成的产品需求索引；每次实现只选择一项并另写小规格
---

# 使用规则

- 本文只记录当前代码仍存在的差距，不保存已完成任务的过程记录。
- partial 表示已有可复用基础，但本文列出的验收仍未闭环；open 表示当前 master 未实现。
- owned 表示已有独立活动计划，后续状态只在对应计划维护，本文不重复展开。
- 非当前 master 祖先的提交只能作为参考，不能据此标记完成。
- 开始实现前重新核对调用点、配置、翻译和测试，并把单个条目拆成独立 plan/spec。

# P0：缺陷与量化

| ID | 状态 | 当前剩余工作 | 下一步 |
| --- | --- | --- | --- |
| PERF-UI-RUNTIME | owned | UI 性能静态实现已完成，fresh client runtime FPS 验收尚未执行 | 按 [UI runtime 验收计划](../plans/2026-06-18-ui-frame-pacing-full-performance-plan.md) 采集新日志并收口 |
| SETTINGS-SCROLL-JITTER | partial | 设置页滚动生命周期已有统一基础，但 resource jobs 的 finalize/limiter 仍存在阶梯式突发，且 QmClient/TClient 滚动没有 fresh runtime 与视觉闭环 | 先在相同页面和输入下采样滚动帧，再为确认的单一 burst 建计划 |
| CHAT-SYSTEM-PREFIX | open | 系统聊天仍会生成 *** 前缀 | 先固定服务端消息与本地系统消息兼容边界，再补行为测试 |
| CHAT-TRANSLATE-POPUP | open | 翻译按钮位置、语言弹窗尺寸和关闭路径未收口 | 写独立 UI 规格并做视觉验收 |
| EMOTICON-MODIFIERS | partial | 常见 Win/Ctrl+Shift 路径已有保护，普通修饰键 fallback 仍可能触发表情 | 明确允许的 modifier 组合并补输入测试 |
| FRIEND-NOTIFICATION-STUTTER | open | 卡顿根因仍未通过实机 telemetry 区分 | 先做可复现采样，禁止凭旧探索直接改代码 |
| GORES-FASTINPUT-COST | open | 距离场已分帧；FastInput 的持续预测成本仍未量化 | 对 tc_fast_input* 做同场景 A/B 和帧归因 |
| LINUX-WARNINGS | open | GCC warning 清单中的真实对象语义问题尚未重新审计 | 在当前 GCC 构建上重跑，再逐条建小修复 |

# P1：现有功能收口

| ID | 状态 | 当前剩余工作 | 下一步 |
| --- | --- | --- | --- |
| SKIN-SORT-CARDS | open | 皮肤列表无 mtime 排序配置，卡片布局仍固定四列 | 设计稳定排序键、迁移策略和响应式列数测试 |
| HUD-CHAT-EDGE | partial | 通知栏已有共用边距 helper；聊天框未接横向流向和安全边距 | 复用现有 HUD editor geometry，统一编辑器与实际渲染 |
| CHAT-SCROLLBAR | partial | 已有拖拽滚动条，但固定在右侧且宽度固定 | 让位置跟随贴边方向，并补窄宽度交互测试 |
| CHAT-ANIMATION | partial | 已有 presentation state 和全局动画开关，缺独立 slide/fade/easing 配置 | 先定义最小配置集，避免再造动画框架 |
| WEAPON-SWITCH-ANIMATION | open | 时长、距离、旋转和 easing 仍为硬编码 | 用现有 Qm 动效等级接入最小高级配置 |
| SKIN-TRANSITION | open | 现有五种类型缺 easing、强度和新增过渡形态 | 先确认产品需要的首批形态，再做视觉规格 |
| NOTIFICATION-COPY | partial | 部分 i18n 文案已自然化，但没有按规则类型完成全量审计 | 建立当前 source key 清单后逐类收口 |
| PROCESS-PRIORITY | partial | 只有 Windows 启动时高优先级，无 UI、热重载和跨平台语义 | 先决定是否仅支持 Windows；不要伪造其他平台能力 |
| IME-STYLE | partial | 新 IME 开关已存在，Windows system-native 仍不可作为完整三选一模式 | 先核对平台回退和失焦行为，再设计选择控件 |
| SERVER-I18N | open | 服务端玩家可见文本仍是英文为主的混合状态 | 先定义协议/业务数据与可本地化文本边界 |
| FRIEND-CATEGORY-DISCOVERY | partial | CRUD、持久化、拖拽和右键管理已实现，但入口不易发现 | 增加显式管理入口、tooltip 与新建分类路径 |

# P2：独立增强

| ID | 状态 | 当前剩余工作 | 前置决策 |
| --- | --- | --- | --- |
| SHORT-SERVER-NAMES | open | 当前 master 无 KoG/Axiom/CHN 简名规则 | 收集稳定服务器样本；未合入分支只作参考 |
| EMOTICON-DEPTH | open | 表情无阴影/深度表现 | 明确是否只做 2D 阴影，避免误称 3D |
| PARTICLE-WEAPON-3D | open | 当前背景粒子 mesh 重构不等于武器/钩子独立线框粒子 | 等当前 2026-07-11 计划收口后另立规格 |
| HOOK-PROGRESS | partial | 已有强弱钩基础显示，缺倒计时、圆形进度和临界反馈 | 确认服务端信息可达性与近似标识 |
| HUD-EDITOR-ADVANCED | partial | 已有吸附 guide，缺多选、布局和 guide 视觉增强 | 将多选、布局、guide 皮肤拆成独立任务 |
| WEAPON-HIT-FEEDBACK | partial | hook line 与 trajectory 已有，缺 hit marker/粒子反馈 | 明确预测近似和所有者归属 |
| CHAT-BUBBLE-COMIC | partial | 基础颜色、字体、圆角框已有，缺样式枚举和漫画尾巴 | 先给出可检查的两种视觉样式 |
| FRIEND-AUTO-FOLLOW | open | 当前 master 只有服务器信息和连接入口，无跟随状态机 | 决定移植参考分支还是按当前代码重做 |
| ALT-ACCOUNT-TAGS | open | 客户端与中心服务均无完整注册、私有标注和展示契约 | 必须先写客户端/服务端接口规格并获远端权限 |
| PLAYER-RANKS | partial | MapHistory 已持久化地图历史，但不是玩家级 statboard/ranks | 先决定扩展 MapHistory 还是建立独立统计模型 |
| UI-REDESIGN | open | 旧 HTML 原型已失去当前状态和实现依据 | 若仍需要全 UI 重设计，先重新确认范围并创建新规格 |

# 独立计划

- [QmLive 后续阶段](../plans/2026-06-28-qmlive-match-live-plan.md)。
- [当前背景粒子、镜头和歌词收口](../plans/2026-07-11-background-camera-lyrics-module-refactor.md)。
- [歌词稳定行为规格](qm_lyrics_hud.md)。

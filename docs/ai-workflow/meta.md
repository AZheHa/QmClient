# AI 工作流文档规范

`docs/ai-workflow/` 只保留对 AI agent 真正有用、且跨会话稳定的规则文档。

目标：

1. 让 agent 快速知道该读什么。
2. 避免把容易过期的状态、历史和一次性说明放进 `docs/ai-workflow/`。

## 当前允许的文件

- `meta.md`
- `ddnet-development.md`
- `verification.md`
- `review.md`
- `git-workflow.md`
- `advanced/README.md`
- `advanced/*.md`

`advanced/` 只放专项稳定规则，例如性能、重构、安全、内存生命周期、线程 jobs、观测和回归防护。它们是按风险触发的补充规则，不替代根目录基础规则。

## 不应放进 `docs/ai-workflow/` 的内容

- 改动历史
- 会话交接
- feature/status JSON
- 冗长的文档体系自解释
- 与 `docs/superpowers/` 重叠的任务内容
- 仅为脚本清单服务的重复说明

## 正确放置位置

- `docs/superpowers/plans/`：执行计划、验证证据、剩余问题
- `docs/superpowers/specs/`：稳定规格
- `docs/superpowers/explore/`：探索记录
- `qmclient_scripts/`：脚本与脚本专属说明

## 写作要求

- 保持简短。
- 一份文档只负责一个主题。
- 优先写稳定规则，不写历史背景。
- 如果规则可以机械化，就交给脚本，而不是扩写 prose。

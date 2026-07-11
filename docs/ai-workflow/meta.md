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
- `advanced/feature-introduction.md`
- `advanced/memory-lifetime.md`
- `advanced/observability-debugging.md`
- `advanced/performance-workflow.md`
- `advanced/perf-system-workflow.md`
- `advanced/refactor-workflow.md`
- `advanced/regression-prevention.md`
- `advanced/safety-security.md`
- `advanced/threading-jobs.md`

`advanced/` 只放专项稳定规则，例如性能、重构、安全、内存生命周期、线程 jobs、观测和回归防护。它们是按风险触发的补充规则，不替代根目录基础规则。

这个清单由 `qmclient_scripts/gate/check_docs.py` 机械校验。新增、删除或改名时，必须同时更新本文、`advanced/README.md` 和检查脚本。

## 不应放进 `docs/ai-workflow/` 的内容

- 改动历史
- 会话交接
- feature/status JSON
- 冗长的文档体系自解释
- 与 `docs/superpowers/` 重叠的任务内容
- 仅为脚本清单服务的重复说明

## 正确放置位置

- `docs/superpowers/README.md`：当前活动文档索引和生命周期入口
- `docs/superpowers/plans/`：正在执行或仍有明确验收 gap 的计划
- `docs/superpowers/specs/`：当前稳定规格和待办索引
- `docs/superpowers/explore/`：尚未形成决策的当前调查；形成结论后迁入 plan/spec 并删除
- `qmclient_scripts/`：脚本与脚本专属说明

`docs/superpowers/` 的活动 Markdown 必须使用 frontmatter `status: active` 或 `status: draft`，并登记在 `README.md`。完成、过时或已取代内容从当前树删除，由 Git 历史归档；禁止 `archive/`、`reports/`、`reviews/` 和 HTML 副本。

## 写作要求

- 保持简短。
- 一份文档只负责一个主题。
- 优先写稳定规则，不写历史背景。
- 如果规则可以机械化，就交给脚本，而不是扩写 prose。

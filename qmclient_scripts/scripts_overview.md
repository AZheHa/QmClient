# scripts_overview

这个文档统一说明 `qmclient_scripts/` 的脚本分层、推荐入口，以及 `check_gate.py` 相关工作流语义。

目标：

- 给 agent 一个单一脚本说明入口
- 不再把“脚本总览”和“gate 工作流说明”拆成两份重复文档

## 脚本分层

### `scripts/` 与 `qmclient_scripts/` 边界

根目录 `scripts/` 视为 DDNet 上游同步目录。QmClient 自有逻辑、gate 入口、生成脚本和检查脚本都放在 `qmclient_scripts/`，不要在 gate 或 GitHub workflow 中直接调用 QmClient 修改过的 `scripts/` 文件。

需要复用上游脚本时，先把脚本复制到 `qmclient_scripts/`，在复制件上做 QmClient 适配；不要让 `qmclient_scripts/` 里的脚本反向依赖根目录 `scripts/` 的 QmClient 特化行为。唯一例外是明确保持上游原样的脚本或平台入口，例如 `scripts/android/cmake_android.sh`。

### 1. `gate/`

这是仓库级门禁与脚本治理层。

当前规范入口：

| 入口 | 说明 |
|------|------|
| `qmclient_scripts/gate/check_gate.py` | Python 版仓库级门禁总入口 |
| `checks/strict_build` | 严格构建与静态分析（`check_gate.py` 调度模块） |
| `qmclient_scripts/gate/check_docs.py` | 治理文档一致性检查（可带 `--sync-only`） |
| `qmclient_scripts/gate/baseline_debt_allowlist.json` | 基线白名单数据 |

适用：

- 跑仓库级 `quick/default/full` 门禁
- 跑严格构建、`/analyze`、clang-tidy、ASan
- 校验 `AGENTS.md` / `CLAUDE.md` / 精简 `docs/ai-workflow/` / CI 入口是否一致
- 维护 baseline debt allowlist

### 2. 构建与平台辅助

这类脚本负责让构建或平台行为成立，不是门禁总入口。

当前主要脚本：

- `qmclient_scripts/cmake-windows.cmd`
- `qmclient_scripts/darwin_fix_install_names.py`
- `qmclient_scripts/make_lib_openssl.sh`

### 3. 代码卫生与内容生成辅助

这类脚本是具体检查项或生成项，不负责仓库级编排。

当前主要脚本：

- `qmclient_scripts/check_config_variables.py`
- `qmclient_scripts/check_header_guards.py`
- `qmclient_scripts/bump_version.py`
- `qmclient_scripts/fix_style.py`
- `qmclient_scripts/export_settings_commands_table.py`
- `qmclient_scripts/generate_release_notes.py`

### 4. 其他专用脚本

与门禁主链无直接关系，按各自职责独立存在：

- `qmclient_scripts/languages_qmclient/`
- `qmclient_scripts/qmclient_center_server/`
- `qmclient_scripts/diff_update.py`
- `qmclient_scripts/tw_api.py`
- `qmclient_scripts/update.zsh`

## 推荐入口

### 仓库级门禁

```bash
python qmclient_scripts/gate/check_gate.py --mode quick
python qmclient_scripts/gate/check_gate.py --mode default --base-ref main
python qmclient_scripts/gate/check_gate.py --mode full --base-ref main
```

### 文档入口一致性

```bash
python qmclient_scripts/gate/check_docs.py
python qmclient_scripts/gate/check_docs.py --sync-only --prefer agents
```

### GitHub Release 说明

```bash
python qmclient_scripts/generate_release_notes.py --version vX.Y.Z --current-tag vX.Y.Z --output tmp/release-notes.md
```

### 版本号收口

```bash
python qmclient_scripts/bump_version.py --version X.Y.Z
python qmclient_scripts/bump_version.py --tag vX.Y.Z
```

### baseline allowlist

```bash
python qmclient_scripts/gate/check_gate.py --mode quick --base-ref main --report-json-path tmp/check-gate-report.json
python qmclient_scripts/gate/tools/refresh_allowlist.py --report tmp/check-gate-report.json --output qmclient_scripts/gate/baseline_debt_allowlist.json
```

说明：

- `refresh_allowlist.py` 是人工确认后的维护工具，不会被 gate 自动调用
- 先看 JSON 报告，再决定是否增量合并或 `--rewrite` 全量重写

## `check_gate.py` 工作流语义

### 角色

`qmclient_scripts/gate/check_gate.py` 是仓库级总入口。

它负责：

- 把源码卫生检查、严格调试检查、测试、allowlist 与 JSON 报告收口成统一工作流
- 用 `check_docs.py` 的内建最小规则校验根规则和文档入口是否齐全
- 区分“已知历史债务”和“当前新增阻断”

### 模式

#### `quick`

- 开发期快速自查
- 不跑真实构建
- 不跑测试

默认内容：

- 配置变量使用检查
- 文档一致性检查
- 头文件 guard 检查
- 标准头文件检查
- `fix_style.py -n`

#### `default`

- 日常提交前严格门
- 跑真实构建、严格静态分析和 C++ 测试

默认内容：

- `quick` 全部
- `checks/strict_build`
- `run_cxx_tests`（会构建 `testrunner`，并在 build 目录下执行测试二进制；源码结构测试通过测试源码根读取 `src/...` / `data/...`）

#### `full`

- 集中收口 / 准发布门
- 在 `default` 基础上增加更重检查

默认内容：

- `default` 全部
- 标识符命名检查
- `run_rust_tests`

### 默认构建口径

- 运行/测试目录默认是 `cmake-build-release`
- 严格调试检查目录默认是 `cmake-build-debug` / `cmake-build-analyze`
- Windows 默认通过 `qmclient_scripts/cmake-windows.cmd` 进入 CMake
- 在 Windows 宿主上验证 Linux 构建时，推荐通过 WSL Ubuntu + GCC/G++ + CMake + Ninja 走原生 Linux 口径，并使用独立目录（例如 `cmake-build-linux-release`），不要复用 Windows 的 `cmake-build-release`
- 同一 build 目录里的 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 必须串行执行，不要并行调度；并行验证只能通过拆分到不同 build 目录实现

### 结果分类

`check_gate.py` 的失败应至少能区分为：

- `环境/工具`
- `仓库基线债务`
- `当前改动/构建阻断`

### 常用命令

```bash
python qmclient_scripts/gate/check_gate.py --mode default --base-ref main --explain-scope --report-json-path tmp/check-gate-report.json
```

## 不要这样用

- 不要把 `check_config_variables.py`、`fix_style.py`、`check_header_guards.py` 误当成仓库级总入口
- 不要绕开 `qmclient_scripts/gate/check_gate.py` 自己临时拼一套等价门禁
- 不要把 `qmclient_scripts/` 根目录当成完全平级；门禁相关内容统一以 `gate/` 为准
- 不要在 QmClient gate 或 workflow 中直接调用根目录 `scripts/` 下的 QmClient 特化脚本；改用 `qmclient_scripts/` 复制件

## 关联文档

- `docs/ai-workflow/meta.md`
- `docs/ai-workflow/verification.md`

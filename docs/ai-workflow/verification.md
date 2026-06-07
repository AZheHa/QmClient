# 验证

用能覆盖改动风险的最小验证集合，然后把证据记录到当前 `docs/superpowers/plans/` 或 `docs/superpowers/specs/`。

## Harness 与文档

```bash
python qmclient_scripts/gate/check_docs.py
```

当你改了 `AGENTS.md`、`CLAUDE.md`、`docs/ai-workflow/`、`docs/superpowers/plans/`、`docs/superpowers/specs/`、governance workflow 文件或 gate 脚本后，都要跑这一项。

## 构建

Windows 推荐：

```pwsh
qmclient_scripts/cmake-windows.cmd -G Ninja -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target game-client -j 14
```

说明：当前仓库的自动化与 Agent 会话在 Windows 上默认走 `qmclient_scripts/cmake-windows.cmd`，因为不能假设当前 PowerShell 已经注入了可用的 MSVC 环境。当前 canonical 的 `cmake-build-*` 目录按 Ninja 生成器维护；只有在调用方已经明确处于可用的 VS/MSVC shell 时，才可以直接使用裸 `cmake`。

Linux/macOS：

```sh
cmake -G Ninja -S . -B cmake-build-release -DCMAKE_BUILD_TYPE=Release
cmake --build cmake-build-release --target game-client -j 14
```

## 测试

Windows:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_rust_tests
```

说明：常规运行/测试目录默认是 `cmake-build-release`；C++ 测试主路径是先构建 `testrunner`，再直接执行测试二进制。`default/full` gate 里的严格构建与静态分析会另外使用 `cmake-build-debug` 和 `cmake-build-analyze`。

Linux/macOS:

```sh
cmake --build cmake-build-release --target run_cxx_tests
cmake --build cmake-build-release --target run_rust_tests
```

## Gate 模式

```bash
python qmclient_scripts/gate/check_gate.py --mode quick --base-ref main
python qmclient_scripts/gate/check_gate.py --mode default --base-ref main
python qmclient_scripts/gate/check_gate.py --mode full --base-ref main
```

版本 / release 相关修改后，至少额外验证：

```bash
python qmclient_scripts/bump_version.py --version 2.58.0 --dry-run
python qmclient_scripts/generate_release_notes.py --version "$(git describe --tags --abbrev=0)" --current-tag "$(git describe --tags --abbrev=0)"
```

## 视觉改动

对菜单、HUD、UI 控件、浏览器列表行、设置页、覆盖层和动画类改动：

- Build the client.
- Launch `DDNet.exe`.
- Verify the target screen at normal UI scale and at least one non-default scale if the layout is scale-sensitive.
- Check hover, selected, disabled, modal, keyboard, and controller paths if relevant.
- Capture screenshots when preparing a PR or visual handoff.

## 证据格式

记录格式：

```text
Command: <exact command>
Result: <pass/fail and key output>
Scope: <what this proves>
Gaps: <what was not verified>
```

没有证据就不要把功能标成 `done`。如果某项检查因为环境或时间跑不了，也要明确记成 gap。

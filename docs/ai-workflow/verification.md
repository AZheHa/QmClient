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

说明：如果当前宿主是 Windows，但需要验证 Linux 构建，优先在 WSL Ubuntu 中使用 GCC/G++、CMake 和 Ninja 走原生 Linux 构建，不要复用 Windows 的 `cmake-build-release` 目录。推荐单独使用 `cmake-build-linux-release` 之类的目录，避免和 Windows 生成的 `CMakeCache.txt` 冲突。已验证可用的 WSL 口径示例：

```pwsh
wsl env HOME=/home/<user> bash -lc 'set -e; . "$HOME/.cargo/env"; cd /mnt/<drive>/<path-to-repo>; cmake -G Ninja -S . -B cmake-build-linux-release -DCMAKE_BUILD_TYPE=Release -DDOWNLOAD_GTEST=ON; cmake --build cmake-build-linux-release --target game-client -j 14'
```

如果需要 Linux 打包，可直接把 target 切到 `package_default`：

```pwsh
wsl env HOME=/home/<user> bash -lc 'set -e; . "$HOME/.cargo/env"; cd /mnt/<drive>/<path-to-repo>; cmake -G Ninja -S . -B cmake-build-linux-release -DCMAKE_BUILD_TYPE=Release -DDOWNLOAD_GTEST=ON; cmake --build cmake-build-linux-release --target package_default -j 14'
```

## 测试

Windows:

```pwsh
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release/testrunner.exe
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_rust_tests
```

说明：常规运行/测试目录默认是 `cmake-build-release`；C++ 测试主路径是先构建 `testrunner`，再直接执行测试二进制。`default/full` gate 里的严格构建与静态分析会另外使用 `cmake-build-debug` 和 `cmake-build-analyze`。

重要：同一 build 目录中的 `game-client`、`testrunner`、`run_cxx_tests`、`run_rust_tests`、`package_default` 不要并行发起。它们会共享生成产物与中间文件，代理或脚本必须串行执行；如果确实要并行，只能拆到不同的 build 目录。

Linux/macOS:

```sh
cmake --build cmake-build-release --target run_cxx_tests
cmake --build cmake-build-release --target run_rust_tests
```

如果走 Windows 宿主下的 WSL Linux 验证，对应地把目录替换成独立的 Linux build 目录，例如：

```pwsh
wsl env HOME=/home/<user> bash -lc 'set -e; . "$HOME/.cargo/env"; cd /mnt/<drive>/<path-to-repo>; cmake --build cmake-build-linux-release --target run_cxx_tests -j 14; cmake --build cmake-build-linux-release --target run_rust_tests -j 14'
```

## Gate 模式

```bash
python qmclient_scripts/gate/check_gate.py --mode quick --base-ref main
python qmclient_scripts/gate/check_gate.py --mode default --base-ref main
python qmclient_scripts/gate/check_gate.py --mode full --base-ref main
```

说明：除非用户明确把任务限制为纯调查、纯文档同步或只要求某个单项命令，否则不要只用 build/test 代替 gate。至少选择一条与本轮范围匹配的 gate 作为验收证据：

- 纯文档 / harness 变更：`python qmclient_scripts/gate/check_docs.py`
- 常规代码改动：至少 `python qmclient_scripts/gate/check_gate.py --mode quick --base-ref main`
- 提交前日常严格门：优先 `python qmclient_scripts/gate/check_gate.py --mode default --base-ref main`
- 集中收口 / 准发布：`python qmclient_scripts/gate/check_gate.py --mode full --base-ref main`

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

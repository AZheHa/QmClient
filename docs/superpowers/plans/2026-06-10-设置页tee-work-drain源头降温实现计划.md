# 设置页 Tee Work Drain 源头降温 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不重建量化系统、不中途扩展 FBO 的前提下，把 `settings:tee` 页面的主线程 `Work Drain` 压回到受控范围，优先收紧后台 backlog 和 idle drain 的过度预取。

**Architecture:** 现有 perf summary 已经证明 `settings:tee / Work Drain` 是当前第一热点，问题不是“日志不够”，而是 Tee 列表在 `all_visible_ready -> idle_drain` 之后会持续为整表发放过大的后台请求窗口，导致 `BACKGROUND_REQUESTED -> PENDING/LOADING -> finalize/upload/merge` 长时间与交互帧重叠。本轮只做 source cooling：收紧 `settings_resource_jobs.*` 的背景请求预算与 backlog 上限，保持 `skins.cpp` 的 admission / loading 合同不变，并用现有 perf log 字段验证行为没有失真。

**Tech Stack:** C++, GoogleTest, settings resource jobs helpers, existing perf telemetry, CMake Windows gate.

---

## Scope

本计划只做 `settings:tee / Work Drain` 主线优化，不重建 `qmclient_scripts/perf`，不做 FBO cleanup 实现，不把 `server_browser / list_frame`、Demo Browser 异步化、Assets drain 或统一 scheduler 重构塞进来。

## Source Context

**Authoritative docs:**

- `docs/superpowers/specs/2026-06-08-页面性能优化框架设计.md`
- `docs/superpowers/explore/2026-06-10-页面性能框架阶段路径校准.md`
- `docs/superpowers/explore/2026-06-09-性能量化固定场景.md`

**Current implementation anchors:**

- `src/game/client/components/menus_settings.cpp`
- `src/game/client/components/settings_resource_jobs.cpp`
- `src/game/client/components/settings_resource_jobs.h`
- `src/game/client/components/skins.cpp`
- `src/test/settings_warmup_test.cpp`
- `src/test/skins_test.cpp`

## File Structure

- Modify `src/game/client/components/settings_resource_jobs.h`
  - Keep the budget-decision input/output contract focused on backlog gating and stop reasons.
- Modify `src/game/client/components/settings_resource_jobs.cpp`
  - Tighten idle-drain background request budget and add a hard backlog cap so healthy progress no longer authorizes unbounded prefetch backlog growth.
- Modify `src/game/client/components/skins.cpp`
  - Keep source admission / loading behavior aligned with the new budget contract; no new queue type, no new scheduler.
- Modify `src/test/settings_warmup_test.cpp`
  - Add deterministic helper-level tests for the new backlog cap and idle-drain tuning.
- Modify `src/test/skins_test.cpp`
  - Keep source-contract coverage around `menus_settings.cpp` / `skins.cpp` logging and queue-state glue.

---

### Task 1: Lock The Source-Cooling Contract In Tests

**Files:**
- Modify: `src/test/settings_warmup_test.cpp`
- Modify: `src/test/skins_test.cpp`

- [ ] **Step 1: Write a failing helper test for healthy-progress backlog capping**

Add this test near the existing `TeeBackgroundRequestBudget*` coverage:

```cpp
TEST(SettingsResourceJobs, TeeBackgroundRequestBudgetCapsHealthyBacklogBeforeQueueInflates)
{
	SSettingsSkinBackgroundRequestBudgetInput Input;
	Input.m_DefaultBudget = 24;
	Input.m_Pending = 8;
	Input.m_Loading = 8;
	Input.m_BackgroundRequested = 256;
	Input.m_CountFuseLimit = 128;
	Input.m_VisibleReserve = 0;
	Input.m_RecentLoadedDelta = 4;
	Input.m_RecentAdmittedDelta = 4;
	Input.m_DrainActive = true;

	const auto Decision = SettingsSkinBackgroundRequestBudgetDecision(Input);
	EXPECT_EQ(Decision.m_RequestBudget, 0);
	EXPECT_EQ(Decision.m_BlockReason, ESettingsSkinBackgroundRequestBlockReason::STALL_BACKPRESSURE);
}
```

- [ ] **Step 2: Write a failing helper test for smaller idle-drain request budget**

Update the settled branch in `ThroughputControllerKeepsVisibleBacklogOutOfIdleDrain` so it expects a smaller background request budget:

```cpp
EXPECT_EQ(Settled.m_Mode, ESettingsSkinThroughputControllerMode::IDLE_DRAIN);
EXPECT_TRUE(Settled.m_BackgroundDrainActive);
EXPECT_EQ(Settled.m_BackgroundRequestBudget, 8);
```

If another test asserts the old `24`, update that test in the same edit so the suite expresses the new target consistently.

- [ ] **Step 3: Write a failing source-contract test for runtime glue staying intact**

Add this contract to `src/test/skins_test.cpp` near `TeeSettingsListEmitsRequestWindowPerfLogs`:

```cpp
EXPECT_NE(Source.find("const auto BackgroundBudgetDecision = SettingsSkinBackgroundRequestBudgetDecision({"), std::string::npos);
EXPECT_NE(Source.find("const int BackgroundRequestBudget = BackgroundBudgetDecision.m_RequestBudget;"), std::string::npos);
EXPECT_NE(Source.find("request_budget_block_reason=%s"), std::string::npos);
EXPECT_NE(Source.find("BackgroundDrainActive"), std::string::npos);
```

- [ ] **Step 4: Run the targeted C++ tests and confirm RED**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release\testrunner.exe --gtest_filter=SettingsResourceJobs.TeeBackgroundRequestBudgetCapsHealthyBacklogBeforeQueueInflates:SettingsResourceJobs.ThroughputControllerKeepsVisibleBacklogOutOfIdleDrain:Skins.TeeSettingsListEmitsRequestWindowPerfLogs
```

Expected: helper tests fail before the implementation patch because the current idle-drain budget is still too large and healthy progress still allows oversized backlog growth.

---

### Task 2: Tighten Idle-Drain Budget And Backlog Cap

**Files:**
- Modify: `src/game/client/components/settings_resource_jobs.cpp`
- Modify: `src/game/client/components/settings_resource_jobs.h`

- [ ] **Step 1: Lower the idle-drain request budget in the throughput profile**

In `SettingsSkinThroughputProfileForMode(...)`, change the `IDLE_DRAIN` profile from:

```cpp
Profile.m_BackgroundRequestBudget = 24;
```

to:

```cpp
Profile.m_BackgroundRequestBudget = 8;
```

Keep `SCROLL_ACTIVE` / `POST_SCROLL_RECOVERY` at `0`, and do not change unrelated upload/finalize/window bounds in the same step.

- [ ] **Step 2: Add a hard backlog cap to the budget decision**

Inside `SettingsSkinBackgroundRequestBudgetDecision(...)`, keep the existing real-inflight and visible-reserve checks, then add a hard cap before computing the final request budget:

```cpp
const int HardBacklogLimit = maximum(CountFuseLimit, maximum(Input.m_DefaultBudget, 1) * 8);
if(Input.m_BackgroundRequested >= HardBacklogLimit)
{
	Output.m_BlockReason = ESettingsSkinBackgroundRequestBlockReason::STALL_BACKPRESSURE;
	return Output;
}
```

After that, keep the softer stall branch for “large backlog + no recent progress”, but tighten it so both loaded and admitted deltas must be dry before reopening:

```cpp
const int BacklogHighWatermark = maximum(VisibleReserve * 8, CountFuseLimit * 2);
if(Input.m_BackgroundRequested >= BacklogHighWatermark &&
	Input.m_RecentLoadedDelta <= 0 &&
	Input.m_RecentAdmittedDelta <= 0)
{
	Output.m_BlockReason = ESettingsSkinBackgroundRequestBlockReason::STALL_BACKPRESSURE;
	return Output;
}
```

This keeps the stop-reason vocabulary stable while preventing “healthy progress” from authorizing thousands of queued background requests.

- [ ] **Step 3: Keep the header contract minimal**

If the implementation needs an inline helper, add it locally in `settings_resource_jobs.cpp`. Do not expand `settings_resource_jobs.h` with new public enums or telemetry shapes unless the `.cpp` patch truly cannot stay private.

- [ ] **Step 4: Run the targeted tests and confirm GREEN**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release\testrunner.exe --gtest_filter=SettingsResourceJobs.TeeBackgroundRequestBudgetCapsHealthyBacklogBeforeQueueInflates:SettingsResourceJobs.ThroughputControllerKeepsVisibleBacklogOutOfIdleDrain
```

Expected: both helper tests pass.

---

### Task 3: Keep `skins.cpp` And Telemetry Contracts Aligned

**Files:**
- Modify: `src/game/client/components/skins.cpp`
- Modify: `src/test/skins_test.cpp`

- [ ] **Step 1: Verify no extra production queue path is needed**

Keep `UpdateStartLoading(...)` in `skins.cpp` on the current contract:

```cpp
const auto Admission = DetermineAdmission(pSkinContainer, Priority);
if(!Admission.m_PromoteAllowed)
{
	str_copy(m_SettingsSourceAdmissionTelemetry.m_aLastWaitReason, Admission.m_pBlockReason, sizeof(m_SettingsSourceAdmissionTelemetry.m_aLastWaitReason));
	LogSettingsSkinSourceWaitEvent(...);
	return false;
}
```

The source-cooling change must come from “fewer background requests are issued”, not from introducing a second delayed queue or a new state machine branch.

- [ ] **Step 2: If a small code cleanup is needed, keep it local**

Only if the new budget cap exposes duplication or stale naming in the `menus_settings.cpp -> skins.cpp` handoff, do the smallest possible cleanup. Example acceptable shape:

```cpp
const auto BackgroundBudgetDecision = SettingsSkinBackgroundRequestBudgetDecision({...});
const int BackgroundRequestBudget = BackgroundBudgetDecision.m_RequestBudget;
```

Do not rewrite `RenderSettingsTee`, `UpdateStartLoading`, or `PrepareSettingsThroughputForFrame` into new helper layers just because this patch touches them.

- [ ] **Step 3: Re-run the source-contract test**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target testrunner -j 14
cmake-build-release\testrunner.exe --gtest_filter=Skins.TeeSettingsListEmitsRequestWindowPerfLogs
```

Expected: PASS, proving the telemetry handoff still exposes the same key request-window / work-drain fields.

---

### Task 4: Verification And Perf Gate

**Files:**
- No new source files beyond earlier tasks.

- [ ] **Step 1: Run the full C++ test target**

Run:

```powershell
qmclient_scripts/cmake-windows.cmd --build cmake-build-release --target run_cxx_tests -j 14
```

Expected: `run_cxx_tests` exits with code 0.

- [ ] **Step 2: Run the quick gate**

Run:

```powershell
python qmclient_scripts/gate/check_gate.py --mode quick
```

Expected: quick gate exits with code 0.

- [ ] **Step 3: Capture the perf follow-up gap explicitly**

This patch is not complete until the implementation note or final report records that the next perf re-check must use:

```text
PERF-SETTINGS-TEE-SWITCH
PERF-TEE-SCROLL
```

from `docs/superpowers/explore/2026-06-09-性能量化固定场景.md`, and confirms whether the top attribution count/backlog moved down from the `2026-06-09 05:35:31` baseline.

## Completion Criteria

The implementation is complete only when all are true:

1. `SettingsSkinBackgroundRequestBudgetDecision(...)` no longer permits unbounded `BACKGROUND_REQUESTED` backlog growth just because some loads are still completing.
2. `IDLE_DRAIN` no longer starts with the previous oversized background request budget.
3. `skins.cpp` still uses the same admission / wait-reason telemetry contract; no second queue type or scheduler rewrite was introduced.
4. Targeted helper tests, `run_cxx_tests`, and `check_gate --mode quick` pass.
5. Final report explicitly records that perf validation against `PERF-SETTINGS-TEE-SWITCH` / `PERF-TEE-SCROLL` is still required if a fresh client run was not performed in this session.

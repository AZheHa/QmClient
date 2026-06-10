import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

import { parseLine, parseLog, parseLogWithDiagnostics } from './lib/parse.ts';
import {
  compareOperationSignatures,
  operationSignature,
} from './lib/quality_core.ts';
import {
  reportQuality,
  summarizeForBundle,
} from './lib/quality.ts';
import { generateReport } from './lib/report.ts';
import {
  compareSessions,
  computeVerdict,
  isSamplingBiased,
  isFrameTimeEntry,
  isListFrameEvent,
  isPageSwitchEvent,
  isUiRebuildEvent,
  isWorkDrainEvent,
  pagePerformanceAttribution,
  snapshot,
} from './lib/stats.ts';

const FIXTURE_DIR = join(dirname(fileURLToPath(import.meta.url)), 'test');

function readFixture(name: string): string {
  return readFileSync(join(FIXTURE_DIR, name), 'utf-8');
}

function testParseKeepsEventOnlyPerfLines() {
  const line = '2026-06-04 12:00:00 I perf/interaction: event=scroll_begin frame=42 page=settings:tee visible_rows=8';
  const entry = parseLine(line);
  assert.ok(entry);
  assert.equal(entry.system, 'perf/interaction');
  assert.equal(entry.fields.event, 'scroll_begin');

  const entries = parseLog([
    line,
    '2026-06-04 12:00:01 I perf/skin-ux: event=first_visible_ready dur_ms=123.500 frame=45 page=settings:tee',
  ].join('\n'));
  assert.equal(entries.length, 2);
}

function testParseSupportsJsonLinesEvents() {
  const entry = parseLine('{"timestamp":"2026-06-04T12:00:02","system":"perf/device","event":"sample","frame":77,"gpu_util_percent":61.5}');
  assert.ok(entry);
  assert.equal(entry?.system, 'perf/device');
  assert.equal(entry?.fields.event, 'sample');
  assert.equal(entry?.fields.frame, 77);

  const prefixed = parseLine('2026-06-04 12:00:03 I perf/settings-invalidate: {"system":"perf/settings-invalidate","frame":88,"session":9,"reason":"config_hash_changed","text":1}');
  assert.ok(prefixed);
  assert.equal(prefixed?.system, 'perf/settings-invalidate');
  assert.equal(prefixed?.fields.reason, 'config_hash_changed');
  assert.equal(prefixed?.fields.frame, 88);
}

function testParseLogWithDiagnosticsCountsInvalidLines() {
  const result = parseLogWithDiagnostics([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    'not a perf line',
    '',
    '{"timestamp":"2026-06-04T12:00:02","system":"perf/device","event":"sample","frame":77}',
    '{broken json',
  ].join('\n'));

  assert.equal(result.entries.length, 2);
  assert.equal(result.diagnostics.totalLines, 4);
  assert.equal(result.diagnostics.invalidLines, 2);
}

function testReportIncludesInteractionAndDeviceSections() {
  const entries = parseLog(readFixture('sample.log'));
  const html = generateReport(entries, 'sample.log', null);
  assert.match(html, /交互窗口/);
  assert.match(html, /Tee 收敛/);
  assert.match(html, /设备资源/);
  assert.match(html, /页面性能归因/);
  assert.match(html, /Section Top-10/);
}

function testReportAttributesPagePerformanceEvents() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=page_switch page=settings from=general to=tee dur_ms=12.500 frame=10',
    '2026-06-04 12:00:01 I perf/interaction: event=list_frame page=server_browser items_total=1200 rows_visible=18 rows_processed=18 rows_skipped=1182 dur_ms=5.250 frame=11',
    '2026-06-04 12:00:02 I perf/section: event=section page=settings:tee section=identity dur_ms=4.750 visible=1 dirty=config text_new=2 text_reused=8 frame=12',
    '2026-06-04 12:00:03 I perf/settings-resource: event=work_drain page=settings:tee kind=upload count=4 bytes=8192 dur_ms=9.000 stop=budget frame=13',
    '2026-06-04 12:00:04 I perf/skin-ux: event=list_drain_summary page=settings:tee dur_ms=18.000 requested=5 pending=2 loading=1 loaded=10 frame=14',
  ].join('\n'));
  const html = generateReport(entries, 'qm_perf_attribution.log', null);
  assert.match(html, /页面性能归因/);
  assert.doesNotMatch(html, /Page Switch/);
  assert.match(html, /page_switch/);
  assert.match(html, /List Interaction/);
  assert.match(html, /UI Rebuild/);
  assert.match(html, /Work Drain/);
  assert.match(html, /server_browser/);
  assert.match(html, /rows_processed/);
  assert.match(html, /stop=budget/);
  assert.match(html, /kind=merge/);
  assert.match(html, /requested=5 pending=2 loading=1 loaded=10/);
}

function testServerBrowserListFrameAttributionUsesRowCounts() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=list_frame page=server_browser items_total=1200 rows_visible=18 rows_rendered=18 rows_iterated=1200 rows_skipped=1182 dur_ms=5.250 frame=11 source=server_browser',
  ].join('\n'));

  const attribution = pagePerformanceAttribution(entries);

  assert.equal(attribution.length, 1);
  assert.equal(attribution[0].kind, 'List Interaction');
  assert.match(attribution[0].summary, /rows_visible=18/);
  assert.match(attribution[0].details, /rows_rendered=18/);
  assert.match(attribution[0].details, /rows_iterated=1200/);
  assert.match(attribution[0].details, /rows_skipped=1182/);
}

function testQmUiRuntimeAttributionUsesDedicatedKind() {
  const entries = parseLog([
    '2026-06-10 12:00:00 I perf/ui_runtime: {"system":"perf/ui_runtime","frame":"1","session":"7","page":"settings:qmclient","event":"ui_runtime","operation":"settings_qmclient","nodes":"120","anim_ms":"0.200","active_anims":"3","queued_anims":"1","render_bridge_ms":"0.050","duration_ms":"0.400"}',
  ].join('\n'));

  const attribution = pagePerformanceAttribution(entries);

  assert.equal(attribution.length, 1);
  assert.equal(attribution[0].kind, 'UI Runtime');
  assert.equal(attribution[0].page, 'settings:qmclient');
  assert.match(attribution[0].summary, /operation=settings_qmclient/);
}

function testPerfEventClassifiersKeepBoundariesTight() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/interaction: event=page_switch page=settings from=general to=tee dur_ms=12.500 frame=11',
    '2026-06-04 12:00:02 I perf/interaction: event=list_frame page=server_browser dur_ms=5.250 frame=12',
    '2026-06-04 12:00:03 I perf/section: event=section page=settings:tee section=identity dur_ms=4.750 frame=13',
    '2026-06-04 12:00:04 I perf/interaction: event=section page=settings:tee section=not-a-section dur_ms=99.000 frame=14',
    '2026-06-04 12:00:05 I perf/settings-resource: event=work_drain page=settings:tee dur_ms=9.000 frame=15',
    '2026-06-04 12:00:06 I perf/device: event=sample frame=16 gpu_util_percent=61.5',
  ].join('\n'));

  assert.equal(isFrameTimeEntry(entries[0]), true);
  assert.equal(isPageSwitchEvent(entries[1]), true);
  assert.equal(isListFrameEvent(entries[2]), true);
  assert.equal(isUiRebuildEvent(entries[3]), true);
  assert.equal(isUiRebuildEvent(entries[4]), false);
  assert.equal(isWorkDrainEvent(entries[5]), true);
  assert.equal(isFrameTimeEntry(entries[6]), false);
}

function testPageSwitchBoundaryDoesNotEnterDurationAttribution() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=page_switch page=settings from=general to=tee dur_ms=0.000 frame=10',
    '2026-06-04 12:00:01 I perf/section: event=section page=settings:tee section=identity dur_ms=4.750 visible=1 dirty=config text_new=2 text_reused=8 frame=11',
  ].join('\n'));

  const attribution = pagePerformanceAttribution(entries);

  assert.equal(attribution.length, 1);
  assert.equal(attribution[0].kind, 'UI Rebuild');
  assert.equal(attribution[0].durationMs, 4.75);
}

function testSnapshotIgnoresEventOnlyTelemetry() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/interaction: event=tee_enter frame=10 page=settings:tee visible_rows=8 first_visible_skin=default',
    '2026-06-04 12:00:02 I perf/device: event=sample frame=12 gpu_util_percent=61.5 cpu_process_percent=12 memory_process_mb=1024',
    '2026-06-04 12:00:03 I perf/menu: stage=settings_page_content duration_ms=10.000 frame=13 page=settings:tee',
  ].join('\n'));

  const current = snapshot(entries, 'qm_perf_current.log');

  assert.equal(current.totalFrames, 2);
  assert.equal(current.percentiles.min, 6);
  assert.equal(current.percentiles.p50, 6);
  assert.equal(current.percentiles.p95, 10);
  assert.equal(current.compliance.h240, 0);
}

function testComparisonReportWarnsAboutAutomaticBaseline() {
  const previousEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=8.000 frame=10 page=settings:tee',
  ].join('\n'));
  const currentEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));
  const comparison = compareSessions(
    snapshot(previousEntries, 'qm_perf_previous.log'),
    snapshot(currentEntries, 'qm_perf_current.log'),
  );

  const html = generateReport(currentEntries, 'qm_perf_current.log', comparison);

  assert.match(html, /自动选择的上一份日志/);
  assert.match(html, /不作为严格回归判定/);
}

function testComparisonReportMarksDifferentOperationsAsAdvisory() {
  const previousEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=demo_browser duration_ms=8.000 frame=10 page=demo_browser',
  ].join('\n'));
  const currentEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));
  const comparison = compareSessions(
    snapshot(previousEntries, 'qm_perf_previous.log'),
    snapshot(currentEntries, 'qm_perf_current.log'),
  );

  const html = generateReport(currentEntries, 'qm_perf_current.log', comparison);

  assert.equal(comparison.operation.comparable, false);
  assert.match(html, /advisory only/i);
  assert.match(html, /page set differs/i);
}

function testOperationCompatibilityChecksEventsAndStages() {
  const previousEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/interaction: event=page_switch page=settings:tee dur_ms=4.000 frame=11',
  ].join('\n'));
  const currentEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_render_total duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/interaction: event=list_frame page=settings:tee dur_ms=4.000 frame=11',
  ].join('\n'));

  const result = compareOperationSignatures(operationSignature(previousEntries), operationSignature(currentEntries));

  assert.equal(result.comparable, false);
  assert.match(result.reason, /event set differs|stage set differs/i);
}

function testReportQualityExplainsMissingAndBiasedData() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=10.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/menu: stage=settings_page_content duration_ms=11.000 frame=11 page=settings:tee',
    '2026-06-04 12:00:02 I perf/menu: stage=settings_page_content duration_ms=12.000 frame=12 page=settings:tee',
    '2026-06-04 12:00:03 I perf/menu: stage=settings_page_content duration_ms=13.000 frame=13 page=settings:tee',
    '2026-06-04 12:00:04 I perf/menu: stage=settings_page_content duration_ms=14.000 frame=14 page=settings:tee',
    '2026-06-04 12:00:05 I perf/menu: stage=settings_page_content duration_ms=15.000 frame=15 page=settings:tee',
    '2026-06-04 12:00:06 I perf/menu: stage=settings_page_content duration_ms=16.000 frame=16 page=settings:tee',
    '2026-06-04 12:00:07 I perf/menu: stage=settings_page_content duration_ms=17.000 frame=17 page=settings:tee',
    '2026-06-04 12:00:08 I perf/menu: stage=settings_page_content duration_ms=18.000 frame=18 page=settings:tee',
    '2026-06-04 12:00:09 I perf/menu: stage=settings_page_content duration_ms=19.000 frame=19 page=settings:tee',
  ].join('\n'));

  const quality = reportQuality(entries, { invalidLines: 2, totalLines: 12 });

  assert.equal(quality.sampleCount, 10);
  assert.equal(quality.invalidLines, 2);
  assert.equal(quality.biased, true);
  assert.match(quality.warnings.join('\n'), /sampling threshold/i);
  assert.match(quality.warnings.join('\n'), /invalid lines/i);
  assert.deepEqual(quality.operation.pages, ['settings:tee']);
}

function testOperationCompatibilityIsExplicit() {
  const settingsEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));
  const demoEntries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=demo_browser duration_ms=9.000 frame=10 page=demo_browser',
  ].join('\n'));

  const result = compareOperationSignatures(operationSignature(settingsEntries), operationSignature(demoEntries));

  assert.equal(result.comparable, false);
  assert.match(result.reason, /page set differs/i);
}

function testBundleSummaryIsStableJsonShape() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/section: event=section page=settings:tee section=identity dur_ms=4.000 visible=1 dirty=unknown text_new=0 text_reused=0 frame=11',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_current.log', { invalidLines: 0, totalLines: 2 });

  assert.equal(summary.sourceFile, 'qm_perf_current.log');
  assert.equal(summary.quality.sampleCount, 1);
  assert.deepEqual(summary.quality.operation.pages, ['settings:tee']);
  assert.equal(summary.attribution.top.length, 1);
  assert.equal(summary.attribution.top[0].kind, 'UI Rebuild');
  assert.equal(typeof summary.generatedAt, 'string');
}

function testSamplingBiasReportUsesP5Estimate() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=1.000 frame=10 page=settings:tee',
    ...Array.from({ length: 19 }, (_, i) =>
      `2026-06-04 12:00:${String(i + 1).padStart(2, '0')} I perf/menu: stage=settings_page_content duration_ms=10.000 frame=${i + 11} page=settings:tee`),
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_sampling_bias.log', null);

  assert.match(html, /p5=10\.0ms/);
  assert.doesNotMatch(html, /当前采样阈值为 1\.0ms/);
  assert.doesNotMatch(html, /当前采样阈值 1\.0ms/);
}

function testSamplingBiasUsesDefaultLoggingThreshold() {
  assert.equal(isSamplingBiased(Array(20).fill(4.1)), true);
}

function testKpiThresholdsAlignWithVerdictBudget() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=16.500 frame=10 page=settings:tee',
  ].join('\n'));
  const html = generateReport(entries, 'qm_perf_thresholds.log', null);

  assert.equal(computeVerdict(snapshot(entries, 'qm_perf_thresholds.log').percentiles, 0), 'PASS');
  assert.match(html, /<div class="kpi-label">p99<\/div><div class="kpi-value ok">16\.5<\/div>/);
  assert.match(html, /<div class="kpi-label">Max<\/div><div class="kpi-value ok">16\.5<\/div>/);
}

function testReportIncludesSectionTop10() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/section: event=section page=settings:tee section=identity dur_ms=4.000 visible=1 dirty=unknown text_new=0 text_reused=0 frame=10',
    '2026-06-04 12:00:01 I perf/section: event=section page=settings:tee section=identity dur_ms=12.000 visible=1 dirty=unknown text_new=0 text_reused=0 frame=11',
    '2026-06-04 12:00:02 I perf/section: event=section page=settings:qmclient section=theme dur_ms=7.000 visible=1 dirty=unknown text_new=0 text_reused=0 frame=12',
    '2026-06-04 12:00:03 I perf/interaction: event=section page=settings:tee section=not-a-section dur_ms=99.000 frame=13',
    '2026-06-04 12:00:04 I perf/menu: stage=settings_page_content duration_ms=8.000 frame=14 page=settings:tee',
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_sections.log', null);

  assert.match(html, /Section Top-10/);
  assert.match(html, /settings:tee/);
  assert.match(html, /identity/);
  assert.match(html, /12\.000ms/);
  assert.doesNotMatch(html, /not-a-section/);
}

function testReportShowsQualityAndUnavailableData() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=tee_enter frame=10 page=settings:tee visible_rows=8',
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_empty_frame_data.log', null, {
    totalLines: 2,
    invalidLines: 1,
  });

  assert.match(html, /样本可信度/);
  assert.match(html, /N\/A/);
  assert.match(html, /WARN/);
  assert.doesNotMatch(html, /<span class="verdict-banner ok">PASS<\/span>/);
  assert.doesNotMatch(html, /性能表现良好/);
  assert.match(html, /没有可用的 frame-time 样本/);
  assert.match(html, /no frame-time samples/i);
  assert.match(html, /invalid lines/i);
}

function testReportCoreKpisUseFrameTimeSamplesConsistently() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=1.000 frame=10 page=settings:tee',
    '2026-06-04 12:00:01 I perf/gameclient: stage=render_frame duration_ms=20.000 frame=11 page=settings:tee',
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_global_frame.log', null);

  assert.match(html, /<div class="kpi-label">p99<\/div><div class="kpi-value warn">20\.0<\/div>/);
  assert.match(html, /最大尖峰耗时 20\.0ms/);
}

function testReportTooltipsCanFloatOutsideCharts() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));

  const html = generateReport(entries, 'qm_perf_tooltip.log', null);

  assert.match(html, /\.figure \.chart-wrap\{[^}]*overflow:visible/);
  assert.match(html, /const tooltipPosition = /);
  assert.match(html, /position: tooltipPosition/);
  assert.match(html, /pointer-events:none/);
}

function testSummaryJsonCanBeSerializedForDebugBundle() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/menu: stage=settings_page_content duration_ms=6.000 frame=10 page=settings:tee',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'C:/tmp/qm_perf_current.log', { invalidLines: 0, totalLines: 1 });
  const parsed = JSON.parse(JSON.stringify(summary));

  assert.equal(parsed.sourceFile, 'qm_perf_current.log');
  assert.equal(parsed.quality.operation.pages[0], 'settings:tee');
  assert.equal(parsed.verdict, 'PASS');
}

function testSummaryJsonMarksUnavailableVerdictForEmptyFrameSamples() {
  const entries = parseLog([
    '2026-06-04 12:00:00 I perf/interaction: event=tee_enter frame=10 page=settings:tee visible_rows=8',
  ].join('\n'));

  const summary = summarizeForBundle(entries, 'qm_perf_empty.log', { invalidLines: 0, totalLines: 1 });
  const current = snapshot(entries, 'qm_perf_empty.log');

  assert.equal(summary.verdict, 'WARN');
  assert.equal(summary.verdictAvailable, false);
  assert.equal(current.verdict, 'WARN');
  assert.equal(current.totalFrames, 0);
  assert.match(summary.quality.warnings.join('\n'), /no frame-time samples/i);
}

function testAnalyzeWritesBundleAndArchiveSummaryFiles() {
  const source = readFileSync(new URL('./analyze.ts', import.meta.url), 'utf-8');

  assert.match(source, /parseLogWithDiagnostics/);
  assert.match(source, /perf_summary\.json/);
  assert.match(source, /\$\{logName\}_summary\.json/);
  assert.match(source, /summarizeForBundle/);
}

testParseKeepsEventOnlyPerfLines();
testParseSupportsJsonLinesEvents();
testParseLogWithDiagnosticsCountsInvalidLines();
testReportIncludesInteractionAndDeviceSections();
testReportAttributesPagePerformanceEvents();
testServerBrowserListFrameAttributionUsesRowCounts();
testQmUiRuntimeAttributionUsesDedicatedKind();
testPerfEventClassifiersKeepBoundariesTight();
testPageSwitchBoundaryDoesNotEnterDurationAttribution();
testSnapshotIgnoresEventOnlyTelemetry();
testComparisonReportWarnsAboutAutomaticBaseline();
testComparisonReportMarksDifferentOperationsAsAdvisory();
testOperationCompatibilityChecksEventsAndStages();
testReportQualityExplainsMissingAndBiasedData();
testOperationCompatibilityIsExplicit();
testBundleSummaryIsStableJsonShape();
testSamplingBiasReportUsesP5Estimate();
testSamplingBiasUsesDefaultLoggingThreshold();
testKpiThresholdsAlignWithVerdictBudget();
testReportIncludesSectionTop10();
testReportShowsQualityAndUnavailableData();
testReportCoreKpisUseFrameTimeSamplesConsistently();
testReportTooltipsCanFloatOutsideCharts();
testSummaryJsonCanBeSerializedForDebugBundle();
testSummaryJsonMarksUnavailableVerdictForEmptyFrameSamples();
testAnalyzeWritesBundleAndArchiveSummaryFiles();

console.log('qmclient perf tests passed');

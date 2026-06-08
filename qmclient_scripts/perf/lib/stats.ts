// stats.ts — 统计算子（R-style 完整版）

import type { PerfEntry } from './parse.ts';
import { compareOperationSignatures, operationSignature, type OperationComparison, type OperationSignature } from './quality_core.ts';

export interface Percentiles {
  p10: number;
  p25: number;
  p50: number;
  p75: number;
  p90: number;
  p95: number;
  p99: number;
  max: number;
  min: number;
  avg: number;
  count: number;
  /** 标准差 */
  std: number;
  /** 四分位距 IQR = Q3 - Q1 */
  iqr: number;
}

/** 计算百分位（nearest-rank method） */
export function calcPercentiles(values: number[]): Percentiles {
  const sorted = [...values].sort((a, b) => a - b);
  const n = sorted.length;
  if (n === 0) return { p10:0, p25:0, p50:0, p75:0, p90:0, p95:0, p99:0, max:0, min:0, avg:0, count:0, std:0, iqr:0 };

  const p = (k: number) => sorted[Math.min(n - 1, Math.ceil((k / 100) * n) - 1)];
  const avg = sorted.reduce((a, b) => a + b, 0) / n;
  const variance = sorted.reduce((sum, v) => sum + (v - avg) ** 2, 0) / n;
  const std = Math.sqrt(variance);
  const q1 = p(25);
  const q3 = p(75);

  return {
    p10: p(10),
    p25: q1,
    p50: p(50),
    p75: q3,
    p90: p(90),
    p95: p(95),
    p99: p(99),
    max: sorted[n - 1],
    min: sorted[0],
    avg,
    count: n,
    std,
    iqr: q3 - q1,
  };
}

/** 计算帧预算合规率（百分比） */
export function complianceRate(values: number[], budgetMs: number): number {
  if (values.length === 0) return 100;
  const within = values.filter(v => v <= budgetMs).length;
  return (within / values.length) * 100;
}

/** 帧预算常量 */
export const BUDGET = {
  /** 240Hz → 4.17ms */
  h240: 4.17,
  /** 120Hz → 8.33ms */
  h120: 8.33,
  /** 60Hz → 16.67ms */
  h60: 16.67,
  /** 默认性能日志采样阈值 qm_perf_debug_threshold_ms */
  samplingDefault: 4,
  /** 2x 60Hz 帧预算，作为严重尖峰阈值 */
  h60Double: 33,
} as const;

/** 从日志数据推断采样阈值（取 p5，避开强制记录的极小样本） */
export function inferSamplingThreshold(durations: number[]): number {
  if (durations.length === 0) return 0;
  const sorted = [...durations].sort((a, b) => a - b);
  return sorted[Math.min(sorted.length - 1, Math.floor(sorted.length * 0.05))];
}

/** 判断数据是否受采样偏差影响 */
export function isSamplingBiased(durations: number[], thresholdMs: number = BUDGET.samplingDefault): boolean {
  if (durations.length === 0) return false;
  // 如果 p5 高于日志采样阈值，说明大量正常帧可能被过滤了。
  return inferSamplingThreshold(durations) > thresholdMs;
}

export interface FrameTimeSeries {
  times: string[];
  durations: number[];
}

export const PERF_SYSTEM = {
  MENU: 'perf/menu',
  GAMECLIENT: 'perf/gameclient',
  INTERACTION: 'perf/interaction',
  DEVICE: 'perf/device',
  SKIN_UX: 'perf/skin-ux',
  SECTION: 'perf/section',
} as const;

export const PERF_EVENT = {
  PAGE_SWITCH: 'page_switch',
  LIST_FRAME: 'list_frame',
  WORK_DRAIN: 'work_drain',
  LIST_DRAIN_SUMMARY: 'list_drain_summary',
} as const;

const FRAME_TIME_SYSTEMS: ReadonlySet<string> = new Set([PERF_SYSTEM.MENU, PERF_SYSTEM.GAMECLIENT]);

function rawField(e: PerfEntry, name: string): unknown {
  return (e.fields as Record<string, unknown>)[name];
}

export function entryDurationMs(e: PerfEntry): number | null {
  const Raw = rawField(e, 'duration_ms') ?? rawField(e, 'dur_ms') ?? rawField(e, 'dur');
  if (Raw === undefined || Raw === null || Raw === '') {
    return null;
  }
  const Parsed = Number(Raw);
  return Number.isFinite(Parsed) ? Parsed : null;
}

export function isFrameTimeEntry(e: PerfEntry): boolean {
  return FRAME_TIME_SYSTEMS.has(e.system) && entryDurationMs(e) !== null;
}

export function selectFrameTimeEntries(entries: PerfEntry[]): PerfEntry[] {
  return entries.filter(isFrameTimeEntry);
}

export function toTimeSeries(entries: PerfEntry[]): FrameTimeSeries {
  const sorted = [...entries].sort((a, b) => a.timestamp.localeCompare(b.timestamp));
  return {
    times: sorted.map(e => e.timestamp),
    durations: sorted.map(e => entryDurationMs(e) ?? e.durationMs),
  };
}

export interface SpikeInfo {
  index: number;
  timestamp: string;
  durationMs: number;
  stage: string;
  page: string;
  threshold: number;
}

export function detectSpikes(entries: PerfEntry[], thresholdMs: number = BUDGET.h60): SpikeInfo[] {
  return entries
    .map((e, i) => ({
      index: i,
      timestamp: e.timestamp,
      durationMs: entryDurationMs(e) ?? e.durationMs,
      stage: e.stage,
      page: e.fields.page ?? '',
      threshold: thresholdMs,
    }))
    .filter(s => s.durationMs > thresholdMs)
    .sort((a, b) => b.durationMs - a.durationMs);
}

export function histogram(values: number[], bucketEdges: number[]): { label: string; count: number }[] {
  const buckets = bucketEdges.map((edge, i) => ({
    label: i < bucketEdges.length - 1 ? `${edge}-${bucketEdges[i + 1]}ms` : `${edge}+ms`,
    min: edge,
    max: bucketEdges[i + 1] ?? Infinity,
    count: 0,
  }));
  for (const v of values) {
    for (const b of buckets) {
      if (v >= b.min && v < b.max) { b.count++; break; }
    }
  }
  return buckets;
}

/** 页面级统计 */
export interface PageStats {
  page: string;
  count: number;
  avg: number;
  min: number;
  max: number;
  p95: number;
  spikes: number;
  /** 箱线图五数: [min, Q1, median, Q3, max] */
  boxPlot: [number, number, number, number, number];
  /** 离群点 */
  outliers: number[];
}

export type AttributionKind = 'List Interaction' | 'UI Rebuild' | 'Work Drain';

export interface AttributionEntry {
  kind: AttributionKind;
  timestamp: string;
  page: string;
  durationMs: number;
  summary: string;
  details: string;
}

export interface SectionPerfStats {
  page: string;
  section: string;
  count: number;
  avg: number;
  p95: number;
  max: number;
}

function fieldDuration(e: PerfEntry): number {
  const Raw = e.fields.dur_ms ?? e.fields.duration_ms ?? e.fields.dur ?? String(e.durationMs);
  const Parsed = Number(Raw);
  return Number.isFinite(Parsed) ? Parsed : 0;
}

function field(e: PerfEntry, name: string, fallback = ''): string {
  const Value = e.fields[name];
  return Value === undefined || Value === null ? fallback : String(Value);
}

function eventName(e: PerfEntry): string {
  return field(e, 'event');
}

export function isPageSwitchEvent(e: PerfEntry): boolean {
  const event = eventName(e);
  return event === PERF_EVENT.PAGE_SWITCH || event.endsWith('_page_switch');
}

export function isListFrameEvent(e: PerfEntry): boolean {
  return eventName(e) === PERF_EVENT.LIST_FRAME;
}

export function isUiRebuildEvent(e: PerfEntry): boolean {
  return e.system === PERF_SYSTEM.SECTION;
}

export function isWorkDrainEvent(e: PerfEntry): boolean {
  const event = eventName(e);
  return event === PERF_EVENT.WORK_DRAIN || event === PERF_EVENT.LIST_DRAIN_SUMMARY;
}

function summaryKv(...pairs: [string, string][]): string {
  return pairs
    .filter(([, value]) => value.length > 0)
    .map(([key, value]) => `${key}=${value}`)
    .join(' ');
}

function makeEntry(
  e: PerfEntry,
  kind: AttributionKind,
  summary: string,
  details = '',
): AttributionEntry {
  return {
    kind,
    timestamp: e.timestamp,
    page: field(e, 'page', 'unknown'),
    durationMs: fieldDuration(e),
    summary,
    details,
  };
}

export function pagePerformanceAttribution(entries: PerfEntry[]): AttributionEntry[] {
  const attribution: AttributionEntry[] = [];
  for (const e of entries) {
    const event = eventName(e);
    if (isPageSwitchEvent(e)) {
      continue;
    } else if (isListFrameEvent(e)) {
      attribution.push(makeEntry(
        e,
        'List Interaction',
        summaryKv(
          ['items_total', field(e, 'items_total')],
          ['rows_visible', field(e, 'rows_visible')],
        ),
        summaryKv(
          ['rows_rendered', field(e, 'rows_rendered')],
          ['rows_iterated', field(e, 'rows_iterated')],
          ['rows_processed', field(e, 'rows_processed')],
          ['rows_skipped', field(e, 'rows_skipped')],
        ),
      ));
    } else if (isUiRebuildEvent(e)) {
      attribution.push(makeEntry(
        e,
        'UI Rebuild',
        summaryKv(
          ['section', field(e, 'section')],
          ['visible', field(e, 'visible')],
        ),
        summaryKv(
          ['dirty', field(e, 'dirty')],
          ['text_new', field(e, 'text_new')],
          ['text_reused', field(e, 'text_reused')],
        ),
      ));
    } else if (isWorkDrainEvent(e)) {
      attribution.push(makeEntry(
        e,
        'Work Drain',
        summaryKv(
          ['kind', field(e, 'kind', event === PERF_EVENT.LIST_DRAIN_SUMMARY ? 'merge' : '')],
          ['count', field(e, 'count')],
          ['bytes', field(e, 'bytes')],
          ['stop', field(e, 'stop')],
        ),
        summaryKv(
          ['source', field(e, 'source', event === PERF_EVENT.LIST_DRAIN_SUMMARY ? PERF_EVENT.LIST_DRAIN_SUMMARY : '')],
          ['requested', field(e, 'requested')],
          ['pending', field(e, 'pending')],
          ['loading', field(e, 'loading')],
          ['loaded', field(e, 'loaded')],
        ),
      ));
    }
  }
  return attribution.sort((a, b) => b.durationMs - a.durationMs);
}

export function sectionPerformanceTop(entries: PerfEntry[], limit: number = 10): SectionPerfStats[] {
  const bySection = new Map<string, { page: string; section: string; durations: number[] }>();
  for (const e of entries) {
    if (!isUiRebuildEvent(e)) {
      continue;
    }
    const page = field(e, 'page', 'unknown');
    const section = field(e, 'section', 'unknown');
    const key = `${page}\u0000${section}`;
    const bucket = bySection.get(key) ?? { page, section, durations: [] };
    bucket.durations.push(fieldDuration(e));
    bySection.set(key, bucket);
  }

  return [...bySection.values()]
    .map(({ page, section, durations }) => {
      const p = calcPercentiles(durations);
      return {
        page,
        section,
        count: p.count,
        avg: p.avg,
        p95: p.p95,
        max: p.max,
      };
    })
    .sort((a, b) => b.p95 - a.p95 || b.max - a.max || b.avg - a.avg)
    .slice(0, limit);
}

export function pageBreakdown(entries: PerfEntry[], spikeThreshold: number = BUDGET.h60): PageStats[] {
  const byPage = new Map<string, PerfEntry[]>();
  for (const e of entries) {
    const page = e.fields.page ?? e.fields.page_name ?? 'unknown';
    const list = byPage.get(page) ?? [];
    list.push(e);
    byPage.set(page, list);
  }

  const stats: PageStats[] = [];
  for (const [page, list] of byPage) {
    const durations = list.map(e => entryDurationMs(e) ?? e.durationMs);
    const p = calcPercentiles(durations);
    const bp = boxPlotStats(durations);
    stats.push({
      page,
      count: list.length,
      avg: p.avg,
      min: bp.whiskerLow,
      max: bp.whiskerHigh,
      p95: p.p95,
      spikes: durations.filter(d => d > spikeThreshold).length,
      boxPlot: [bp.whiskerLow, bp.q1, bp.median, bp.q3, bp.whiskerHigh],
      outliers: bp.outliers,
    });
  }
  return stats.sort((a, b) => b.avg - a.avg);
}

/** 箱线图统计 (Tukey) */
export interface BoxPlotResult {
  q1: number;
  median: number;
  q3: number;
  iqr: number;
  whiskerLow: number;
  whiskerHigh: number;
  outliers: number[];
}

export function boxPlotStats(values: number[]): BoxPlotResult {
  const sorted = [...values].sort((a, b) => a - b);
  const n = sorted.length;
  if (n === 0) return { q1: 0, median: 0, q3: 0, iqr: 0, whiskerLow: 0, whiskerHigh: 0, outliers: [] };

  const rank = (q: number) => sorted[Math.min(n - 1, Math.ceil((q / 100) * n) - 1)];
  const q1 = rank(25);
  const median = rank(50);
  const q3 = rank(75);
  const iqr = q3 - q1;
  const fenceLow = q1 - 1.5 * iqr;
  const fenceHigh = q3 + 1.5 * iqr;

  const inliers = sorted.filter(v => v >= fenceLow && v <= fenceHigh);
  const outliers = sorted.filter(v => v < fenceLow || v > fenceHigh);

  return {
    q1,
    median,
    q3,
    iqr,
    whiskerLow: inliers.length > 0 ? inliers[0] : sorted[0],
    whiskerHigh: inliers.length > 0 ? inliers[inliers.length - 1] : sorted[n - 1],
    outliers,
  };
}

/** 核密度估计 (Gaussian KDE, Silverman bandwidth) */
export function kde(values: number[], nPoints: number = 80): { x: number; y: number }[] {
  if (values.length === 0) return [];
  const n = values.length;
  const sorted = [...values].sort((a, b) => a - b);
  const mean = sorted.reduce((a, b) => a + b, 0) / n;
  const std = Math.sqrt(sorted.reduce((s, v) => s + (v - mean) ** 2, 0) / n);
  // Silverman's rule of thumb
  const h = std > 0 ? 1.06 * std * Math.pow(n, -0.2) : 1;

  const lo = Math.max(0, sorted[0] - 3 * h);
  const hi = sorted[n - 1] + 3 * h;
  const step = (hi - lo) / (nPoints - 1);

  const points: { x: number; y: number }[] = [];
  for (let i = 0; i < nPoints; i++) {
    const x = lo + i * step;
    let density = 0;
    for (const v of values) {
      const u = (x - v) / h;
      density += Math.exp(-0.5 * u * u);
    }
    density /= (n * h * Math.sqrt(2 * Math.PI));
    points.push({ x: parseFloat(x.toFixed(3)), y: parseFloat(density.toFixed(6)) });
  }
  return points;
}

/** QQ 图数据（相对正态分布） */
export function qqNorm(values: number[]): { theoretical: number; sample: number }[] {
  const n = values.length;
  if (n === 0) return [];
  const sorted = [...values].sort((a, b) => a - b);
  const mean = sorted.reduce((a, b) => a + b, 0) / n;
  const std = Math.sqrt(sorted.reduce((s, v) => s + (v - mean) ** 2, 0) / n);

  // 采样最多 500 个点，避免大数据量渲染缓慢
  const step = Math.max(1, Math.floor(n / 500));
  const result: { theoretical: number; sample: number }[] = [];
  for (let i = 0; i < n; i += step) {
    const p = (i + 0.5) / n;
    // 正态分布逆CDF近似 (Beasley-Springer-Moro)
    const z = normInv(p);
    result.push({ theoretical: mean + std * z, sample: sorted[i] });
  }
  return result;
}

/** 标准正态逆CDF近似 */
function normInv(p: number): number {
  // Rational approximation (Abramowitz & Stegun)
  if (p <= 0) return -4;
  if (p >= 1) return 4;
  const a = [-3.969683028665376e1, 2.209460984245205e2, -2.759285104469687e2, 1.383577518672690e2, -3.066479806614716e1, 2.506628277459239e0];
  const b = [-5.447609879822406e1, 1.615858368580409e2, -1.556989798598866e2, 6.680131188771972e1, -1.328068155288572e1];
  const c = [-7.784894002430293e-3, -3.223964580411365e-1, -2.400758277161838e0, -2.549732539343734e0, 4.374664141464968e0, 2.938163982698783e0];
  const d = [7.784695709041462e-3, 3.224671290700398e-1, 2.445134137142996e0, 3.754408661907416e0];

  const pLow = 0.02425, pHigh = 1 - pLow;
  let q, r;
  if (p < pLow) {
    q = Math.sqrt(-2 * Math.log(p));
    return (((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
  } else if (p <= pHigh) {
    q = p - 0.5; r = q * q;
    return (((((a[0]*r+a[1])*r+a[2])*r+a[3])*r+a[4])*r+a[5])*q / (((((b[0]*r+b[1])*r+b[2])*r+b[3])*r+b[4])*r+1);
  } else {
    q = Math.sqrt(-2 * Math.log(1 - p));
    return -(((((c[0]*q+c[1])*q+c[2])*q+c[3])*q+c[4])*q+c[5]) / ((((d[0]*q+d[1])*q+d[2])*q+d[3])*q+1);
  }
}

/** 整体性能判定 */
export type Verdict = 'PASS' | 'WARN' | 'FAIL';

export function computeVerdict(p: Percentiles, spikeCount: number): Verdict {
  if (p.p99 >= BUDGET.h60Double || spikeCount >= 5) return 'FAIL';
  if (p.p99 >= BUDGET.h60 || spikeCount >= 1) return 'WARN';
  return 'PASS';
}

/** 生成自动叙事文本 */
export function generateNarrative(p: Percentiles, spikes: SpikeInfo[], compliance240: number, compliance120: number, compliance60: number, biased: boolean, samplingThresholdMs: number = p.min): string {
  if (p.count === 0) {
    return '本次 session 没有可用的 frame-time 样本，无法判断渲染性能。请确认日志包含 perf/menu 或 perf/gameclient 的 duration_ms 数据后重新生成报告。';
  }

  const verdict = computeVerdict(p, spikes.length);
  const verdictText = verdict === 'PASS' ? '性能表现良好' : verdict === 'WARN' ? '存在轻微性能问题' : '存在显著性能问题';

  const lines: string[] = [];
  lines.push(`本次 session 共采集 ${p.count} 帧渲染数据，整体 ${verdictText}。`);

  if (biased) {
    lines.push(`注意：当前采样阈值估计 p5=${samplingThresholdMs.toFixed(1)}ms（当前默认 4ms），日志可能仅包含超过阈值的帧。实际合规率可能高于日志所示。建议确认 qm_perf_debug_threshold_ms 4 后重新采集完整帧分布。`);
  } else {
    lines.push(`帧预算合规率：240Hz (4.17ms) 为 ${compliance240.toFixed(1)}%，120Hz (8.33ms) 为 ${compliance120.toFixed(1)}%，60Hz (16.67ms) 为 ${compliance60.toFixed(1)}%。`);
  }

  if (spikes.length > 0) {
    const worst = spikes[0];
    lines.push(`检测到 ${spikes.length} 个性能尖峰（>16.67ms），最大尖峰耗时 ${worst.durationMs.toFixed(1)}ms（超标 ${(worst.durationMs / 16.67).toFixed(1)}x），出现在 ${worst.page || '未知'} 页面。`);
  } else {
    lines.push('未检测到超过 16.67ms 阈值的性能尖峰。');
  }

  lines.push(`中位数帧耗时 ${p.p50.toFixed(1)}ms；p99 达 ${p.p99.toFixed(1)}ms，反映尾部帧体验。`);

  return lines.join(' ');
}

// ── 会话对比 ─────────────────────────────────────

export interface SessionSnapshot {
  /** 来源日志文件名 */
  file: string;
  /** 百分位 */
  percentiles: Percentiles;
  /** 尖峰数 */
  spikeCount: number;
  /** 尖峰列表（Top-5） */
  topSpikes: SpikeInfo[];
  /** 帧预算合规率 */
  compliance: { h240: number; h120: number; h60: number };
  /** 判定 */
  verdict: Verdict;
  /** 总帧数 */
  totalFrames: number;
  /** 操作路径签名，用于判断两个日志是否可严格对比 */
  operation: OperationSignature;
}

/** 从 entries 生成会话快照 */
export function snapshot(entries: PerfEntry[], sourceFile: string): SessionSnapshot {
  const frameEntries = selectFrameTimeEntries(entries);
  const durations = frameEntries.map(e => entryDurationMs(e) ?? e.durationMs);
  const p = calcPercentiles(durations);
  const spikes = detectSpikes(frameEntries, BUDGET.h60);
  return {
    file: sourceFile,
    percentiles: p,
    spikeCount: spikes.length,
    topSpikes: spikes.slice(0, 5),
    compliance: {
      h240: complianceRate(durations, BUDGET.h240),
      h120: complianceRate(durations, BUDGET.h120),
      h60: complianceRate(durations, BUDGET.h60),
    },
    verdict: frameEntries.length === 0 ? 'WARN' : computeVerdict(p, spikes.length),
    totalFrames: frameEntries.length,
    operation: operationSignature(entries),
  };
}

export interface MetricDelta {
  /** 指标名 */
  name: string;
  /** 旧值 */
  before: number;
  /** 新值 */
  after: number;
  /** 差值 (after - before) */
  delta: number;
  /** 变化百分比 */
  changePercent: number;
  /** good/bad/neutral — 正数不一定好（spikes 增加 = 坏） */
  direction: 'better' | 'worse' | 'neutral';
}

export interface ComparisonResult {
  previous: SessionSnapshot;
  current: SessionSnapshot;
  /** 通用指标对比（p50/p95/p99/max/spikes） */
  metrics: MetricDelta[];
  /** 合规率对比 */
  compliance: MetricDelta[];
  /** 判定变化 */
  verdictChanged: boolean;
  /** 自动生成的对比叙事 */
  narrative: string;
  /** 对比是否来自同一类操作路径 */
  operation: OperationComparison;
}

function calcDelta(name: string, before: number, after: number, lowerIsBetter: boolean): MetricDelta {
  const delta = after - before;
  const changePercent = before !== 0 ? (delta / before) * 100 : 0;
  const direction = Math.abs(delta) < 0.001 ? 'neutral'
    : lowerIsBetter ? (delta < 0 ? 'better' : 'worse')
    : (delta > 0 ? 'better' : 'worse');
  return { name, before, after, delta, changePercent, direction };
}

/** 对比两个 session */
export function compareSessions(previous: SessionSnapshot, current: SessionSnapshot): ComparisonResult {
  const p = previous.percentiles;
  const c = current.percentiles;

  const metrics: MetricDelta[] = [
    calcDelta('p50', p.p50, c.p50, true),
    calcDelta('p95', p.p95, c.p95, true),
    calcDelta('p99', p.p99, c.p99, true),
    calcDelta('max', p.max, c.max, true),
    calcDelta('spikes', previous.spikeCount, current.spikeCount, true),
    calcDelta('mean', p.avg, c.avg, true),
    calcDelta('stddev', p.std, c.std, true),
  ];

  const compliance: MetricDelta[] = [
    calcDelta('240Hz 合规', previous.compliance.h240, current.compliance.h240, false),
    calcDelta('120Hz 合规', previous.compliance.h120, current.compliance.h120, false),
    calcDelta('60Hz 合规', previous.compliance.h60, current.compliance.h60, false),
  ];

  const verdictChanged = previous.verdict !== current.verdict;
  const operation = compareOperationSignatures(previous.operation, current.operation);
  const improvements = metrics.filter(m => m.direction === 'better').length;
  const regressions = metrics.filter(m => m.direction === 'worse').length;

  const lines: string[] = [];
  if (verdictChanged) {
    lines.push(`判定从 ${previous.verdict} 变为 ${current.verdict}。`);
  }
  if (improvements > 0) lines.push(`${improvements} 项指标改善。`);
  if (regressions > 0) lines.push(`${regressions} 项指标退化。`);

  // 最显著的变化
  const significant = [...metrics, ...compliance]
    .filter(m => m.direction !== 'neutral')
    .sort((a, b) => Math.abs(b.changePercent) - Math.abs(a.changePercent));

  if (significant.length > 0) {
    const top = significant[0];
    const trend = top.direction === 'better' ? '↓' : '↑';
    lines.push(`最显著变化：${top.name} ${top.before.toFixed(1)} → ${top.after.toFixed(1)}ms (${trend}${Math.abs(top.changePercent).toFixed(0)}%)。`);
  }

  const narrative = lines.join(' ');

  return { previous, current, metrics, compliance, verdictChanged, operation, narrative };
}

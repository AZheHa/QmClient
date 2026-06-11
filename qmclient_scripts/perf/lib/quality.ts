import { basename } from 'node:path';

import type { PerfEntry } from './parse.ts';
import { operationSignature, type OperationSignature } from './quality_core.ts';
import {
  BUDGET,
  calcPercentiles,
  detectSpikes,
  inferSamplingThreshold,
  isSamplingBiased,
  fpsSummaries,
  hasOnlineTargetSettingsFpsSummary,
  pagePerformanceAttribution,
  selectFrameTimeEntries,
  targetSettingsSnapshot,
  type AttributionEntry,
  type FpsSummary,
  type Percentiles,
  type Verdict,
  computeVerdict,
  entryDurationMs,
} from './stats.ts';

export interface ParseDiagnostics {
  totalLines: number;
  invalidLines: number;
}

export interface ReportQuality {
  sampleCount: number;
  totalEntries: number;
  totalLines: number;
  invalidLines: number;
  samplingThresholdMs: number;
  biased: boolean;
  operation: OperationSignature;
  warnings: string[];
}

export interface PerfBundleSummary {
  generatedAt: string;
  sourceFile: string;
  percentiles: Percentiles;
  verdict: Verdict;
  verdictAvailable: boolean;
  spikeCount: number;
  quality: ReportQuality;
  attribution: {
    top: AttributionEntry[];
  };
  fps: {
    available: boolean;
    summaries: FpsSummary[];
  };
  targetSettings: {
    verdict: Verdict;
    verdictAvailable: boolean;
    spikeCount: number;
    percentiles: Percentiles;
  };
}

export function reportQuality(entries: PerfEntry[], diagnostics: ParseDiagnostics): ReportQuality {
  const frameEntries = selectFrameTimeEntries(entries);
  const durations = frameEntries.map(e => entryDurationMs(e) ?? e.durationMs);
  const samplingThresholdMs = inferSamplingThreshold(durations);
  const biased = isSamplingBiased(durations, BUDGET.samplingDefault);
  const warnings: string[] = [];

  if (frameEntries.length === 0) {
    warnings.push('no frame-time samples; percentile and verdict data are unavailable');
  }
  if (diagnostics.invalidLines > 0) {
    warnings.push(`${diagnostics.invalidLines} invalid lines were ignored during parsing`);
  }
  if (biased) {
    warnings.push(`sampling threshold appears above default 4ms; p5=${samplingThresholdMs.toFixed(1)}ms`);
  }
  const fps = fpsSummaries(entries);
  if (fps.length === 0) {
    warnings.push('missing fps_summary; settings acceptance is incomplete');
  }
  if (!hasOnlineTargetSettingsFpsSummary(entries)) {
    warnings.push('missing ingame/online operation window; settings acceptance is incomplete');
  }

  return {
    sampleCount: frameEntries.length,
    totalEntries: entries.length,
    totalLines: diagnostics.totalLines,
    invalidLines: diagnostics.invalidLines,
    samplingThresholdMs,
    biased,
    operation: operationSignature(entries),
    warnings,
  };
}

export function summarizeForBundle(entries: PerfEntry[], sourceFile: string, diagnostics: ParseDiagnostics): PerfBundleSummary {
  const frameEntries = selectFrameTimeEntries(entries);
  const durations = frameEntries.map(e => entryDurationMs(e) ?? e.durationMs);
  const percentiles = calcPercentiles(durations);
  const spikes = detectSpikes(frameEntries, BUDGET.h60);
  const fps = fpsSummaries(entries);
  const targetSettings = targetSettingsSnapshot(entries);
  return {
    generatedAt: new Date().toISOString(),
    sourceFile: basename(sourceFile),
    percentiles,
    verdict: frameEntries.length === 0 ? 'WARN' : computeVerdict(percentiles, spikes.length),
    verdictAvailable: frameEntries.length > 0,
    spikeCount: spikes.length,
    quality: reportQuality(entries, diagnostics),
    attribution: {
      top: pagePerformanceAttribution(entries).slice(0, 10),
    },
    fps: {
      available: fps.length > 0,
      summaries: fps,
    },
    targetSettings,
  };
}

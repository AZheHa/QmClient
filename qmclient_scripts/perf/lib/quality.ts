import { basename } from 'node:path';

import type { PerfEntry } from './parse.ts';
import { operationSignature, type OperationSignature } from './quality_core.ts';
import {
  BUDGET,
  calcPercentiles,
  detectSpikes,
  inferSamplingThreshold,
  isSamplingBiased,
  pagePerformanceAttribution,
  selectFrameTimeEntries,
  type AttributionEntry,
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
  };
}

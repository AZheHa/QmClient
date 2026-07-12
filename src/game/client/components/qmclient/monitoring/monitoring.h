// 请抬头享受阳光｜日子很好 我很我---------致咩子
#ifndef GAME_CLIENT_COMPONENTS_QMCLIENT_MONITORING_MONITORING_H
#define GAME_CLIENT_COMPONENTS_QMCLIENT_MONITORING_MONITORING_H

#include <base/system.h>
#include <base/types.h>

#include <game/client/component.h>
#include <game/client/ui_rect.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>

enum class EQmConnectionGrade
{
	NORMAL,
	ELEVATED,
	SEVERE,
	DISCONNECTED,
};

enum class EQmDiagnosticCause
{
	NONE,
	DOWNSTREAM,
	UPSTREAM,
	JITTER,
	PACKET_LOSS,
	CLIENT_PERFORMANCE,
};

struct SQmNetworkMetrics
{
	struct STrafficStats
	{
		uint64_t m_Packets = 0;
		uint64_t m_PayloadBytes = 0;
		uint64_t m_OverheadBytes = 0;
		uint64_t m_TotalBytes = 0;
		uint64_t m_AveragePayloadBytes = 0;
		float m_RateKibPerSec = 0.0f;
	};

	float m_SnapshotLatencyMs = 0.0f;
	float m_PredictionLatencyMs = 0.0f;
	float m_PredictionMarginMs = 0.0f;
	float m_JitterMs = 0.0f;
	float m_GameTimeMarginMs = 0.0f;
	float m_ServerRollbackMs = 0.0f;
	float m_PacketLossPct = 0.0f;
	float m_ServerRollbackRatePct = 0.0f;
	float m_DownBytesPerSec = 0.0f;
	float m_UpBytesPerSec = 0.0f;
	STrafficStats m_Send;
	STrafficStats m_Recv;
	bool m_ConnectionProblems = false;
	bool m_Connected = false;
};

struct SQmPerformanceMetrics
{
	struct SGraphicsMemoryStats
	{
		uint64_t m_TextureKiB = 0;
		uint64_t m_BufferKiB = 0;
		uint64_t m_StreamedKiB = 0;
		uint64_t m_StagingKiB = 0;
	};

	float m_Fps = 0.0f;
	float m_FrameTimeMs = 0.0f;
	float m_FrameTimeSpikeMs = 0.0f;
	float m_FrameTimeUs = 0.0f;
	float m_CpuUsagePct = -1.0f;
	float m_TotalCpuUsagePct = -1.0f;
	float m_MemoryUsageMb = -1.0f;
	float m_GpuUtilPct = -1.0f;
	float m_GpuDedicatedVramMb = -1.0f;
	float m_GpuDedicatedVramBudgetMb = -1.0f;
	float m_GpuSharedVramMb = -1.0f;
	float m_DiskReadMbPerSec = -1.0f;
	float m_PredictionTimeMs = 0.0f;
	float m_PredictionStress = 0.0f;
	int m_GameTick = 0;
	int m_PredictedTick = 0;
	bool m_DeviceSampleAvailable = false;
	SGraphicsMemoryStats m_GraphicsMemory;
};

struct SQmDevicePerfSample
{
	float m_GpuUtilPct = -1.0f;
	float m_GpuDedicatedVramMb = -1.0f;
	float m_GpuDedicatedVramBudgetMb = -1.0f;
	float m_GpuSharedVramMb = -1.0f;
	float m_DiskReadMbPerSec = -1.0f;
	bool m_Available = false;
};

struct SQmDevicePerfSnapshot
{
	SQmDevicePerfSample m_Sample;
	uint64_t m_Version = 0;
};

class CQmDevicePerfSnapshotCache
{
public:
	SQmDevicePerfSnapshot Publish(const SQmDevicePerfSample &Sample);
	void Reset();
	SQmDevicePerfSnapshot Snapshot() const;

private:
	mutable std::mutex m_Mutex;
	SQmDevicePerfSnapshot m_Snapshot;
};

class CQmAsyncDevicePerfSampler
{
public:
	using FSampleOverride = std::function<SQmDevicePerfSample()>;

	explicit CQmAsyncDevicePerfSampler(FSampleOverride SampleOverride = {}, std::chrono::milliseconds PollInterval = std::chrono::milliseconds(100));
	~CQmAsyncDevicePerfSampler();

	void EnsureStarted();
	void SetEnabled(bool Enabled);
	void Stop();
	SQmDevicePerfSnapshot Snapshot() const;

private:
	void Run();

	FSampleOverride m_SampleOverride;
	std::chrono::milliseconds m_PollInterval;
	CQmDevicePerfSnapshotCache m_Cache;
	std::thread m_Thread;
	mutable std::mutex m_StateMutex;
	std::condition_variable m_StateCv;
	bool m_Started = false;
	bool m_Enabled = false;
	bool m_StopRequested = false;
};

void QmUpdateDevicePerfSamplerState(CQmAsyncDevicePerfSampler &Sampler, bool Enabled);

struct SQmDiagnosticVerdict
{
	EQmConnectionGrade m_Grade = EQmConnectionGrade::DISCONNECTED;
	EQmDiagnosticCause m_PrimaryCause = EQmDiagnosticCause::NONE;
	const char *m_pSummary = "";
	const char *m_pDetail = "";
};

struct SQmMonitoringSnapshot
{
	SQmNetworkMetrics m_Network;
	SQmPerformanceMetrics m_Performance;
	SQmDiagnosticVerdict m_Verdict;
};

struct SQmMonitoringHudLayout
{
	CUIRect m_PanelRect = {0.0f, 0.0f, 0.0f, 0.0f};
	CUIRect m_ContentRect = {0.0f, 0.0f, 0.0f, 0.0f};
};

struct SQmMonitoringBodyLayout
{
	float m_MainGraphHeight = 0.0f;
	float m_FpsGraphHeight = 0.0f;
	float m_PrimaryCardsHeight = 0.0f;
	float m_MetricsExtraHeight = 0.0f;
};

struct SQmHistoryStats
{
	float m_Current = 0.0f;
	float m_Average = 0.0f;
	float m_Min = 0.0f;
	float m_Max = 0.0f;
	bool m_HasData = false;
};

inline constexpr float QM_MONITORING_PANEL_PADDING = 12.0f;
inline constexpr float QM_MONITORING_HEADER_HEIGHT = 56.0f;
inline constexpr float QM_MONITORING_SECTION_GAP = 8.0f;
inline constexpr float QM_MONITORING_MAIN_GRAPH_HEIGHT = 280.0f;
inline constexpr float QM_MONITORING_FPS_GRAPH_HEIGHT = 180.0f;
inline constexpr float QM_MONITORING_PRIMARY_CARDS_HEIGHT = 112.0f;
inline constexpr float QM_MONITORING_SECONDARY_CARDS_HEIGHT = 96.0f;
inline constexpr int QM_MONITORING_HISTORY_CAPACITY = 180;

inline float QmComputeMonitoringUiScale(float ScreenWidth, float ScreenHeight)
{
	const float WidthScale = ScreenWidth / 1920.0f;
	const float HeightScale = ScreenHeight / 1080.0f;
	const float AreaScale = std::sqrt(std::max(WidthScale * HeightScale, 0.0f));
	return std::clamp(AreaScale, 0.65f, 1.8f);
}

inline float QmComputeRateKibPerSec(float BytesPerSec)
{
	return BytesPerSec <= 0.0f ? 0.0f : BytesPerSec / 1024.0f;
}

inline float QmComputeMonitoringPanelOpacity(int OpacityPercent)
{
	return std::clamp(OpacityPercent / 100.0f, 0.0f, 1.0f);
}

inline float QmNormalizeProcessCpuUsagePct(float RawCpuUsagePct, unsigned CpuCount)
{
	if(RawCpuUsagePct < 0.0f)
		return -1.0f;
	if(CpuCount == 0)
		return std::clamp(RawCpuUsagePct, 0.0f, 100.0f);
	return std::clamp(RawCpuUsagePct / (float)CpuCount, 0.0f, 100.0f);
}

inline float QmComputeTotalCpuUsagePct(uint64_t PrevIdle, uint64_t PrevTotal, uint64_t CurrentIdle, uint64_t CurrentTotal)
{
	if(PrevTotal == 0 || CurrentTotal <= PrevTotal || CurrentIdle < PrevIdle)
		return -1.0f;

	const uint64_t TotalDelta = CurrentTotal - PrevTotal;
	const uint64_t IdleDelta = CurrentIdle - PrevIdle;
	if(TotalDelta == 0)
		return -1.0f;
	return std::clamp((float)(TotalDelta - std::min(IdleDelta, TotalDelta)) * 100.0f / (float)TotalDelta, 0.0f, 100.0f);
}

inline float QmComputeRollbackMs(float GameTimeMarginMs)
{
	return GameTimeMarginMs < 0.0f ? -GameTimeMarginMs : 0.0f;
}

inline float QmComputeDiskReadMbPerSec(uint64_t PrevReadBytes, uint64_t PrevTickNs, uint64_t CurrentReadBytes, uint64_t CurrentTickNs)
{
	if(PrevTickNs == 0 || CurrentTickNs <= PrevTickNs || CurrentReadBytes < PrevReadBytes)
		return -1.0f;

	const uint64_t DeltaNs = CurrentTickNs - PrevTickNs;
	if(DeltaNs == 0)
		return -1.0f;

	const double DeltaBytes = (double)(CurrentReadBytes - PrevReadBytes);
	const double DeltaSeconds = (double)DeltaNs / 1000000000.0;
	return DeltaSeconds > 0.0 ? (float)(DeltaBytes / (1024.0 * 1024.0) / DeltaSeconds) : -1.0f;
}

inline SQmNetworkMetrics::STrafficStats QmComputeTrafficStats(const NETSTATS &Prev, const NETSTATS &Current)
{
	SQmNetworkMetrics::STrafficStats Stats;
	constexpr uint64_t OverheadSize = 14 + 20 + 8;

	if(Current.sent_packets < Prev.sent_packets || Current.sent_bytes < Prev.sent_bytes)
		return Stats;

	Stats.m_Packets = Current.sent_packets - Prev.sent_packets;
	Stats.m_PayloadBytes = Current.sent_bytes - Prev.sent_bytes;
	Stats.m_OverheadBytes = Stats.m_Packets * OverheadSize;
	Stats.m_TotalBytes = Stats.m_PayloadBytes + Stats.m_OverheadBytes;
	Stats.m_AveragePayloadBytes = Stats.m_Packets == 0 ? 0 : Stats.m_PayloadBytes / Stats.m_Packets;
	Stats.m_RateKibPerSec = (float)Stats.m_TotalBytes * 8.0f / 1024.0f;
	return Stats;
}

inline SQmNetworkMetrics::STrafficStats QmComputeTrafficStats(uint64_t PrevPackets, uint64_t PrevBytes, uint64_t CurrentPackets, uint64_t CurrentBytes)
{
	NETSTATS Prev = {};
	Prev.sent_packets = PrevPackets;
	Prev.sent_bytes = PrevBytes;
	NETSTATS Current = {};
	Current.sent_packets = CurrentPackets;
	Current.sent_bytes = CurrentBytes;
	return QmComputeTrafficStats(Prev, Current);
}

inline void FormatMetricValue(char *pBuf, int BufSize, const char *pUnit, float Value, int Precision = 0)
{
	if(Value < 0.0f)
	{
		str_copy(pBuf, "--", BufSize);
		return;
	}
	if(Precision <= 0)
		str_format(pBuf, BufSize, "%.0f%s", Value, pUnit);
	else
		str_format(pBuf, BufSize, "%.*f%s", Precision, Value, pUnit);
}

inline void FormatRateValue(char *pBuf, int BufSize, float BytesPerSec)
{
	if(BytesPerSec < 0.0f)
	{
		str_copy(pBuf, "--", BufSize);
		return;
	}

	const float KibPerSec = QmComputeRateKibPerSec(BytesPerSec);
	if(KibPerSec >= 1024.0f)
		str_format(pBuf, BufSize, "%.1fMiB/s", KibPerSec / 1024.0f);
	else
		str_format(pBuf, BufSize, "%.1fKiB/s", KibPerSec);
}

inline void FormatCpuRatioValue(char *pBuf, int BufSize, float ProcessCpuPct, float TotalCpuPct)
{
	if(ProcessCpuPct < 0.0f)
	{
		str_copy(pBuf, "--", BufSize);
		return;
	}
	if(TotalCpuPct < 0.0f)
	{
		str_format(pBuf, BufSize, "%.0f%%", ProcessCpuPct);
		return;
	}
	str_format(pBuf, BufSize, "%.0f%%/%.0f%%", ProcessCpuPct, TotalCpuPct);
}

template<size_t N>
inline SQmHistoryStats QmComputeHistoryStats(const std::array<float, N> &aHistory, int HistoryHead, int HistoryCount)
{
	SQmHistoryStats Stats;
	if(HistoryCount <= 0)
		return Stats;

	const int Start = (HistoryHead - HistoryCount + (int)aHistory.size()) % (int)aHistory.size();
	Stats.m_Current = aHistory[(Start + HistoryCount - 1) % (int)aHistory.size()];
	Stats.m_Min = Stats.m_Current;
	Stats.m_Max = Stats.m_Current;
	float Sum = 0.0f;
	for(int i = 0; i < HistoryCount; ++i)
	{
		const float Value = aHistory[(Start + i) % (int)aHistory.size()];
		Stats.m_Min = std::min(Stats.m_Min, Value);
		Stats.m_Max = std::max(Stats.m_Max, Value);
		Sum += Value;
	}
	Stats.m_Average = Sum / (float)HistoryCount;
	Stats.m_HasData = true;
	return Stats;
}

template<size_t N>
inline int QmFindLatestPeakIndex(const std::array<float, N> &aHistory, int HistoryHead, int HistoryCount)
{
	if(HistoryCount <= 0)
		return 0;

	const int Start = (HistoryHead - HistoryCount + (int)aHistory.size()) % (int)aHistory.size();
	float PeakValue = aHistory[Start];
	for(int i = 1; i < HistoryCount; ++i)
		PeakValue = std::max(PeakValue, aHistory[(Start + i) % (int)aHistory.size()]);

	const float Tolerance = std::max(0.05f, std::abs(PeakValue) * 0.005f);
	for(int i = HistoryCount - 1; i >= 0; --i)
	{
		const float Value = aHistory[(Start + i) % (int)aHistory.size()];
		if(Value >= PeakValue - Tolerance)
			return i;
	}
	return 0;
}

template<size_t N>
inline int QmFindLatestAbsolutePeakIndex(const std::array<float, N> &aHistory, int HistoryHead, int HistoryCount)
{
	if(HistoryCount <= 0)
		return 0;

	const int Start = (HistoryHead - HistoryCount + (int)aHistory.size()) % (int)aHistory.size();
	float PeakValue = std::abs(aHistory[Start]);
	for(int i = 1; i < HistoryCount; ++i)
		PeakValue = std::max(PeakValue, std::abs(aHistory[(Start + i) % (int)aHistory.size()]));

	const float Tolerance = std::max(0.05f, PeakValue * 0.005f);
	for(int i = HistoryCount - 1; i >= 0; --i)
	{
		const float Value = std::abs(aHistory[(Start + i) % (int)aHistory.size()]);
		if(Value >= PeakValue - Tolerance)
			return i;
	}
	return 0;
}

inline float QmComputeDiagnosticPacketLossPct(uint64_t SendPacketsDelta, int PendingResendCount)
{
	if(SendPacketsDelta == 0)
		return PendingResendCount > 0 ? 100.0f : 0.0f;
	return std::clamp(((float)std::max(PendingResendCount, 0) / std::max((float)SendPacketsDelta, 1.0f)) * 100.0f, 0.0f, 100.0f);
}

inline float QmComputeDiagnosticPacketLossPct(const NETSTATS &Prev, const NETSTATS &Current, int PendingResendCount)
{
	const uint64_t SendPacketsDelta = Current.sent_packets >= Prev.sent_packets ? Current.sent_packets - Prev.sent_packets : 0;
	return QmComputeDiagnosticPacketLossPct(SendPacketsDelta, PendingResendCount);
}

inline EQmConnectionGrade QmDetermineConnectionGrade(const SQmNetworkMetrics &Net)
{
	if(!Net.m_Connected)
		return EQmConnectionGrade::DISCONNECTED;
	if(Net.m_JitterMs >= 25.0f || Net.m_SnapshotLatencyMs >= 180.0f || Net.m_PredictionLatencyMs >= 180.0f || Net.m_ConnectionProblems)
		return EQmConnectionGrade::SEVERE;
	if((Net.m_PacketLossPct >= 5.0f && Net.m_ConnectionProblems) || Net.m_JitterMs >= 10.0f || Net.m_SnapshotLatencyMs >= 90.0f || Net.m_PredictionLatencyMs >= 90.0f)
		return EQmConnectionGrade::ELEVATED;
	return EQmConnectionGrade::NORMAL;
}

inline EQmDiagnosticCause QmDeterminePrimaryCause(const SQmNetworkMetrics &Net, const SQmPerformanceMetrics &Perf, EQmConnectionGrade Grade)
{
	if(Grade == EQmConnectionGrade::DISCONNECTED)
		return EQmDiagnosticCause::NONE;
	if(Net.m_PacketLossPct >= 5.0f)
		return EQmDiagnosticCause::PACKET_LOSS;
	if(Net.m_JitterMs >= 25.0f)
		return EQmDiagnosticCause::JITTER;
	if(Net.m_PacketLossPct >= 1.0f)
		return EQmDiagnosticCause::PACKET_LOSS;
	if(Net.m_JitterMs >= 10.0f)
		return EQmDiagnosticCause::JITTER;
	if(Net.m_SnapshotLatencyMs >= Net.m_PredictionLatencyMs + 20.0f)
		return EQmDiagnosticCause::DOWNSTREAM;
	if(Net.m_PredictionLatencyMs >= Net.m_SnapshotLatencyMs + 20.0f)
		return EQmDiagnosticCause::UPSTREAM;
	if(Grade == EQmConnectionGrade::NORMAL &&
		(Perf.m_FrameTimeMs > 16.7f || Perf.m_CpuUsagePct >= 75.0f || Perf.m_PredictionStress >= 12.0f))
		return EQmDiagnosticCause::CLIENT_PERFORMANCE;
	if(Net.m_ConnectionProblems)
		return EQmDiagnosticCause::DOWNSTREAM;
	return EQmDiagnosticCause::NONE;
}

inline SQmMonitoringHudLayout QmComputeMonitoringHudLayout(float ScreenWidth, float ScreenHeight, float GraphX, float GraphSpacing)
{
	SQmMonitoringHudLayout Layout;
	const float UiScale = QmComputeMonitoringUiScale(ScreenWidth, ScreenHeight);
	const float Padding = std::round(QM_MONITORING_PANEL_PADDING * UiScale);

	float PanelW = std::round(ScreenWidth * 0.48f);
	float PanelH = std::round(ScreenHeight * 0.66f);
	PanelW = std::max(PanelW, 760.0f * UiScale);
	PanelH = std::max(PanelH, 650.0f * UiScale);
	PanelW = std::min(PanelW, 1040.0f * UiScale);
	PanelH = std::min(PanelH, 1000.0f * UiScale);
	const float PreferredContentHeight = (QM_MONITORING_HEADER_HEIGHT +
						     QM_MONITORING_SECTION_GAP * 4.0f +
						     QM_MONITORING_MAIN_GRAPH_HEIGHT +
						     QM_MONITORING_FPS_GRAPH_HEIGHT +
						     QM_MONITORING_PRIMARY_CARDS_HEIGHT +
						     QM_MONITORING_SECONDARY_CARDS_HEIGHT) *
					     UiScale;
	PanelH = std::min(PanelH, std::round(PreferredContentHeight + Padding * 2.0f));
	PanelW = std::min(PanelW, ScreenWidth);
	PanelH = std::min(PanelH, ScreenHeight);

	Layout.m_PanelRect.w = PanelW;
	Layout.m_PanelRect.h = PanelH;
	Layout.m_PanelRect.x = std::clamp(GraphX - PanelW - GraphSpacing, 0.0f, std::max(ScreenWidth - PanelW, 0.0f));
	Layout.m_PanelRect.y = std::clamp(GraphSpacing * 2.0f, 0.0f, std::max(ScreenHeight - PanelH, 0.0f));

	Layout.m_ContentRect = Layout.m_PanelRect;
	Layout.m_ContentRect.x += Padding;
	Layout.m_ContentRect.y += Padding;
	Layout.m_ContentRect.w = std::max(0.0f, Layout.m_ContentRect.w - Padding * 2.0f);
	Layout.m_ContentRect.h = std::max(0.0f, Layout.m_ContentRect.h - Padding * 2.0f);
	return Layout;
}

inline SQmMonitoringBodyLayout QmComputeMonitoringBodyLayout(float ContentHeight, float UiScale)
{
	SQmMonitoringBodyLayout Layout;
	const float HeaderHeight = QM_MONITORING_HEADER_HEIGHT * UiScale;
	const float SectionGap = QM_MONITORING_SECTION_GAP * UiScale;
	const float MainGraphHeight = QM_MONITORING_MAIN_GRAPH_HEIGHT * UiScale;
	const float FpsGraphHeight = QM_MONITORING_FPS_GRAPH_HEIGHT * UiScale;
	const float PrimaryCardsHeight = QM_MONITORING_PRIMARY_CARDS_HEIGHT * UiScale;
	const float MetricsExtraHeight = QM_MONITORING_SECONDARY_CARDS_HEIGHT * UiScale;
	const float MainGraphMinHeight = 190.0f * UiScale;
	const float FpsGraphMinHeight = 120.0f * UiScale;
	const float AvailableBodyHeight = std::max(ContentHeight - HeaderHeight - SectionGap * 4.0f, 0.0f);
	if(AvailableBodyHeight <= 0.0f)
		return Layout;

	const float PreferredGraphTotal = MainGraphHeight + FpsGraphHeight;
	const float MinimumGraphTotal = MainGraphMinHeight + FpsGraphMinHeight;
	const float ReservedCardTotal = PrimaryCardsHeight + MetricsExtraHeight;

	Layout.m_PrimaryCardsHeight = PrimaryCardsHeight;
	Layout.m_MetricsExtraHeight = MetricsExtraHeight;

	if(AvailableBodyHeight >= ReservedCardTotal + PreferredGraphTotal)
	{
		Layout.m_MainGraphHeight = MainGraphHeight;
		Layout.m_FpsGraphHeight = FpsGraphHeight;
		return Layout;
	}

	const float AvailableGraphHeight = std::max(AvailableBodyHeight - ReservedCardTotal, 0.0f);
	if(AvailableGraphHeight >= MinimumGraphTotal && PreferredGraphTotal > MinimumGraphTotal)
	{
		const float GraphScale = (AvailableGraphHeight - MinimumGraphTotal) / (PreferredGraphTotal - MinimumGraphTotal);
		Layout.m_MainGraphHeight = MainGraphMinHeight + (MainGraphHeight - MainGraphMinHeight) * GraphScale;
		Layout.m_FpsGraphHeight = FpsGraphMinHeight + (FpsGraphHeight - FpsGraphMinHeight) * GraphScale;
		Layout.m_MetricsExtraHeight = std::max(AvailableBodyHeight - Layout.m_MainGraphHeight - Layout.m_FpsGraphHeight - Layout.m_PrimaryCardsHeight, 0.0f);
		return Layout;
	}

	const float FallbackTotal = MinimumGraphTotal + ReservedCardTotal;
	const float FallbackScale = FallbackTotal > 0.0f ? std::clamp(AvailableBodyHeight / FallbackTotal, 0.0f, 1.0f) : 0.0f;
	Layout.m_MainGraphHeight = MainGraphMinHeight * FallbackScale;
	Layout.m_FpsGraphHeight = FpsGraphMinHeight * FallbackScale;
	Layout.m_PrimaryCardsHeight = PrimaryCardsHeight * FallbackScale;
	Layout.m_MetricsExtraHeight = std::max(AvailableBodyHeight - Layout.m_MainGraphHeight - Layout.m_FpsGraphHeight - Layout.m_PrimaryCardsHeight, 0.0f);
	return Layout;
}

class CQmMonitoring : public CComponent
{
	SQmMonitoringSnapshot m_Snapshot;
	std::array<float, QM_MONITORING_HISTORY_CAPACITY> m_aPingHistory = {};
	std::array<float, QM_MONITORING_HISTORY_CAPACITY> m_aPredHistory = {};
	std::array<float, QM_MONITORING_HISTORY_CAPACITY> m_aPredictionMarginHistory = {};
	std::array<float, QM_MONITORING_HISTORY_CAPACITY> m_aJitterHistory = {};
	std::array<float, QM_MONITORING_HISTORY_CAPACITY> m_aGameTimeMarginHistory = {};
	std::array<float, QM_MONITORING_HISTORY_CAPACITY> m_aFpsHistory = {};
	int m_HistoryHead = 0;
	int m_HistoryCount = 0;
	int64_t m_LastSampleTick = 0;

	void ResetHistory();
	void UpdateNetworkMetrics(SQmNetworkMetrics &Net);
	void UpdatePerformanceMetrics(SQmPerformanceMetrics &Perf);
	void UpdateDiagnosticVerdict(SQmDiagnosticVerdict &Verdict, const SQmNetworkMetrics &Net, const SQmPerformanceMetrics &Perf);
	void PushHistorySample(float PingMs, float PredMs, float PredictionMarginMs, float JitterMs, float GameTimeMarginMs, float Fps);
	void RenderHeader(CUIRect Rect) const;
	void RenderMainGraph(CUIRect Rect) const;
	void RenderFpsGraph(CUIRect Rect) const;
	void RenderPrimaryCards(CUIRect Rect) const;
	void RenderDebugDetails(CUIRect Rect) const;

public:
	int Sizeof() const override { return sizeof(*this); }
	void OnInit() override;
	void OnShutdown() override;
	void OnStateChange(int NewState, int OldState) override;
	void OnRender() override;

	void UpdateSnapshot();
	void RenderHud(CUIRect View) const;

	const SQmMonitoringSnapshot &Snapshot() const { return m_Snapshot; }
};

#endif

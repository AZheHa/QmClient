#include "section_loader.h"

#include <base/perf_timer.h>
#include <base/system.h>

#include <engine/shared/config.h>
#include <engine/storage.h>

#include <game/client/components/qmclient/perf_logging.h>
#include <game/client/components/settings_runtime_cache.h>

#include <cstdlib>

static uint64_t ParseSessionCacheU64(const char *pValue)
{
	if(pValue == nullptr || pValue[0] == '\0')
		return 0;
#if defined(CONF_FAMILY_WINDOWS)
	return _strtoui64(pValue, nullptr, 10);
#else
	return strtoull(pValue, nullptr, 10);
#endif
}

CSectionLoader::CSectionLoader() = default;

CSectionLoader::~CSectionLoader()
{
	for(auto &Section : m_vSections)
		DestroyRenderTarget(Section);
}

bool CSectionLoader::IsVisibleSummarySectionName(const char *pName)
{
	return pName != nullptr &&
	       str_find(pName, "DeferredSummary") == nullptr &&
	       str_find(pName, "CompactSummary") == nullptr &&
	       str_find(pName, "SummaryBlock") == nullptr;
}

CUIRect CSectionLoader::MakeRenderTargetCacheRectForTests(float Width, float Height)
{
	return CUIRect{0.0f, 0.0f, Width, Height};
}

CUIRect CSectionLoader::MakeRenderTargetCacheRectForTests(float Width, float Height, float Padding)
{
	return CUIRect{Padding, Padding, Width, Height};
}

static void ClearSectionCallbacks(std::vector<SSettingsSection> &vSections)
{
	for(auto &Section : vSections)
	{
		Section.m_MeasureFn = nullptr;
		Section.m_RenderCompactFn = nullptr;
		Section.m_RenderFullFn = nullptr;
		Section.m_RenderStaticLayerFn = nullptr;
		Section.m_RenderInteractiveLayerFn = nullptr;
		Section.m_ShouldRenderInteractiveLayerFn = nullptr;
	}
}

void CSectionLoader::Register(std::vector<SSettingsSection> vSections)
{
	std::vector<bool> vTransferred(m_vSections.size(), false);
	for(auto &NewSection : vSections)
	{
		for(size_t OldIndex = 0; OldIndex < m_vSections.size(); ++OldIndex)
		{
			const auto &OldSection = m_vSections[OldIndex];
			if(str_comp(NewSection.m_pName, OldSection.m_pName) != 0)
				continue;

			NewSection.m_State = OldSection.m_State;
			NewSection.m_CachedHeight = OldSection.m_CachedHeight;
			NewSection.m_LastConfigHash = OldSection.m_LastConfigHash;
			NewSection.m_Dirty = OldSection.m_Dirty;
			NewSection.m_CacheValid = OldSection.m_CacheValid;
			NewSection.m_DirtyReason = OldSection.m_DirtyReason;
			NewSection.m_CacheRuntimeKey = OldSection.m_CacheRuntimeKey;
			NewSection.m_RenderTarget = OldSection.m_RenderTarget;
			NewSection.m_RenderTargetWidth = OldSection.m_RenderTargetWidth;
			NewSection.m_RenderTargetHeight = OldSection.m_RenderTargetHeight;
			vTransferred[OldIndex] = true;
			if(ComputeConfigHash(NewSection) != NewSection.m_LastConfigHash)
			{
				NewSection.m_Dirty = true;
				NewSection.m_CacheValid = false;
				NewSection.m_DirtyReason = ESettingsCacheDirtyReason::CONFIG;
			}
			else if(!(NewSection.m_CacheRuntimeKey == m_RuntimeKey))
			{
				NewSection.m_Dirty = true;
				NewSection.m_CacheValid = false;
				NewSection.m_DirtyReason = ESettingsCacheDirtyReason::WINDOW_SIZE;
			}
			break;
		}
	}
	for(size_t OldIndex = 0; OldIndex < m_vSections.size(); ++OldIndex)
	{
		if(!vTransferred[OldIndex])
			DestroyRenderTarget(m_vSections[OldIndex]);
	}
	m_vSections = std::move(vSections);
}

void CSectionLoader::SetGraphicsForCache(IGraphics *pGraphics)
{
	m_pGraphics = pGraphics;
}

void CSectionLoader::SetRuntimeKey(const SSettingsSectionCacheRuntimeKey &RuntimeKey)
{
	if(m_RuntimeKey == RuntimeKey)
		return;
	m_RuntimeKey = RuntimeKey;
	InvalidateCache(ESettingsCacheDirtyReason::WINDOW_SIZE);
}

void CSectionLoader::SetProgressiveEnabled(bool Enabled)
{
	m_ProgressiveEnabled = Enabled;
}

void CSectionLoader::SetLiveStaticCacheRecordingEnabled(bool Enabled)
{
	m_LiveStaticCacheRecordingEnabled = Enabled;
}

void CSectionLoader::SetRenderTargetSupportedForTests(bool Supported)
{
	m_RenderTargetSupportedForTests = Supported;
}

void CSectionLoader::MarkCacheValidForTests(const char *pName)
{
	for(auto &Section : m_vSections)
	{
		if(str_comp(Section.m_pName, pName) == 0)
		{
			Section.m_CacheValid = true;
			Section.m_Dirty = false;
			Section.m_DirtyReason = ESettingsCacheDirtyReason::NONE;
			Section.m_CacheRuntimeKey = m_RuntimeKey;
			return;
		}
	}
}

bool CSectionLoader::IsCacheValidForTests(const char *pName) const
{
	for(const auto &Section : m_vSections)
	{
		if(str_comp(Section.m_pName, pName) == 0)
			return Section.m_CacheValid;
	}
	return false;
}

const char *CSectionLoader::CacheDirtyReasonName(ESettingsCacheDirtyReason Reason)
{
	switch(Reason)
	{
	case ESettingsCacheDirtyReason::NONE: return "clean";
	case ESettingsCacheDirtyReason::CONFIG: return "config";
	case ESettingsCacheDirtyReason::LANGUAGE: return "language";
	case ESettingsCacheDirtyReason::WINDOW_SIZE: return "window_size";
	case ESettingsCacheDirtyReason::UI_SCALE: return "ui_scale";
	case ESettingsCacheDirtyReason::FONT: return "font";
	case ESettingsCacheDirtyReason::ACTIVE_INTERACTION: return "active_interaction";
	case ESettingsCacheDirtyReason::GRAPHICS_RESET: return "graphics_reset";
	case ESettingsCacheDirtyReason::UNKNOWN: return "unknown";
	}
	return "unknown";
}

void CSectionLoader::InvalidateSectionByName(const char *pName, ESettingsCacheDirtyReason Reason)
{
	for(auto &Section : m_vSections)
	{
		if(str_comp(Section.m_pName, pName) != 0)
			continue;
		Section.m_Dirty = true;
		Section.m_CacheValid = false;
		Section.m_DirtyReason = Reason;
		return;
	}
}

bool CSectionLoader::PrewarmSectionByName(const char *pName, CUIRect MainView, float ScrollY)
{
	if(!m_RenderTargetSupportedForTests)
	{
		ClearSectionCallbacks(m_vSections);
		return false;
	}

	m_MainView = MainView;
	m_ScrollY = ScrollY;
	CUIRect RunningColumn = MainView;
	for(SSettingsSection &Section : m_vSections)
	{
		if(Section.m_MeasureFn)
		{
			CUIRect MeasureColumn = RunningColumn;
			Section.m_CachedHeight = Section.m_MeasureFn(MeasureColumn);
		}
		if(Section.m_CachedHeight <= 0.0f)
			continue;

		const CUIRect SectionRect{RunningColumn.x, RunningColumn.y, RunningColumn.w, Section.m_CachedHeight};
		RunningColumn.y += Section.m_CachedHeight;
		RunningColumn.h = maximum(0.0f, RunningColumn.h - Section.m_CachedHeight);

		if(str_comp(Section.m_pName, pName) != 0)
			continue;
		if(!Section.m_CanCacheStaticLayer)
		{
			ClearSectionCallbacks(m_vSections);
			return false;
		}

		const int Priority = ComputeViewportPriority(SectionRect);
		if(Priority > 1)
		{
			ClearSectionCallbacks(m_vSections);
			return false;
		}

		const int Padding = std::max(0, (int)std::ceil(Section.m_StaticCachePadding));
		const int Width = std::max(1, (int)MainView.w + Padding * 2);
		const int Height = std::max(1, (int)Section.m_CachedHeight + Padding * 2);
		const bool Recorded = RecordStaticRenderTarget(Section, Width, Height);
		if(Recorded)
		{
			Section.m_LastConfigHash = ComputeConfigHash(Section);
			Section.m_CacheRuntimeKey = m_RuntimeKey;
			Section.m_Dirty = false;
			Section.m_CacheValid = true;
			Section.m_DirtyReason = ESettingsCacheDirtyReason::NONE;
		}
		ClearSectionCallbacks(m_vSections);
		return Recorded;
	}

	ClearSectionCallbacks(m_vSections);
	return false;
}

bool CSectionLoader::DrawCachedSectionByName(const char *pName, CUIRect MainView, float ScrollY, ESettingsCacheDirtyReason *pDirtyReason)
{
	m_MainView = MainView;
	m_ScrollY = ScrollY;
	m_RunningColumn = MainView;

	for(SSettingsSection &Section : m_vSections)
	{
		if(Section.m_MeasureFn)
		{
			CUIRect MeasureColumn = m_RunningColumn;
			Section.m_CachedHeight = Section.m_MeasureFn(MeasureColumn);
		}
		if(Section.m_CachedHeight <= 0.0f)
			continue;

		if(str_comp(Section.m_pName, pName) != 0)
		{
			m_RunningColumn.y += Section.m_CachedHeight;
			m_RunningColumn.h = maximum(0.0f, m_RunningColumn.h - Section.m_CachedHeight);
			continue;
		}

		const bool Drawn = TryRenderCachedSection(Section);
		if(pDirtyReason != nullptr)
			*pDirtyReason = Section.m_DirtyReason;
		ClearSectionCallbacks(m_vSections);
		return Drawn;
	}

	ClearSectionCallbacks(m_vSections);
	return false;
}

void CSectionLoader::Begin(CUIRect MainView, float TimeBudgetMs)
{
	m_MainView = MainView;
	m_BudgetPerFrameMs = (double)TimeBudgetMs;
	m_CurrentIndex = 0;

	m_Complete = false;
	m_TotalFrameTimeMs = 0.0;
}

bool CSectionLoader::Process()
{
	if(!m_Initialized)
	{
		for(auto &Section : m_vSections)
		{
			Section.m_State = ESettingsSectionState::UNINITIALIZED;
			Section.m_CachedHeight = 0.0f;
			Section.m_Dirty = !Section.m_CacheValid;
		}
		m_Initialized = true;
		m_CurrentIndex = 0;
	}

	m_RunningColumn = m_MainView;

	CPerfTimer FrameTimer;
	int UnlockedThisFrame = 0;
	const int MaxUnlockPerFrame = 2;

	while(m_CurrentIndex < (int)m_vSections.size())
	{
		SSettingsSection &Section = m_vSections[m_CurrentIndex];
		if(!m_ProgressiveEnabled && Section.m_State != ESettingsSectionState::FULL)
		{
			CUIRect MeasureColumn = m_RunningColumn;
			if(Section.m_MeasureFn)
				Section.m_CachedHeight = Section.m_MeasureFn(MeasureColumn);
			else
				Section.m_CachedHeight = 0.0f;
			Section.m_State = ESettingsSectionState::FULL;
			Section.m_LastConfigHash = ComputeConfigHash(Section);
			Section.m_Dirty = false;
		}
		const bool BudgetAvailable = FrameTimer.ElapsedMs() < m_BudgetPerFrameMs;

		switch(Section.m_State)
		{
		case ESettingsSectionState::UNINITIALIZED:
		{
			if(Section.m_MeasureFn)
				Section.m_CachedHeight = Section.m_MeasureFn(m_RunningColumn);
			else
				Section.m_CachedHeight = 0.0f;
			Section.m_State = ESettingsSectionState::MEASURING;
			++m_CurrentIndex;
			break;
		}
		case ESettingsSectionState::MEASURING:
		{
			const CUIRect SectionRect{m_MainView.x, m_RunningColumn.y, m_MainView.w, Section.m_CachedHeight};
			const int Priority = ComputeViewportPriority(SectionRect);
			if(Priority <= 1)
			{
				Section.m_State = ESettingsSectionState::COMPACT;
				if(Section.m_RenderCompactFn)
					Section.m_RenderCompactFn(m_RunningColumn);
				else
					m_RunningColumn.y += Section.m_CachedHeight;
			}
			else
			{
				m_RunningColumn.y += Section.m_CachedHeight;
			}
			++m_CurrentIndex;
			break;
		}
		case ESettingsSectionState::COMPACT:
		{
			const CUIRect SectionRect{m_MainView.x, m_RunningColumn.y, m_MainView.w, Section.m_CachedHeight};
			const int Priority = ComputeViewportPriority(SectionRect);
			if(BudgetAvailable && UnlockedThisFrame < MaxUnlockPerFrame && Priority <= 1)
			{
				Section.m_State = ESettingsSectionState::FULL;
				if(Section.m_RenderFullFn)
					Section.m_RenderFullFn(m_RunningColumn);
				else
					m_RunningColumn.y += Section.m_CachedHeight;
				Section.m_LastConfigHash = ComputeConfigHash(Section);
				Section.m_Dirty = false;
				++UnlockedThisFrame;
				++m_CurrentIndex;
				break;
			}
			if(Section.m_RenderCompactFn)
				Section.m_RenderCompactFn(m_RunningColumn);
			else
				m_RunningColumn.y += Section.m_CachedHeight;
			++m_CurrentIndex;
			break;
		}
		case ESettingsSectionState::FULL:
		{
			if(Section.m_Dirty && Section.m_MeasureFn)
			{
				CUIRect MeasureColumn = m_RunningColumn;
				Section.m_CachedHeight = Section.m_MeasureFn(MeasureColumn);
				Section.m_LastConfigHash = ComputeConfigHash(Section);
			}
			const CUIRect SectionRect{m_MainView.x, m_RunningColumn.y, m_MainView.w, Section.m_CachedHeight};
			if(ComputeViewportPriority(SectionRect) > 1)
			{
				m_RunningColumn.y += Section.m_CachedHeight;
				Section.m_Dirty = false;
				++m_CurrentIndex;
				break;
			}
			if(TryRenderCachedSection(Section))
			{
				if(Section.m_Dirty)
					Section.m_LastConfigHash = ComputeConfigHash(Section);
				Section.m_Dirty = false;
				++m_CurrentIndex;
				break;
			}
			if(Section.m_RenderFullFn)
				Section.m_CachedHeight = Section.m_RenderFullFn(m_RunningColumn);
			else
				m_RunningColumn.y += Section.m_CachedHeight;
			if(Section.m_Dirty)
				Section.m_LastConfigHash = ComputeConfigHash(Section);
			Section.m_Dirty = false;
			++m_CurrentIndex;
			break;
		}
		}
	}

	if(m_CurrentIndex >= (int)m_vSections.size())
	{
		m_Complete = true;
		for(const auto &Sect : m_vSections)
		{
			if(Sect.m_State != ESettingsSectionState::FULL)
			{
				m_Complete = false;
				m_CurrentIndex = 0;
				break;
			}
		}
	}

	m_TotalFrameTimeMs += FrameTimer.ElapsedMs();
	ClearSectionCallbacks(m_vSections);

	// Profiling: log when budget is exceeded and perf-debug is enabled
	if(g_Config.m_QmPerfDebug && m_TotalFrameTimeMs > 1.0)
	{
		char aPayload[256];
		str_format(aPayload, sizeof(aPayload),
			"stage=section_loader sections=%d budget_ms=%.1f actual_ms=%.1f complete=%d",
			(int)m_vSections.size(), m_BudgetPerFrameMs, m_TotalFrameTimeMs,
			m_Complete ? 1 : 0);
		QmPerfLogPayload("perf/section_loader", aPayload);
	}

	return !m_Complete;
}

bool CSectionLoader::IsComplete() const
{
	return m_Complete;
}

void CSectionLoader::Reset()
{
	m_Initialized = false;
	m_Complete = false;
	m_CurrentIndex = 0;
	m_TotalFrameTimeMs = 0.0;
	InvalidateCache(ESettingsCacheDirtyReason::CONFIG);
}

// -- Pre-warming --

bool CSectionLoader::Warmup(const SSessionUiCache *pCache, float TimeBudgetMs)
{
	if(!pCache || !pCache->m_Valid)
	{
		m_WarmupActive = false;
		ClearSectionCallbacks(m_vSections);
		return true;
	}

	if(!m_WarmupActive)
	{
		m_WarmupActive = true;
		m_WarmupIndex = 0;
		m_WarmupBudgetMs = TimeBudgetMs;
		m_pWarmupCache = pCache;
		for(auto &Section : m_vSections)
		{
			Section.m_State = ESettingsSectionState::UNINITIALIZED;
			Section.m_CachedHeight = 0.0f;
		}
	}

	CPerfTimer WarmupTimer;
	while(m_WarmupIndex < (int)m_vSections.size())
	{
		if(WarmupTimer.ElapsedMs() >= (double)m_WarmupBudgetMs)
			break;

		SSettingsSection &Section = m_vSections[m_WarmupIndex];

		const CUIRect SectionRect{m_MainView.x, m_MainView.y, m_MainView.w, Section.m_CachedHeight};
		const int Priority = ComputeViewportPriority(SectionRect);

		if(Priority > 1)
		{
			// Far from viewport: measure only
			if(Section.m_State == ESettingsSectionState::UNINITIALIZED)
			{
				if(Section.m_MeasureFn)
				{
					CUIRect MeasureRect = m_MainView;
					Section.m_CachedHeight = Section.m_MeasureFn(MeasureRect);
				}
				Section.m_State = ESettingsSectionState::MEASURING;
			}
			++m_WarmupIndex;
			continue;
		}

		// In or near viewport: render the registered real warmup path to populate glyphs/cache.
		const CPerfTimer SectTimer;
		if(Section.m_RenderCompactFn)
			Section.m_RenderCompactFn(m_MainView);
		Section.m_State = ESettingsSectionState::COMPACT;

		const double Elapsed = SectTimer.ElapsedMs();
		++m_WarmupIndex;

		if(Elapsed > 1.0)
			break; // expensive section; leave rest for next frame
	}

	if(m_WarmupIndex >= (int)m_vSections.size())
	{
		m_WarmupActive = false;
		ClearSectionCallbacks(m_vSections);
		return true;
	}
	ClearSectionCallbacks(m_vSections);
	return false;
}

bool CSectionLoader::IsWarmupComplete() const
{
	return !m_WarmupActive;
}

bool CSectionLoader::PrewarmStaticRenderTargets(CUIRect MainView, float ScrollY, float TimeBudgetMs, bool IncludeFarSections)
{
	if(!m_RenderTargetSupportedForTests)
	{
		ClearSectionCallbacks(m_vSections);
		return true;
	}

	m_MainView = MainView;
	m_ScrollY = ScrollY;
	CUIRect RunningColumn = MainView;
	CPerfTimer WarmupTimer;
	for(SSettingsSection &Section : m_vSections)
	{
		if(WarmupTimer.ElapsedMs() >= (double)TimeBudgetMs)
		{
			ClearSectionCallbacks(m_vSections);
			return false;
		}
		if(Section.m_MeasureFn)
		{
			CUIRect MeasureColumn = RunningColumn;
			Section.m_CachedHeight = Section.m_MeasureFn(MeasureColumn);
		}
		if(Section.m_CachedHeight <= 0.0f)
			continue;
		const CUIRect SectionRect{RunningColumn.x, RunningColumn.y, RunningColumn.w, Section.m_CachedHeight};
		RunningColumn.y += Section.m_CachedHeight;
		RunningColumn.h = maximum(0.0f, RunningColumn.h - Section.m_CachedHeight);
		if(!Section.m_CanCacheStaticLayer || Section.m_CacheValid)
			continue;
		if(!IncludeFarSections && ComputeViewportPriority(SectionRect) > 1)
			continue;
		const int Padding = std::max(0, (int)std::ceil(Section.m_StaticCachePadding));
		const int Width = std::max(1, (int)MainView.w + Padding * 2);
		const int Height = std::max(1, (int)Section.m_CachedHeight + Padding * 2);
		if(!RecordStaticRenderTarget(Section, Width, Height))
		{
			ClearSectionCallbacks(m_vSections);
			return false;
		}
		Section.m_LastConfigHash = ComputeConfigHash(Section);
		Section.m_CacheRuntimeKey = m_RuntimeKey;
		Section.m_Dirty = false;
		Section.m_DirtyReason = ESettingsCacheDirtyReason::NONE;
	}
	ClearSectionCallbacks(m_vSections);
	return true;
}

// -- Cache invalidation --

void CSectionLoader::InvalidateCache(ESettingsCacheDirtyReason Reason)
{
	for(auto &Section : m_vSections)
	{
		Section.m_Dirty = true;
		Section.m_CacheValid = false;
		Section.m_DirtyReason = Reason;
	}
}

void CSectionLoader::SetDirtyByConfig(const void *pConfigVar)
{
	for(auto &Section : m_vSections)
	{
		for(const int *pInt : Section.m_DependencyConfigInts)
		{
			if(static_cast<const void *>(pInt) == pConfigVar)
			{
				Section.m_Dirty = true;
				Section.m_CacheValid = false;
				Section.m_DirtyReason = ESettingsCacheDirtyReason::CONFIG;
				return;
			}
		}
		for(const unsigned *pCol : Section.m_DependencyConfigCols)
		{
			if(static_cast<const void *>(pCol) == pConfigVar)
			{
				Section.m_Dirty = true;
				Section.m_CacheValid = false;
				Section.m_DirtyReason = ESettingsCacheDirtyReason::CONFIG;
				return;
			}
		}
	}
}

// -- Session cache I/O --

bool CSectionLoader::LoadSessionCache(SSessionUiCache &Cache, const char *pFilename, IStorage *pStorage)
{
	const IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_READ, IStorage::TYPE_SAVE);
	if(!File)
		return false;

	char aBuf[256];
	const unsigned Read = io_read(File, aBuf, sizeof(aBuf) - 1);
	io_close(File);
	if(Read == 0)
		return false;

	aBuf[Read] = '\0';

	// Parse simple key=value lines
	const char *p = aBuf;
	while(*p)
	{
		// Skip whitespace
		while(*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
			++p;
		if(*p == '\0')
			break;

		// Read key
		const char *pKeyEnd = p;
		while(*pKeyEnd && *pKeyEnd != '=' && *pKeyEnd != '\r' && *pKeyEnd != '\n')
			++pKeyEnd;
		const int KeyLen = (int)(pKeyEnd - p);
		if(KeyLen <= 0 || *pKeyEnd != '=')
		{
			// Advance past this line
			while(*p && *p != '\n')
				++p;
			if(*p == '\n')
				++p;
			continue;
		}

		const char *pVal = pKeyEnd + 1;
		const char *pValEnd = pVal;
		while(*pValEnd && *pValEnd != '\r' && *pValEnd != '\n')
			++pValEnd;
		const int ValLen = (int)(pValEnd - pVal);

		if(KeyLen == 13 && strncmp(p, "settings_page", 13) == 0)
			Cache.m_LastSettingsPage = atoi(pVal);
		else if(KeyLen == 11 && strncmp(p, "tab_tclient", 11) == 0)
			Cache.m_LastTClientTab = atoi(pVal);
		else if(KeyLen == 6 && strncmp(p, "tab_qm", 6) == 0)
			Cache.m_LastQmTab = atoi(pVal);
		else if(KeyLen == 8 && strncmp(p, "scroll_y", 8) == 0)
			Cache.m_LastScrollY = (float)atof(pVal);
		else if(KeyLen == 14 && strncmp(p, "viewport_width", 14) == 0)
			Cache.m_RuntimeKey.m_ViewportWidth = atoi(pVal);
		else if(KeyLen == 15 && strncmp(p, "viewport_height", 15) == 0)
			Cache.m_RuntimeKey.m_ViewportHeight = atoi(pVal);
		else if(KeyLen == 8 && strncmp(p, "ui_scale", 8) == 0)
			Cache.m_RuntimeKey.m_UiScale = atoi(pVal);
		else if(KeyLen == 11 && strncmp(p, "config_hash", 11) == 0)
			Cache.m_RuntimeKey.m_ConfigHash = ParseSessionCacheU64(pVal);
		else if(KeyLen == 13 && strncmp(p, "language_hash", 13) == 0)
			Cache.m_RuntimeKey.m_LanguageHash = ParseSessionCacheU64(pVal);
		else if(KeyLen == 9 && strncmp(p, "font_hash", 9) == 0)
			Cache.m_RuntimeKey.m_FontHash = ParseSessionCacheU64(pVal);
		else if(KeyLen == 12 && strncmp(p, "backend_hash", 12) == 0)
			Cache.m_RuntimeKey.m_BackendHash = ParseSessionCacheU64(pVal);
		else if(KeyLen == 11 && strncmp(p, "window_hash", 11) == 0)
			Cache.m_RuntimeKey.m_WindowHash = ParseSessionCacheU64(pVal);

		p = pValEnd;
		if(*p == '\n')
			++p;
	}

	Cache.m_Valid = (Cache.m_LastSettingsPage >= 0 || Cache.m_LastTClientTab >= 0 || Cache.m_LastQmTab >= 0);
	return Cache.m_Valid;
}

void CSectionLoader::SaveSessionCache(const SSessionUiCache &Cache, const char *pFilename, IStorage *pStorage)
{
	if(!Cache.m_Valid)
		return;

	pStorage->CreateFolder("qmclient", IStorage::TYPE_SAVE);
	const IOHANDLE File = pStorage->OpenFile(pFilename, IOFLAG_WRITE, IStorage::TYPE_SAVE);
	if(!File)
		return;

	char aLine[128];
	int Len;

	Len = str_format(aLine, sizeof(aLine), "settings_page=%d\n", Cache.m_LastSettingsPage);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "tab_tclient=%d\n", Cache.m_LastTClientTab);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "tab_qm=%d\n", Cache.m_LastQmTab);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "scroll_y=%f\n", Cache.m_LastScrollY);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "viewport_width=%d\n", Cache.m_RuntimeKey.m_ViewportWidth);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "viewport_height=%d\n", Cache.m_RuntimeKey.m_ViewportHeight);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "ui_scale=%d\n", Cache.m_RuntimeKey.m_UiScale);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "config_hash=%llu\n", (unsigned long long)Cache.m_RuntimeKey.m_ConfigHash);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "language_hash=%llu\n", (unsigned long long)Cache.m_RuntimeKey.m_LanguageHash);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "font_hash=%llu\n", (unsigned long long)Cache.m_RuntimeKey.m_FontHash);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "backend_hash=%llu\n", (unsigned long long)Cache.m_RuntimeKey.m_BackendHash);
	io_write(File, aLine, (unsigned)Len);

	Len = str_format(aLine, sizeof(aLine), "window_hash=%llu\n", (unsigned long long)Cache.m_RuntimeKey.m_WindowHash);
	io_write(File, aLine, (unsigned)Len);

	io_close(File);
}

// -- Profiling --

const char *CSectionLoader::GetPerfReport() const
{
	return ""; // Reporting is inline via dbg_msg in Process()
}

// -- Private helpers --

void CSectionLoader::DestroyRenderTarget(SSettingsSection &Section)
{
	if(m_pGraphics && Section.m_RenderTarget.IsValid())
		m_pGraphics->DestroyRenderTarget(&Section.m_RenderTarget);
	Section.m_RenderTarget.Invalidate();
	Section.m_RenderTargetWidth = 0;
	Section.m_RenderTargetHeight = 0;
	Section.m_CacheValid = false;
}

bool CSectionLoader::TryRenderCachedSection(SSettingsSection &Section)
{
	const CPerfTimer CacheTimer;
	bool CacheHit = false;
	if(!Section.m_CanCacheStaticLayer || !m_RenderTargetSupportedForTests)
		return false;
	if(Section.m_CacheValid && !(Section.m_CacheRuntimeKey == m_RuntimeKey))
	{
		Section.m_CacheValid = false;
		Section.m_Dirty = true;
		Section.m_DirtyReason = ESettingsCacheDirtyReason::WINDOW_SIZE;
		return false;
	}
	if(!m_pGraphics)
	{
		if(!Section.m_CacheValid || Section.m_Dirty)
			return false;
		if(Section.m_RenderInteractiveLayerFn)
		{
			if(Section.m_KeepCachedHeightStable)
			{
				CUIRect InteractiveColumn = m_RunningColumn;
				const float CachedHeight = Section.m_CachedHeight;
				const float InteractiveHeight = Section.m_RenderInteractiveLayerFn(InteractiveColumn);
				if(InteractiveHeight > CachedHeight + 0.5f)
				{
					Section.m_CachedHeight = InteractiveHeight;
					Section.m_CacheValid = false;
					Section.m_Dirty = true;
					Section.m_DirtyReason = ESettingsCacheDirtyReason::WINDOW_SIZE;
					return false;
				}
				const float ConsumedHeight = maximum(CachedHeight, InteractiveHeight);
				m_RunningColumn.y += ConsumedHeight;
				m_RunningColumn.h = maximum(0.0f, m_RunningColumn.h - ConsumedHeight);
			}
			else
				Section.m_CachedHeight = Section.m_RenderInteractiveLayerFn(m_RunningColumn);
		}
		else
			m_RunningColumn.y += Section.m_CachedHeight;
		return true;
	}
	if(!SettingsRuntimeCachingEnabled(g_Config.m_QmSettingsPrewarm, g_Config.m_QmSettingsFboCache, g_Config.m_QmNewUi))
		return false;
	if(!m_pGraphics->IsRenderTargetSupported())
		return false;

	const int Padding = std::max(0, (int)std::ceil(Section.m_StaticCachePadding));
	const int Width = std::max(1, (int)m_MainView.w + Padding * 2);
	const int Height = std::max(1, (int)Section.m_CachedHeight + Padding * 2);
	if(Section.m_MeasureFn)
	{
		CUIRect MeasureColumn = m_RunningColumn;
		const float MeasuredHeight = Section.m_MeasureFn(MeasureColumn);
		if(std::max(1, (int)MeasuredHeight + Padding * 2) != Height)
		{
			Section.m_CachedHeight = MeasuredHeight;
			DestroyRenderTarget(Section);
			Section.m_Dirty = true;
			Section.m_DirtyReason = ESettingsCacheDirtyReason::WINDOW_SIZE;
			return false;
		}
	}
	if(Section.m_RenderTarget.IsValid() && (Section.m_RenderTargetWidth != Width || Section.m_RenderTargetHeight != Height))
	{
		DestroyRenderTarget(Section);
		Section.m_Dirty = true;
		Section.m_DirtyReason = ESettingsCacheDirtyReason::WINDOW_SIZE;
	}
	if(!Section.m_RenderTarget.IsValid())
	{
		Section.m_RenderTarget = m_pGraphics->CreateRenderTarget(Width, Height);
		Section.m_RenderTargetWidth = Width;
		Section.m_RenderTargetHeight = Height;
	}
	if(!Section.m_RenderTarget.IsValid())
		return false;

	if(!Section.m_CacheValid || Section.m_Dirty)
	{
		if(!m_LiveStaticCacheRecordingEnabled)
			return false;
		if(!RecordStaticRenderTarget(Section, Width, Height))
			return false;
	}
	else
	{
		CacheHit = true;
	}

	const CUIRect CachedColumn = m_RunningColumn;
	m_pGraphics->DrawRenderTarget(Section.m_RenderTarget, m_RunningColumn.x - Padding, m_RunningColumn.y - Padding, m_RunningColumn.w + Padding * 2, Section.m_CachedHeight + Padding * 2);
	if(Section.m_RenderInteractiveLayerFn)
	{
		if(Section.m_KeepCachedHeightStable)
		{
			CUIRect InteractiveColumn = m_RunningColumn;
			const float CachedHeight = Section.m_CachedHeight;
			const float InteractiveHeight = Section.m_RenderInteractiveLayerFn(InteractiveColumn);
			if(InteractiveHeight > CachedHeight + 0.5f)
			{
				Section.m_CachedHeight = InteractiveHeight;
				Section.m_CacheValid = false;
				Section.m_Dirty = true;
				Section.m_DirtyReason = ESettingsCacheDirtyReason::WINDOW_SIZE;
				DestroyRenderTarget(Section);
				m_RunningColumn = CachedColumn;
				return false;
			}
			const float ConsumedHeight = maximum(CachedHeight, InteractiveHeight);
			m_RunningColumn.y += ConsumedHeight;
			m_RunningColumn.h = maximum(0.0f, m_RunningColumn.h - ConsumedHeight);
		}
		else
		{
			Section.m_CachedHeight = Section.m_RenderInteractiveLayerFn(m_RunningColumn);
			Section.m_CacheValid = Section.m_RenderTargetHeight == std::max(1, (int)Section.m_CachedHeight + Padding * 2);
		}
	}
	else
		m_RunningColumn.y += Section.m_CachedHeight;
	if(g_Config.m_QmSettingsFboCacheDebug)
	{
		// Profiling is opt-in to avoid polluting normal frame logs.
		dbg_msg("settings/cache", "section=%s hit=%d dirty=%d reason=%d full_ms=%.2f",
			Section.m_pName, CacheHit ? 1 : 0, Section.m_Dirty ? 1 : 0,
			(int)Section.m_DirtyReason, CacheTimer.ElapsedMs());
	}
	Section.m_DirtyReason = ESettingsCacheDirtyReason::NONE;
	return true;
}

bool CSectionLoader::RecordStaticRenderTarget(SSettingsSection &Section, int Width, int Height)
{
	if(!Section.m_RenderStaticLayerFn)
		return false;
	if(!m_pGraphics)
	{
		CUIRect CacheRect = MakeRenderTargetCacheRectForTests((float)Width - Section.m_StaticCachePadding * 2.0f, (float)Height - Section.m_StaticCachePadding * 2.0f, Section.m_StaticCachePadding);
		Section.m_RenderStaticLayerFn(CacheRect);
		Section.m_RenderTargetWidth = Width;
		Section.m_RenderTargetHeight = Height;
		Section.m_CacheValid = true;
		Section.m_CacheRuntimeKey = m_RuntimeKey;
		return true;
	}
	if(!SettingsRuntimeCachingEnabled(g_Config.m_QmSettingsPrewarm, g_Config.m_QmSettingsFboCache, g_Config.m_QmNewUi))
		return false;
	if(!m_pGraphics->IsRenderTargetSupported())
		return false;
	if(Section.m_RenderTarget.IsValid() && (Section.m_RenderTargetWidth != Width || Section.m_RenderTargetHeight != Height))
		DestroyRenderTarget(Section);
	if(!Section.m_RenderTarget.IsValid())
	{
		Section.m_RenderTarget = m_pGraphics->CreateRenderTarget(Width, Height);
		Section.m_RenderTargetWidth = Width;
		Section.m_RenderTargetHeight = Height;
	}
	if(!Section.m_RenderTarget.IsValid())
		return false;

	CUIRect CacheRect = MakeRenderTargetCacheRectForTests((float)Width - Section.m_StaticCachePadding * 2.0f, (float)Height - Section.m_StaticCachePadding * 2.0f, Section.m_StaticCachePadding);
	if(!m_pGraphics->BeginRenderTarget(Section.m_RenderTarget, ColorRGBA(0.0f, 0.0f, 0.0f, 0.0f)))
		return false;
	float ScreenTLX = 0.0f;
	float ScreenTLY = 0.0f;
	float ScreenBRX = 0.0f;
	float ScreenBRY = 0.0f;
	m_pGraphics->GetScreen(&ScreenTLX, &ScreenTLY, &ScreenBRX, &ScreenBRY);
	m_pGraphics->MapScreen(0.0f, 0.0f, (float)Width, (float)Height);
	Section.m_RenderStaticLayerFn(CacheRect);
	m_pGraphics->MapScreen(ScreenTLX, ScreenTLY, ScreenBRX, ScreenBRY);
	m_pGraphics->EndRenderTarget();
	Section.m_CacheValid = true;
	Section.m_CacheRuntimeKey = m_RuntimeKey;
	return true;
}

int CSectionLoader::ComputeViewportPriority(const CUIRect &SectionRect) const
{
	const float ViewportTop = m_MainView.y - m_ScrollY;
	const float ViewportBottom = ViewportTop + m_MainView.h;
	const float PrefetchMargin = 200.0f;

	if(SectionRect.y + SectionRect.h >= ViewportTop - PrefetchMargin &&
		SectionRect.y <= ViewportBottom + PrefetchMargin)
	{
		if(SectionRect.y + SectionRect.h >= ViewportTop &&
			SectionRect.y <= ViewportBottom)
			return 0; // In viewport
		return 1; // Near viewport
	}
	return 2; // Far from viewport
}

uint64_t CSectionLoader::ComputeConfigHash(const SSettingsSection &Section)
{
	// FNV-1a 64-bit
	uint64_t Hash = 14695981039346656037ull;
	for(const int *pVal : Section.m_DependencyConfigInts)
	{
		const uint8_t *pBytes = reinterpret_cast<const uint8_t *>(pVal);
		for(size_t i = 0; i < sizeof(int); ++i)
		{
			Hash ^= pBytes[i];
			Hash *= 1099511628211ull;
		}
	}
	for(const unsigned *pVal : Section.m_DependencyConfigCols)
	{
		const uint8_t *pBytes = reinterpret_cast<const uint8_t *>(pVal);
		for(size_t i = 0; i < sizeof(unsigned); ++i)
		{
			Hash ^= pBytes[i];
			Hash *= 1099511628211ull;
		}
	}
	return Hash;
}

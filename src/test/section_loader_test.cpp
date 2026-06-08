#include <base/system.h>

#include <engine/storage.h>

#include <game/client/components/section_loader.h>

#include <gtest/gtest.h>
#include <test/test.h>

// Helper: modify the CUIRect inline without calling HSplitTop (not linked in test runner)
static float ConsumeHeight(CUIRect &Rect, float Height)
{
	Rect.h -= Height;
	Rect.y += Height;
	return Height;
}

// Helper: create a simple section with a fixed height
static SSettingsSection MakeTestSection(const char *pName, float Height)
{
	SSettingsSection S;
	S.m_pName = pName;
	S.m_MeasureFn = [Height](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, Height);
	};
	S.m_RenderCompactFn = [Height](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, Height);
	};
	S.m_RenderFullFn = [Height](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, Height);
	};
	return S;
}

static void RunRegisteredFrames(CSectionLoader &Loader, const CUIRect &MainView, const std::function<std::vector<SSettingsSection>()> &MakeSections, float BudgetMs = 100.0f)
{
	int FrameCount = 0;
	do
	{
		Loader.Register(MakeSections());
		Loader.Begin(MainView, BudgetMs);
		++FrameCount;
	} while(Loader.Process() && FrameCount < 100);
}

TEST(SectionLoader, StateTransitions)
{
	CSectionLoader Loader;
	Loader.Register({
		MakeTestSection("Section A", 50.0f),
		MakeTestSection("Section B", 60.0f),
	});

	CUIRect MainView{0, 0, 400, 600};
	Loader.Begin(MainView, 100.0f);

	int Iterations = 0;
	while(Loader.Process() && Iterations < 10)
		++Iterations;

	EXPECT_TRUE(Loader.IsComplete());
}

TEST(SectionLoader, FrameBudgetTruncation)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(true);
	std::vector<SSettingsSection> vSections;
	for(int i = 0; i < 30; ++i)
	{
		char aName[32];
		str_format(aName, sizeof(aName), "Section %d", i);
		vSections.push_back(MakeTestSection(aName, 30.0f));
	}
	Loader.Register(std::move(vSections));

	CUIRect MainView{0, 0, 400, 800};
	Loader.Begin(MainView, 0.1);

	int FrameCount = 0;
	while(Loader.Process() && FrameCount < 100)
		++FrameCount;

	// With 0.1ms budget and 30 sections, it MUST take more than 1 frame
	EXPECT_GT(FrameCount, 1);
}

TEST(SectionLoader, FullSectionsRenderEveryFrame)
{
	CSectionLoader Loader;
	bool FullRenderCalled = false;
	int ConfigValue = 42;

	SSettingsSection S;
	S.m_pName = "DirtyTest";
	S.m_DependencyConfigInts.push_back(&ConfigValue);
	S.m_MeasureFn = [](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, 10.0f);
	};
	S.m_RenderCompactFn = [](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, 10.0f);
	};
	S.m_RenderFullFn = [&FullRenderCalled](CUIRect &Rect) -> float {
		FullRenderCalled = true;
		return ConsumeHeight(Rect, 10.0f);
	};

	CUIRect MainView{0, 0, 400, 600};
	auto MakeSections = [&]() {
		return std::vector<SSettingsSection>{S};
	};

	// First frame: m_Dirty=true → FullRender must be called
	RunRegisteredFrames(Loader, MainView, MakeSections);
	EXPECT_TRUE(FullRenderCalled);

	// Second frame: config unchanged, but immediate-mode UI still renders.
	FullRenderCalled = false;
	RunRegisteredFrames(Loader, MainView, MakeSections);
	EXPECT_TRUE(FullRenderCalled);

	// Modify config → dirty flag triggers FullRender again
	ConfigValue = 99;
	Loader.SetDirtyByConfig(&ConfigValue);
	FullRenderCalled = false;
	RunRegisteredFrames(Loader, MainView, MakeSections);
	EXPECT_TRUE(FullRenderCalled);
}

TEST(SectionLoader, FullSectionsIgnoreFrameBudget)
{
	CSectionLoader Loader;
	int FullRenderCount = 0;
	auto MakeSections = [&]() {
		std::vector<SSettingsSection> vSections;
		for(int SectionIndex = 0; SectionIndex < 3; ++SectionIndex)
		{
			SSettingsSection S = MakeTestSection(SectionIndex == 0 ? "Full A" : SectionIndex == 1 ? "Full B" :
														"Full C",
				10.0f);
			S.m_RenderFullFn = [&FullRenderCount](CUIRect &Rect) -> float {
				++FullRenderCount;
				return ConsumeHeight(Rect, 10.0f);
			};
			vSections.push_back(S);
		}
		return vSections;
	};

	CUIRect MainView{0, 0, 400, 600};
	RunRegisteredFrames(Loader, MainView, MakeSections);
	ASSERT_TRUE(Loader.IsComplete());

	FullRenderCount = 0;
	RunRegisteredFrames(Loader, MainView, MakeSections, 0.0f);
	EXPECT_EQ(FullRenderCount, 3);
	EXPECT_TRUE(Loader.IsComplete());
}

TEST(SectionLoader, FarFullSectionsAdvanceWithoutRendering)
{
	CSectionLoader Loader;
	int FarFullRenderCount = 0;

	SSettingsSection Top = MakeTestSection("Tall Top", 900.0f);
	SSettingsSection Far = MakeTestSection("Far Section", 50.0f);
	Far.m_RenderFullFn = [&FarFullRenderCount](CUIRect &Rect) -> float {
		++FarFullRenderCount;
		return ConsumeHeight(Rect, 50.0f);
	};

	Loader.SetProgressiveEnabled(false);
	Loader.Register({Top, Far});
	Loader.Begin(CUIRect{0, 0, 400, 240}, 100.0f);

	EXPECT_FALSE(Loader.Process());
	EXPECT_TRUE(Loader.IsComplete());
	EXPECT_EQ(FarFullRenderCount, 0);
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 950.0f);
}

TEST(SectionLoader, DirtyFarFullSectionUpdatesMeasuredHeightWithoutRendering)
{
	CSectionLoader Loader;
	int FarMeasureHeight = 50;
	int FarFullRenderCount = 0;

	auto MakeSections = [&]() {
		SSettingsSection Top = MakeTestSection("Tall Top", 900.0f);
		SSettingsSection Far = MakeTestSection("Far Section", 50.0f);
		Far.m_MeasureFn = [&FarMeasureHeight](CUIRect &Rect) -> float {
			return ConsumeHeight(Rect, (float)FarMeasureHeight);
		};
		Far.m_RenderFullFn = [&FarFullRenderCount, &FarMeasureHeight](CUIRect &Rect) -> float {
			++FarFullRenderCount;
			return ConsumeHeight(Rect, (float)FarMeasureHeight);
		};
		Far.m_DependencyConfigInts = {&FarMeasureHeight};
		return std::vector<SSettingsSection>{Top, Far};
	};

	Loader.SetProgressiveEnabled(false);
	Loader.Register(MakeSections());
	Loader.Begin(CUIRect{0, 0, 400, 240}, 100.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 950.0f);

	FarMeasureHeight = 80;
	Loader.Register(MakeSections());
	Loader.Begin(CUIRect{0, 0, 400, 240}, 100.0f);

	EXPECT_FALSE(Loader.Process());
	EXPECT_EQ(FarFullRenderCount, 0);
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 980.0f);
}

TEST(SectionLoader, FarFullSectionRendersAfterScrollingIntoView)
{
	CSectionLoader Loader;
	int FarFullRenderCount = 0;

	auto MakeSections = [&]() {
		SSettingsSection Top = MakeTestSection("Tall Top", 900.0f);
		SSettingsSection Far = MakeTestSection("Far Section", 50.0f);
		Far.m_RenderFullFn = [&FarFullRenderCount](CUIRect &Rect) -> float {
			++FarFullRenderCount;
			return ConsumeHeight(Rect, 50.0f);
		};
		return std::vector<SSettingsSection>{Top, Far};
	};

	Loader.SetProgressiveEnabled(false);
	Loader.Register(MakeSections());
	Loader.Begin(CUIRect{0, 0, 400, 240}, 100.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_EQ(FarFullRenderCount, 0);

	Loader.m_ScrollY = -900.0f;
	Loader.Register(MakeSections());
	Loader.Begin(CUIRect{0, -900.0f, 400, 240}, 100.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_EQ(FarFullRenderCount, 1);
}

TEST(SectionLoader, FarCachedInteractiveLayerRunsAfterScrollingIntoView)
{
	CSectionLoader Loader;
	int InteractiveRenderCount = 0;

	auto MakeSections = [&]() {
		SSettingsSection Top = MakeTestSection("Tall Top", 900.0f);
		SSettingsSection Far = MakeTestSection("Far Cached Interactive", 50.0f);
		Far.m_CanCacheStaticLayer = true;
		Far.m_RenderInteractiveLayerFn = [&InteractiveRenderCount](CUIRect &Rect) -> float {
			++InteractiveRenderCount;
			return ConsumeHeight(Rect, 50.0f);
		};
		Far.m_ShouldRenderInteractiveLayerFn = [](const CUIRect &) {
			return true;
		};
		return std::vector<SSettingsSection>{Top, Far};
	};

	Loader.SetProgressiveEnabled(false);
	Loader.Register(MakeSections());
	Loader.m_ScrollY = -900.0f;
	Loader.Begin(CUIRect{0, -900.0f, 400, 240}, 100.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_EQ(InteractiveRenderCount, 0);

	Loader.MarkCacheValidForTests("Far Cached Interactive");
	Loader.m_ScrollY = 0.0f;
	Loader.Register(MakeSections());
	Loader.Begin(CUIRect{0, 0, 400, 240}, 100.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_EQ(InteractiveRenderCount, 0);

	Loader.m_ScrollY = -900.0f;
	Loader.Register(MakeSections());
	Loader.Begin(CUIRect{0, -900.0f, 400, 240}, 100.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_EQ(InteractiveRenderCount, 1);
}

TEST(SectionLoader, CachedInteractiveLayerInvalidatesWhenInteractiveExceedsMeasuredHeight)
{
	CSectionLoader Loader;
	int InteractiveRenderCount = 0;
	int FullRenderCount = 0;

	SSettingsSection Section = MakeTestSection("Stable Cached Interactive", 50.0f);
	Section.m_CanCacheStaticLayer = true;
	Section.m_KeepCachedHeightStable = true;
	Section.m_RenderFullFn = [&FullRenderCount](CUIRect &Rect) -> float {
		++FullRenderCount;
		return ConsumeHeight(Rect, 50.0f);
	};
	Section.m_RenderInteractiveLayerFn = [&InteractiveRenderCount](CUIRect &Rect) -> float {
		++InteractiveRenderCount;
		return ConsumeHeight(Rect, 70.0f);
	};

	Loader.SetProgressiveEnabled(false);
	Loader.Register({Section});
	Loader.MarkCacheValidForTests("Stable Cached Interactive");
	Loader.Begin(CUIRect{0, 0, 400, 240}, 100.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_EQ(InteractiveRenderCount, 1);
	EXPECT_EQ(FullRenderCount, 1);
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 50.0f);
}

TEST(SectionLoader, FullSectionsAfterUnfinishedSectionStillRender)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(true);
	int LastFullRenderCount = 0;

	auto MakeSections = [&](const char *pMiddleName) {
		std::vector<SSettingsSection> vSections;
		vSections.push_back(MakeTestSection("Full A", 10.0f));
		vSections.push_back(MakeTestSection(pMiddleName, 10.0f));
		SSettingsSection Last = MakeTestSection("Full C", 10.0f);
		Last.m_RenderFullFn = [&LastFullRenderCount](CUIRect &Rect) -> float {
			++LastFullRenderCount;
			return ConsumeHeight(Rect, 10.0f);
		};
		vSections.push_back(Last);
		return vSections;
	};

	Loader.Register(MakeSections("Middle B"));
	CUIRect MainView{0, 0, 400, 600};
	Loader.Begin(MainView, 100.0f);
	while(Loader.Process()) {}
	ASSERT_TRUE(Loader.IsComplete());

	LastFullRenderCount = 0;
	Loader.Register(MakeSections("New Middle B"));
	Loader.Begin(MainView, 0.0f);
	EXPECT_TRUE(Loader.Process());
	EXPECT_EQ(LastFullRenderCount, 1);
}

TEST(SectionLoader, ScrollOffsetPromotesScrolledIntoViewSection)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(true);
	int CompactRenderCount = 0;

	SSettingsSection First = MakeTestSection("Tall Top", 900.0f);
	SSettingsSection Second = MakeTestSection("Scrolled Into View", 10.0f);
	Second.m_RenderCompactFn = [&CompactRenderCount](CUIRect &Rect) -> float {
		++CompactRenderCount;
		return ConsumeHeight(Rect, 10.0f);
	};
	auto MakeSections = [&]() {
		return std::vector<SSettingsSection>{First, Second};
	};

	CUIRect ScrolledMainView{0, -500.0f, 400, 600};
	Loader.m_ScrollY = -500.0f;
	Loader.Register(MakeSections());
	Loader.Begin(ScrolledMainView, 100.0f);
	EXPECT_TRUE(Loader.Process());

	Loader.m_ScrollY = -500.0f;
	Loader.Register(MakeSections());
	Loader.Begin(ScrolledMainView, 100.0f);
	EXPECT_TRUE(Loader.Process());
	EXPECT_EQ(CompactRenderCount, 1);
}

TEST(SectionLoader, MeasureFullHeightConsistency)
{
	CSectionLoader Loader;
	float MeasureHeight = 0.0f;
	float FullHeight = 0.0f;

	SSettingsSection S;
	S.m_pName = "HeightTest";
	S.m_MeasureFn = [&MeasureHeight](CUIRect &Rect) -> float {
		MeasureHeight = 45.0f;
		return ConsumeHeight(Rect, 45.0f);
	};
	S.m_RenderCompactFn = [](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, 45.0f);
	};
	S.m_RenderFullFn = [&FullHeight](CUIRect &Rect) -> float {
		FullHeight = 45.0f;
		return ConsumeHeight(Rect, 45.0f);
	};

	CUIRect MainView{0, 0, 400, 800};
	RunRegisteredFrames(Loader, MainView, [&]() {
		return std::vector<SSettingsSection>{S};
	});

	EXPECT_FLOAT_EQ(MeasureHeight, FullHeight);
}

TEST(SectionLoader, ReRegisterSameSectionPreservesProgress)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(true);
	int MeasureCount = 0;
	int CompactCount = 0;
	int FullCount = 0;

	auto MakeCountingSection = [&]() {
		SSettingsSection S;
		S.m_pName = "FrameLocalSection";
		S.m_MeasureFn = [&MeasureCount](CUIRect &Rect) -> float {
			++MeasureCount;
			return ConsumeHeight(Rect, 10.0f);
		};
		S.m_RenderCompactFn = [&CompactCount](CUIRect &Rect) -> float {
			++CompactCount;
			return ConsumeHeight(Rect, 10.0f);
		};
		S.m_RenderFullFn = [&FullCount](CUIRect &Rect) -> float {
			++FullCount;
			return ConsumeHeight(Rect, 10.0f);
		};
		return S;
	};

	CUIRect MainView{0, 0, 400, 600};
	Loader.Register({MakeCountingSection()});
	Loader.Begin(MainView, 100.0f);
	EXPECT_TRUE(Loader.Process());
	EXPECT_EQ(MeasureCount, 1);
	EXPECT_EQ(CompactCount, 0);
	EXPECT_EQ(FullCount, 0);

	Loader.Register({MakeCountingSection()});
	Loader.Begin(MainView, 100.0f);
	EXPECT_TRUE(Loader.Process());
	EXPECT_EQ(MeasureCount, 1);
	EXPECT_EQ(CompactCount, 1);
	EXPECT_EQ(FullCount, 0);

	Loader.Register({MakeCountingSection()});
	Loader.Begin(MainView, 100.0f);
	while(Loader.Process()) {}
	EXPECT_EQ(MeasureCount, 1);
	EXPECT_EQ(CompactCount, 1);
	EXPECT_EQ(FullCount, 1);
	EXPECT_TRUE(Loader.IsComplete());

	Loader.Register({MakeCountingSection()});
	Loader.Begin(MainView, 100.0f);
	while(Loader.Process()) {}
	EXPECT_EQ(MeasureCount, 1);
	EXPECT_EQ(CompactCount, 1);
	EXPECT_EQ(FullCount, 2);
	EXPECT_TRUE(Loader.IsComplete());
}

TEST(SectionLoader, ReRegisterSameSectionUsesLatestCallbacks)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(true);
	int RenderedVersion = 0;

	auto MakeVersionedSection = [&](int Version) {
		SSettingsSection S;
		S.m_pName = "FrameLocalSection";
		S.m_MeasureFn = [](CUIRect &Rect) -> float {
			return ConsumeHeight(Rect, 10.0f);
		};
		S.m_RenderCompactFn = [Version, &RenderedVersion](CUIRect &Rect) -> float {
			RenderedVersion = Version;
			return ConsumeHeight(Rect, 10.0f);
		};
		S.m_RenderFullFn = [Version, &RenderedVersion](CUIRect &Rect) -> float {
			RenderedVersion = Version;
			return ConsumeHeight(Rect, 10.0f);
		};
		return S;
	};

	CUIRect MainView{0, 0, 400, 600};
	Loader.Register({MakeVersionedSection(1)});
	Loader.Begin(MainView, 100.0f);
	EXPECT_TRUE(Loader.Process());

	Loader.Register({MakeVersionedSection(2)});
	Loader.Begin(MainView, 100.0f);
	EXPECT_TRUE(Loader.Process());
	EXPECT_EQ(RenderedVersion, 2);

	Loader.Register({MakeVersionedSection(3)});
	Loader.Begin(MainView, 100.0f);
	while(Loader.Process()) {}
	EXPECT_EQ(RenderedVersion, 3);
}

TEST(SectionLoader, NonProgressiveModeRendersFullSectionOnFirstProcess)
{
	CSectionLoader Loader;
	int MeasureCount = 0;
	int CompactCount = 0;
	int FullCount = 0;

	SSettingsSection S;
	S.m_pName = "ImmediateFullSection";
	S.m_MeasureFn = [&MeasureCount](CUIRect &Rect) -> float {
		++MeasureCount;
		return ConsumeHeight(Rect, 10.0f);
	};
	S.m_RenderCompactFn = [&CompactCount](CUIRect &Rect) -> float {
		++CompactCount;
		return ConsumeHeight(Rect, 10.0f);
	};
	S.m_RenderFullFn = [&FullCount](CUIRect &Rect) -> float {
		++FullCount;
		return ConsumeHeight(Rect, 10.0f);
	};

	Loader.SetProgressiveEnabled(false);
	Loader.Register({S});
	Loader.Begin(CUIRect{0, 0, 400, 600}, 0.0f);

	EXPECT_FALSE(Loader.Process());
	EXPECT_TRUE(Loader.IsComplete());
	EXPECT_EQ(MeasureCount, 1);
	EXPECT_EQ(CompactCount, 0);
	EXPECT_EQ(FullCount, 1);
}

TEST(SectionLoader, ProcessDoesNotRetainCallbacksAcrossFrames)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(true);
	int RenderedVersion = 0;

	SSettingsSection S;
	S.m_pName = "FrameLocalSection";
	S.m_MeasureFn = [](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, 10.0f);
	};
	S.m_RenderCompactFn = [&RenderedVersion](CUIRect &Rect) -> float {
		RenderedVersion = 1;
		return ConsumeHeight(Rect, 10.0f);
	};
	S.m_RenderFullFn = [&RenderedVersion](CUIRect &Rect) -> float {
		RenderedVersion = 1;
		return ConsumeHeight(Rect, 10.0f);
	};

	CUIRect MainView{0, 0, 400, 600};
	Loader.Register({S});
	Loader.Begin(MainView, 100.0f);
	EXPECT_TRUE(Loader.Process());
	EXPECT_EQ(RenderedVersion, 0);

	Loader.Begin(MainView, 100.0f);
	EXPECT_TRUE(Loader.Process());
	EXPECT_EQ(RenderedVersion, 0);

	Loader.Begin(MainView, 100.0f);
	while(Loader.Process()) {}
	EXPECT_EQ(RenderedVersion, 0);
	EXPECT_TRUE(Loader.IsComplete());
}

TEST(SectionLoader, WarmupWithoutCache)
{
	CSectionLoader Loader;
	Loader.Register({MakeTestSection("A", 50.0f)});

	// nullptr cache → warmup should immediately report done
	EXPECT_TRUE(Loader.Warmup(nullptr, 3.0f));
	EXPECT_TRUE(Loader.IsWarmupComplete());

	// Invalid cache → same
	SSessionUiCache InvalidCache;
	InvalidCache.m_Valid = false;
	EXPECT_TRUE(Loader.Warmup(&InvalidCache, 3.0f));
	EXPECT_TRUE(Loader.IsWarmupComplete());
}

TEST(SectionLoader, WarmupWithCache)
{
	CSectionLoader Loader;
	Loader.Register({
		MakeTestSection("A", 50.0f),
		MakeTestSection("B", 50.0f),
		MakeTestSection("C", 50.0f),
	});

	CUIRect MainView{0, 0, 400, 600};
	Loader.Begin(MainView, 5.0f);

	SSessionUiCache Cache;
	Cache.m_LastTClientTab = 0;
	Cache.m_LastScrollY = 0.0f;
	Cache.m_Valid = true;

	bool Done = false;
	for(int i = 0; i < 20 && !Done; ++i)
		Done = Loader.Warmup(&Cache, 100.0f);

	EXPECT_TRUE(Done);
	EXPECT_TRUE(Loader.IsWarmupComplete());
}

TEST(SectionLoader, SessionCacheRoundTripsLastTabAndScroll)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	SSessionUiCache Saved;
	Saved.m_LastSettingsPage = 8;
	Saved.m_LastTClientTab = 0;
	Saved.m_LastQmTab = -1;
	Saved.m_LastScrollY = -240.0f;
	Saved.m_RuntimeKey.m_ViewportWidth = 1600;
	Saved.m_RuntimeKey.m_ViewportHeight = 900;
	Saved.m_RuntimeKey.m_UiScale = 100;
	Saved.m_RuntimeKey.m_ConfigHash = 11;
	Saved.m_RuntimeKey.m_LanguageHash = 22;
	Saved.m_RuntimeKey.m_FontHash = 33;
	Saved.m_RuntimeKey.m_BackendHash = 44;
	Saved.m_RuntimeKey.m_WindowHash = 55;
	Saved.m_Valid = true;

	CSectionLoader::SaveSessionCache(Saved, "qmclient/settings_section_cache_metadata.cfg", pStorage.get());

	SSessionUiCache Loaded;
	ASSERT_TRUE(CSectionLoader::LoadSessionCache(Loaded, "qmclient/settings_section_cache_metadata.cfg", pStorage.get()));
	EXPECT_EQ(Loaded.m_LastSettingsPage, 8);
	EXPECT_EQ(Loaded.m_LastTClientTab, 0);
	EXPECT_EQ(Loaded.m_LastQmTab, -1);
	EXPECT_FLOAT_EQ(Loaded.m_LastScrollY, -240.0f);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_ViewportWidth, 1600);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_ViewportHeight, 900);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_UiScale, 100);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_ConfigHash, 11u);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_LanguageHash, 22u);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_FontHash, 33u);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_BackendHash, 44u);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_WindowHash, 55u);
	EXPECT_TRUE(Loaded.m_Valid);
}

TEST(SectionLoader, SessionCacheLoadKeepsWorkingWhenRuntimeKeyFieldsAreMissing)
{
	CTestInfo Info;
	std::unique_ptr<IStorage> pStorage = Info.CreateTestStorage();
	ASSERT_NE(pStorage, nullptr);

	const char *pPath = "settings_section_cache_metadata.cfg";
	const char *pLegacyContents =
		"settings_page=8\n"
		"tab_tclient=0\n"
		"tab_qm=-1\n"
		"scroll_y=-240.000000\n";

	char aSavePath[IO_MAX_PATH_LENGTH];
	pStorage->GetCompletePath(IStorage::TYPE_SAVE, pPath, aSavePath, sizeof(aSavePath));
	IOHANDLE File = io_open(aSavePath, IOFLAG_WRITE);
	ASSERT_TRUE(File != nullptr);
	io_write(File, pLegacyContents, str_length(pLegacyContents));
	io_close(File);

	SSessionUiCache Loaded;
	ASSERT_TRUE(CSectionLoader::LoadSessionCache(Loaded, pPath, pStorage.get()));
	EXPECT_EQ(Loaded.m_LastSettingsPage, 8);
	EXPECT_EQ(Loaded.m_LastTClientTab, 0);
	EXPECT_EQ(Loaded.m_LastQmTab, -1);
	EXPECT_FLOAT_EQ(Loaded.m_LastScrollY, -240.0f);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_ViewportWidth, 0);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_ViewportHeight, 0);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_UiScale, 0);
	EXPECT_EQ(Loaded.m_RuntimeKey.m_ConfigHash, 0u);
	EXPECT_TRUE(Loaded.m_Valid);
}

TEST(SectionLoader, InvalidateAfterLanguageSwitch)
{
	CSectionLoader Loader;
	int RenderCount = 0;

	SSettingsSection S;
	S.m_pName = "CacheTest";
	S.m_MeasureFn = [](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, 10.0f);
	};
	S.m_RenderCompactFn = [](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, 10.0f);
	};
	S.m_RenderFullFn = [&RenderCount](CUIRect &Rect) -> float {
		++RenderCount;
		return ConsumeHeight(Rect, 10.0f);
	};

	CUIRect MainView{0, 0, 400, 600};
	auto MakeSections = [&]() {
		return std::vector<SSettingsSection>{S};
	};

	// First frame: renders
	RunRegisteredFrames(Loader, MainView, MakeSections);
	EXPECT_EQ(RenderCount, 1);

	// Second frame: renders again because UI is immediate-mode
	RunRegisteredFrames(Loader, MainView, MakeSections);
	EXPECT_EQ(RenderCount, 2);

	// Simulate language change
	Loader.InvalidateCache();
	RunRegisteredFrames(Loader, MainView, MakeSections);
	EXPECT_EQ(RenderCount, 3);
}

TEST(SectionLoader, RejectsVisibleSummarySectionNames)
{
	EXPECT_FALSE(CSectionLoader::IsVisibleSummarySectionName("DeferredSummary:Mouse"));
	EXPECT_FALSE(CSectionLoader::IsVisibleSummarySectionName("CompactSummary:Controls"));
	EXPECT_FALSE(CSectionLoader::IsVisibleSummarySectionName("SummaryBlock:Controls"));
	EXPECT_TRUE(CSectionLoader::IsVisibleSummarySectionName("Controls:Mouse"));
	EXPECT_TRUE(CSectionLoader::IsVisibleSummarySectionName("TClient:Pet"));
}

TEST(SectionLoader, RenderTargetUnsupportedFallsBackToFullRender)
{
	CSectionLoader Loader;
	int FullRenderCount = 0;
	SSettingsSection S = MakeTestSection("CacheFallback", 10.0f);
	S.m_CanCacheStaticLayer = true;
	S.m_RenderFullFn = [&FullRenderCount](CUIRect &Rect) -> float {
		++FullRenderCount;
		return ConsumeHeight(Rect, 10.0f);
	};
	Loader.SetRenderTargetSupportedForTests(false);

	CUIRect MainView{0, 0, 400, 600};
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });

	EXPECT_EQ(FullRenderCount, 1);
}

TEST(SectionLoader, CleanCachedSectionSkipsFullRender)
{
	CSectionLoader Loader;
	int FullRenderCount = 0;

	SSettingsSection S = MakeTestSection("Cached", 10.0f);
	S.m_CanCacheStaticLayer = true;
	S.m_RenderFullFn = [&FullRenderCount](CUIRect &Rect) -> float {
		++FullRenderCount;
		return ConsumeHeight(Rect, 10.0f);
	};

	CUIRect MainView{0, 0, 400, 600};
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });
	EXPECT_EQ(FullRenderCount, 1);

	Loader.MarkCacheValidForTests("Cached");
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });
	EXPECT_EQ(FullRenderCount, 1);
}

TEST(SectionLoader, CachedInteractiveLayerUpdatesSectionHeight)
{
	CSectionLoader Loader;
	SSettingsSection S = MakeTestSection("CachedInteractiveHeight", 40.0f);
	S.m_CanCacheStaticLayer = true;
	S.m_RenderInteractiveLayerFn = [](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, 60.0f);
	};

	CUIRect MainView{0, 0, 400, 600};
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });

	Loader.MarkCacheValidForTests("CachedInteractiveHeight");
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });

	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 60.0f);
}

TEST(SectionLoader, CachedInteractiveLayerStaysVisibleWhenPredicateRejects)
{
	CSectionLoader Loader;
	int FullRenderCount = 0;
	int InteractiveRenderCount = 0;

	SSettingsSection S = MakeTestSection("CachedInteractiveAlwaysVisible", 40.0f);
	S.m_CanCacheStaticLayer = true;
	S.m_RenderFullFn = [&FullRenderCount](CUIRect &Rect) -> float {
		++FullRenderCount;
		return ConsumeHeight(Rect, 40.0f);
	};
	S.m_RenderInteractiveLayerFn = [&InteractiveRenderCount](CUIRect &Rect) -> float {
		++InteractiveRenderCount;
		return ConsumeHeight(Rect, 40.0f);
	};
	S.m_ShouldRenderInteractiveLayerFn = [](const CUIRect &) {
		return false;
	};

	CUIRect MainView{0, 0, 400, 600};
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });
	EXPECT_EQ(FullRenderCount, 1);

	Loader.MarkCacheValidForTests("CachedInteractiveAlwaysVisible");
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });

	EXPECT_EQ(FullRenderCount, 1);
	EXPECT_EQ(InteractiveRenderCount, 1);
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 40.0f);
}

TEST(SectionLoader, CachedInteractiveLayerRunsWhenNeeded)
{
	CSectionLoader Loader;
	int InteractiveRenderCount = 0;

	SSettingsSection S = MakeTestSection("CachedInteractiveNeeded", 40.0f);
	S.m_CanCacheStaticLayer = true;
	S.m_RenderInteractiveLayerFn = [&InteractiveRenderCount](CUIRect &Rect) -> float {
		++InteractiveRenderCount;
		return ConsumeHeight(Rect, 45.0f);
	};
	S.m_ShouldRenderInteractiveLayerFn = [](const CUIRect &) {
		return true;
	};

	CUIRect MainView{0, 0, 400, 600};
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });

	Loader.MarkCacheValidForTests("CachedInteractiveNeeded");
	RunRegisteredFrames(Loader, MainView, [&]() { return std::vector<SSettingsSection>{S}; });

	EXPECT_EQ(InteractiveRenderCount, 1);
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 45.0f);
}

TEST(SectionLoader, DirtyReasonInvalidatesCache)
{
	CSectionLoader Loader;
	SSettingsSection S = MakeTestSection("DirtyCache", 10.0f);
	S.m_CanCacheStaticLayer = true;
	Loader.Register({S});
	Loader.MarkCacheValidForTests("DirtyCache");

	Loader.InvalidateCache(ESettingsCacheDirtyReason::LANGUAGE);

	EXPECT_FALSE(Loader.IsCacheValidForTests("DirtyCache"));
}

TEST(SectionLoader, RenderTargetCacheRectUsesSectionLocalCoordinates)
{
	const CUIRect CacheRect = CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 120.0f);

	EXPECT_FLOAT_EQ(CacheRect.x, 0.0f);
	EXPECT_FLOAT_EQ(CacheRect.y, 0.0f);
	EXPECT_FLOAT_EQ(CacheRect.w, 320.0f);
	EXPECT_FLOAT_EQ(CacheRect.h, 120.0f);
}

TEST(SectionLoader, RenderTargetCacheRectIncludesPaddingOffset)
{
	const CUIRect CacheRect = CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 120.0f, 20.0f);

	EXPECT_FLOAT_EQ(CacheRect.x, 20.0f);
	EXPECT_FLOAT_EQ(CacheRect.y, 20.0f);
	EXPECT_FLOAT_EQ(CacheRect.w, 320.0f);
	EXPECT_FLOAT_EQ(CacheRect.h, 120.0f);
}

TEST(SectionLoader, PrewarmStaticRenderTargetsMarksNearSectionCacheValidWithoutFullRender)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(false);
	Loader.SetRenderTargetSupportedForTests(true);

	int StaticRendered = 0;
	int FullRendered = 0;
	SSettingsSection Section;
	Section.m_pName = "TClient:FirstScreen";
	Section.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 120.0f);
	};
	Section.m_RenderStaticLayerFn = [&](CUIRect &Col) {
		++StaticRendered;
		return ConsumeHeight(Col, 120.0f);
	};
	Section.m_RenderFullFn = [&](CUIRect &Col) {
		++FullRendered;
		return ConsumeHeight(Col, 120.0f);
	};
	Section.m_CanCacheStaticLayer = true;
	Loader.Register({Section});

	EXPECT_TRUE(Loader.PrewarmStaticRenderTargets(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f, 5.0f));
	EXPECT_EQ(StaticRendered, 1);
	EXPECT_EQ(FullRendered, 0);
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:FirstScreen"));
}

TEST(SectionLoader, PrewarmedStaticRenderTargetSkipsFullRenderOnNextFrame)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(false);
	Loader.SetRenderTargetSupportedForTests(true);

	int StaticRendered = 0;
	int FullRendered = 0;
	SSettingsSection Section;
	Section.m_pName = "TClient:CachedFirstScreen";
	Section.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 120.0f);
	};
	Section.m_RenderStaticLayerFn = [&](CUIRect &Col) {
		++StaticRendered;
		return ConsumeHeight(Col, 120.0f);
	};
	Section.m_RenderFullFn = [&](CUIRect &Col) {
		++FullRendered;
		return ConsumeHeight(Col, 120.0f);
	};
	Section.m_CanCacheStaticLayer = true;
	Loader.Register({Section});

	EXPECT_TRUE(Loader.PrewarmStaticRenderTargets(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f, 5.0f));

	Loader.Register({Section});
	Loader.Begin(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 5.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_EQ(StaticRendered, 1);
	EXPECT_EQ(FullRendered, 0);
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 120.0f);
}

TEST(SectionLoader, ReRegisterConfigChangeInvalidatesPrewarmedCacheBeforeFullState)
{
	CSectionLoader Loader;
	int ConfigValue = 1;
	SSettingsSection Section;
	Section.m_pName = "TClient:ConfigSensitive";
	Section.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 100.0f);
	};
	Section.m_RenderStaticLayerFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 100.0f);
	};
	Section.m_CanCacheStaticLayer = true;
	Section.m_DependencyConfigInts = {&ConfigValue};

	Loader.Register({Section});
	EXPECT_TRUE(Loader.PrewarmStaticRenderTargets(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f, 5.0f));
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:ConfigSensitive"));

	ConfigValue = 2;
	Loader.Register({Section});

	EXPECT_FALSE(Loader.IsCacheValidForTests("TClient:ConfigSensitive"));
}

TEST(SectionLoader, ReRegisterRuntimeKeyChangeInvalidatesPrewarmedCache)
{
	CSectionLoader Loader;
	SSettingsSection Section;
	Section.m_pName = "TClient:RuntimeSensitive";
	Section.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 100.0f);
	};
	Section.m_RenderStaticLayerFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 100.0f);
	};
	Section.m_CanCacheStaticLayer = true;

	SSettingsSectionCacheRuntimeKey RuntimeKey;
	RuntimeKey.m_ViewportWidth = 320;
	RuntimeKey.m_ViewportHeight = 240;
	RuntimeKey.m_UiScale = 100;
	Loader.SetRuntimeKey(RuntimeKey);
	Loader.Register({Section});
	EXPECT_TRUE(Loader.PrewarmStaticRenderTargets(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f, 5.0f));
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:RuntimeSensitive"));

	RuntimeKey.m_ViewportHeight = 260;
	Loader.SetRuntimeKey(RuntimeKey);
	Loader.Register({Section});

	EXPECT_FALSE(Loader.IsCacheValidForTests("TClient:RuntimeSensitive"));
}

TEST(SectionLoader, PrewarmStaticRenderTargetsSkipsFarSectionsUsingRunningColumn)
{
	CSectionLoader Loader;
	int StaticRendered = 0;

	SSettingsSection SpacerTop;
	SpacerTop.m_pName = "TClient:SpacerTop";
	SpacerTop.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 900.0f);
	};

	SSettingsSection FarSecond;
	FarSecond.m_pName = "TClient:FarSecond";
	FarSecond.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 100.0f);
	};
	FarSecond.m_RenderStaticLayerFn = [&](CUIRect &Col) {
		++StaticRendered;
		return ConsumeHeight(Col, 100.0f);
	};
	FarSecond.m_CanCacheStaticLayer = true;

	Loader.Register({SpacerTop, FarSecond});

	EXPECT_TRUE(Loader.PrewarmStaticRenderTargets(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f, 5.0f));
	EXPECT_EQ(StaticRendered, 0);
	EXPECT_FALSE(Loader.IsCacheValidForTests("TClient:FarSecond"));
}

TEST(SectionLoader, PrewarmStaticRenderTargetsCanIncludeFarSections)
{
	CSectionLoader Loader;
	int StaticRendered = 0;

	SSettingsSection SpacerTop;
	SpacerTop.m_pName = "TClient:SpacerTop";
	SpacerTop.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 900.0f);
	};

	SSettingsSection FarSecond;
	FarSecond.m_pName = "TClient:FarSecond";
	FarSecond.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 100.0f);
	};
	FarSecond.m_RenderStaticLayerFn = [&](CUIRect &Col) {
		++StaticRendered;
		return ConsumeHeight(Col, 100.0f);
	};
	FarSecond.m_CanCacheStaticLayer = true;

	Loader.Register({SpacerTop, FarSecond});

	EXPECT_TRUE(Loader.PrewarmStaticRenderTargets(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f, 5.0f, true));
	EXPECT_EQ(StaticRendered, 1);
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:FarSecond"));
}

TEST(SectionLoader, PaddedPrewarmedStaticRenderTargetKeepsSectionHeight)
{
	CSectionLoader Loader;
	Loader.SetProgressiveEnabled(false);
	Loader.SetRenderTargetSupportedForTests(true);

	int StaticRendered = 0;
	SSettingsSection Section;
	Section.m_pName = "TClient:Padded";
	Section.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 120.0f);
	};
	Section.m_RenderStaticLayerFn = [&](CUIRect &Col) {
		++StaticRendered;
		EXPECT_FLOAT_EQ(Col.x, 20.0f);
		EXPECT_FLOAT_EQ(Col.y, 20.0f);
		EXPECT_FLOAT_EQ(Col.w, 320.0f);
		EXPECT_FLOAT_EQ(Col.h, 120.0f);
		return ConsumeHeight(Col, 120.0f);
	};
	Section.m_CanCacheStaticLayer = true;
	Section.m_StaticCachePadding = 20.0f;
	Loader.Register({Section});

	EXPECT_TRUE(Loader.PrewarmStaticRenderTargets(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f, 5.0f));
	EXPECT_EQ(StaticRendered, 1);

	Loader.Register({Section});
	Loader.Begin(CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 5.0f);
	EXPECT_FALSE(Loader.Process());
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 120.0f);
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:Padded"));
}

TEST(SectionLoader, PrewarmSectionByNameOnlyBuildsTargetCache)
{
	CSectionLoader Loader;
	int StaticRenderedFirst = 0;
	int StaticRenderedSecond = 0;

	SSettingsSection First;
	First.m_pName = "TClient:First";
	First.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 120.0f);
	};
	First.m_RenderStaticLayerFn = [&](CUIRect &Col) {
		++StaticRenderedFirst;
		return ConsumeHeight(Col, 120.0f);
	};
	First.m_CanCacheStaticLayer = true;

	SSettingsSection Second;
	Second.m_pName = "TClient:Second";
	Second.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 80.0f);
	};
	Second.m_RenderStaticLayerFn = [&](CUIRect &Col) {
		++StaticRenderedSecond;
		return ConsumeHeight(Col, 80.0f);
	};
	Second.m_CanCacheStaticLayer = true;

	Loader.Register({First, Second});

	EXPECT_TRUE(Loader.PrewarmSectionByName("TClient:Second", CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f));
	EXPECT_EQ(StaticRenderedFirst, 0);
	EXPECT_EQ(StaticRenderedSecond, 1);
	EXPECT_FALSE(Loader.IsCacheValidForTests("TClient:First"));
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:Second"));
}

TEST(SectionLoader, DrawCachedSectionByNameRunsInteractiveLayer)
{
	CSectionLoader Loader;
	int InteractiveRenderCount = 0;

	SSettingsSection Section;
	Section.m_pName = "TClient:Interactive";
	Section.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 40.0f);
	};
	Section.m_RenderStaticLayerFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 40.0f);
	};
	Section.m_RenderInteractiveLayerFn = [&](CUIRect &Col) {
		++InteractiveRenderCount;
		return ConsumeHeight(Col, 45.0f);
	};
	Section.m_ShouldRenderInteractiveLayerFn = [](const CUIRect &) {
		return true;
	};
	Section.m_CanCacheStaticLayer = true;

	Loader.Register({Section});
	Loader.MarkCacheValidForTests("TClient:Interactive");

	EXPECT_TRUE(Loader.DrawCachedSectionByName("TClient:Interactive", CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f));
	EXPECT_EQ(InteractiveRenderCount, 1);
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 45.0f);
}

TEST(SectionLoader, DrawCachedSectionByNameKeepsInteractiveLayerVisible)
{
	CSectionLoader Loader;
	int InteractiveRenderCount = 0;

	SSettingsSection Section;
	Section.m_pName = "TClient:Interactive";
	Section.m_MeasureFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 40.0f);
	};
	Section.m_RenderStaticLayerFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 40.0f);
	};
	Section.m_RenderInteractiveLayerFn = [&](CUIRect &Col) {
		++InteractiveRenderCount;
		return ConsumeHeight(Col, 45.0f);
	};
	Section.m_ShouldRenderInteractiveLayerFn = [](const CUIRect &) {
		return false;
	};
	Section.m_CanCacheStaticLayer = true;

	Loader.Register({Section});
	Loader.MarkCacheValidForTests("TClient:Interactive");

	EXPECT_TRUE(Loader.DrawCachedSectionByName("TClient:Interactive", CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f));
	EXPECT_EQ(InteractiveRenderCount, 1);
	EXPECT_FLOAT_EQ(Loader.GetRunningColumn().y, 45.0f);
}

TEST(SectionLoader, DrawCachedSectionByNameReportsDirtyReasonAfterCacheAttempt)
{
	CSectionLoader Loader;

	SSettingsSection Section = MakeTestSection("TClient:DirtyReason", 10.0f);
	Section.m_CanCacheStaticLayer = true;
	Section.m_KeepCachedHeightStable = true;
	Section.m_RenderInteractiveLayerFn = [](CUIRect &Rect) -> float {
		return ConsumeHeight(Rect, 24.0f);
	};

	Loader.Register({Section});
	Loader.MarkCacheValidForTests("TClient:DirtyReason");

	ESettingsCacheDirtyReason DirtyReason = ESettingsCacheDirtyReason::NONE;
	EXPECT_FALSE(Loader.DrawCachedSectionByName("TClient:DirtyReason", CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f, &DirtyReason));
	EXPECT_EQ(DirtyReason, ESettingsCacheDirtyReason::WINDOW_SIZE);
}

TEST(SectionLoader, InvalidateSectionByNameClearsOnlyTargetCache)
{
	CSectionLoader Loader;

	SSettingsSection First = MakeTestSection("TClient:First", 40.0f);
	First.m_CanCacheStaticLayer = true;
	SSettingsSection Second = MakeTestSection("TClient:Second", 40.0f);
	Second.m_CanCacheStaticLayer = true;

	Loader.Register({First, Second});
	Loader.MarkCacheValidForTests("TClient:First");
	Loader.MarkCacheValidForTests("TClient:Second");

	Loader.InvalidateSectionByName("TClient:Second");

	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:First"));
	EXPECT_FALSE(Loader.IsCacheValidForTests("TClient:Second"));
}

TEST(SectionLoader, RegisterPreservesSiblingCachesWithStableSectionSet)
{
	CSectionLoader Loader;
	SSettingsSection First = MakeTestSection("TClient:First", 40.0f);
	First.m_CanCacheStaticLayer = true;
	First.m_RenderStaticLayerFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 40.0f);
	};
	SSettingsSection Second = MakeTestSection("TClient:Second", 40.0f);
	Second.m_CanCacheStaticLayer = true;
	Second.m_RenderStaticLayerFn = [](CUIRect &Col) {
		return ConsumeHeight(Col, 40.0f);
	};

	Loader.Register({First, Second});
	EXPECT_TRUE(Loader.PrewarmSectionByName("TClient:First", CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f));
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:First"));
	EXPECT_FALSE(Loader.IsCacheValidForTests("TClient:Second"));

	Loader.Register({First, Second});
	EXPECT_TRUE(Loader.PrewarmSectionByName("TClient:Second", CSectionLoader::MakeRenderTargetCacheRectForTests(320.0f, 240.0f), 0.0f));
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:First"));
	EXPECT_TRUE(Loader.IsCacheValidForTests("TClient:Second"));
}

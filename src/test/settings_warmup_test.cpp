#include <game/client/components/assets_preview_scale.h>
#include <game/client/components/menus.h>
#include <game/client/components/settings_resource_jobs.h>
#include <game/client/components/settings_warmup.h>

#include <gtest/gtest.h>

#include <fstream>
#include <limits>
#include <sstream>

TEST(SettingsWarmup, StageReadinessUsesInclusiveThreshold)
{
	EXPECT_FALSE(IsSettingsWarmupStageReady(0, 1));
	EXPECT_TRUE(IsSettingsWarmupStageReady(1, 1));
	EXPECT_TRUE(IsSettingsWarmupStageReady(-1, 5));
}

TEST(SettingsWarmup, StageAdvanceStopsAfterLastStage)
{
	EXPECT_EQ(AdvanceSettingsWarmupStage(0, 3), 1);
	EXPECT_EQ(AdvanceSettingsWarmupStage(2, 3), 3);
	EXPECT_EQ(AdvanceSettingsWarmupStage(3, 3), -1);
	EXPECT_EQ(AdvanceSettingsWarmupStage(-1, 3), -1);
	EXPECT_EQ(AdvanceSettingsWarmupStage(0, -1), -1);
}

TEST(SettingsWarmup, PrioritizesLastSessionPage)
{
	CSettingsWarmupScheduler Scheduler;
	int WarmedControls = 0;
	int WarmedTClient = 0;

	Scheduler.RegisterSection({EClassicSettingsPage::CONTROLS, "Controls:Mouse", 0, [&]() {
					   ++WarmedControls;
					   return 1.0;
				   }});
	Scheduler.RegisterSection({EClassicSettingsPage::TCLIENT, "TClient:Pet", 0, [&]() {
					   ++WarmedTClient;
					   return 1.0;
				   }});
	Scheduler.SetLastSessionPage(EClassicSettingsPage::TCLIENT);

	EXPECT_FALSE(Scheduler.WarmupFrame(1.5));
	EXPECT_EQ(WarmedControls, 0);
	EXPECT_EQ(WarmedTClient, 1);
}

TEST(SettingsWarmup, StopsAtFrameBudget)
{
	CSettingsWarmupScheduler Scheduler;
	int WarmedCount = 0;

	Scheduler.RegisterSection({EClassicSettingsPage::CONTROLS, "Controls:Mouse", 0, [&]() {
					   ++WarmedCount;
					   return 2.0;
				   }});
	Scheduler.RegisterSection({EClassicSettingsPage::CONTROLS, "Controls:Movement", 1, [&]() {
					   ++WarmedCount;
					   return 2.0;
				   }});

	EXPECT_FALSE(Scheduler.WarmupFrame(2.5));
	EXPECT_EQ(WarmedCount, 1);
	EXPECT_TRUE(Scheduler.WarmupFrame(2.5));
	EXPECT_EQ(WarmedCount, 2);
}

TEST(SettingsWarmup, DisabledSchedulerDoesNotRunSections)
{
	CSettingsWarmupScheduler Scheduler;
	int WarmedCount = 0;

	Scheduler.RegisterSection({EClassicSettingsPage::CONTROLS, "Controls:Mouse", 0, [&]() {
					   ++WarmedCount;
					   return 1.0;
				   }});
	Scheduler.SetEnabled(false);

	EXPECT_TRUE(Scheduler.WarmupFrame(10.0));
	EXPECT_EQ(WarmedCount, 0);
}

TEST(SettingsWarmup, ManySettingsSectionsRespectBudget)
{
	CSettingsWarmupScheduler Scheduler;
	int WarmedCount = 0;

	for(int i = 0; i < 20; ++i)
	{
		Scheduler.RegisterSection({EClassicSettingsPage::CONTROLS, "Controls:Bulk", i, [&]() {
						   ++WarmedCount;
						   return 1.0;
					   }});
	}
	Scheduler.SetLastSessionPage(EClassicSettingsPage::CONTROLS);

	EXPECT_FALSE(Scheduler.WarmupFrame(3.0));
	EXPECT_EQ(WarmedCount, 3);
}

TEST(SettingsWarmup, SectionCacheMetadataRequiresMatchingRuntimeKey)
{
	SSettingsSectionCacheRuntimeKey RuntimeKey;
	RuntimeKey.m_ViewportWidth = 900;
	RuntimeKey.m_ViewportHeight = 620;
	RuntimeKey.m_UiScale = 100;
	RuntimeKey.m_ConfigHash = 1234;
	RuntimeKey.m_LanguageHash = 5678;
	RuntimeKey.m_FontHash = 9012;
	RuntimeKey.m_BackendHash = 3456;
	RuntimeKey.m_WindowHash = 7890;

	SSettingsSectionCacheMetadata Metadata;
	Metadata.m_LastPage = EClassicSettingsPage::TCLIENT;
	Metadata.m_LastTab = 0;
	Metadata.m_LastScrollY = 140.0f;
	Metadata.m_SectionNameHash = 42;
	Metadata.m_SectionHeight = 180.0f;
	Metadata.m_RuntimeKey = RuntimeKey;

	EXPECT_TRUE(Metadata.Matches(RuntimeKey));

	RuntimeKey.m_ViewportHeight += 1;
	EXPECT_FALSE(Metadata.Matches(RuntimeKey));
}

TEST(SettingsWarmup, PageRuntimeCacheShortCircuitsOnlyFirstMatchingDraw)
{
	SSettingsSectionCacheRuntimeKey RuntimeKey;
	RuntimeKey.m_ViewportWidth = 900;
	RuntimeKey.m_ViewportHeight = 620;
	RuntimeKey.m_UiScale = 100;
	RuntimeKey.m_ConfigHash = 1234;

	SSettingsPageRuntimeCacheState Cache;
	Cache.m_Page = 8;
	Cache.m_Tab = 2;
	Cache.m_Width = 900;
	Cache.m_Height = 620;
	Cache.m_RuntimeKey = RuntimeKey;
	Cache.m_Valid = true;

	EXPECT_TRUE(SettingsPageRuntimeCacheShouldShortCircuit(Cache, 8, 2, 900, 620, RuntimeKey));
	EXPECT_FALSE(Cache.m_DrawnOnce);
	EXPECT_TRUE(SettingsPageRuntimeCacheShouldShortCircuit(Cache, 8, 2, 900, 620, RuntimeKey));
}

TEST(SettingsWarmup, PageRuntimeCacheRejectsMismatchedRuntimeKey)
{
	SSettingsSectionCacheRuntimeKey RuntimeKey;
	RuntimeKey.m_ViewportWidth = 900;
	RuntimeKey.m_ViewportHeight = 620;
	RuntimeKey.m_ConfigHash = 1234;

	SSettingsPageRuntimeCacheState Cache;
	Cache.m_Page = 9;
	Cache.m_Tab = 1;
	Cache.m_Width = 900;
	Cache.m_Height = 620;
	Cache.m_RuntimeKey = RuntimeKey;
	Cache.m_Valid = true;

	RuntimeKey.m_ConfigHash = 5678;
	EXPECT_FALSE(SettingsPageRuntimeCacheShouldShortCircuit(Cache, 9, 1, 900, 620, RuntimeKey));
	EXPECT_FALSE(Cache.m_DrawnOnce);
}

TEST(SettingsWarmup, PageRuntimeCacheTracksWhetherResourcesWereReadyAtRecord)
{
	SSettingsPageRuntimeCacheState Cache;
	EXPECT_TRUE(Cache.m_ResourcesReadyAtRecord);

	Cache.m_ResourcesReadyAtRecord = false;
	EXPECT_FALSE(Cache.m_ResourcesReadyAtRecord);
}

TEST(SettingsWarmup, RuntimeCacheNumericKeysRejectNonFiniteValues)
{
	EXPECT_EQ(SettingsRuntimeCacheDimensionKey(-std::numeric_limits<float>::infinity()), 1);
	EXPECT_EQ(SettingsRuntimeCachePositiveRoundedKey(-std::numeric_limits<float>::infinity()), 1);
	EXPECT_EQ(SettingsRuntimeCacheRoundedKey(std::numeric_limits<float>::infinity(), 7), 7);
}

TEST(SettingsWarmup, RuntimeCacheNumericKeysClampBeforeIntConversion)
{
	EXPECT_EQ(SettingsRuntimeCacheDimensionKey(0.0f), 1);
	EXPECT_EQ(SettingsRuntimeCacheDimensionKey(900.9f), 900);
	EXPECT_EQ(SettingsRuntimeCachePositiveRoundedKey(125.4f), 125);
	EXPECT_EQ(SettingsRuntimeCacheRoundedKey(12.6f), 13);
	EXPECT_EQ(SettingsRuntimeCacheDimensionKey(std::numeric_limits<float>::max()), std::numeric_limits<int>::max());
	EXPECT_EQ(SettingsRuntimeCacheRoundedKey(-std::numeric_limits<float>::max()), std::numeric_limits<int>::min());
}

TEST(SettingsWarmup, RuntimeFboWarmupRunsBeforeTextOnlyFallback)
{
	CSettingsWarmupScheduler Scheduler;
	std::vector<const char *> vOrder;

	Scheduler.RegisterSection({EClassicSettingsPage::TCLIENT, "TClient:Title", 1, [&]() {
					   vOrder.push_back("text");
					   return 0.1;
				   }});
	Scheduler.RegisterSection({EClassicSettingsPage::TCLIENT, "TClient:FboFirstScreen", 0, [&]() {
					   vOrder.push_back("fbo");
					   return 1.0;
				   },
		false, ESettingsWarmupKind::RUNTIME_FBO});
	Scheduler.SetLastSessionPage(EClassicSettingsPage::TCLIENT);

	EXPECT_FALSE(Scheduler.WarmupFrame(1.1));
	ASSERT_EQ(vOrder.size(), 1);
	EXPECT_STREQ(vOrder[0], "fbo");
}

TEST(SettingsWarmup, LoadingRuntimeCacheWarmupIncludesRegisteredPagesAndTClientTabs)
{
	EXPECT_EQ(CMenus::SettingsRuntimeCacheWarmupSteps(), (int)BuildSettingsPageRuntimeRegistry().m_vPages.size() +
								     6 + (CMenus::NUMBER_OF_QMCLIENT_SETTINGS_TABS - 1));
}

TEST(SettingsRuntimeCache, RegistersAllSettingsPages)
{
	const SSettingsPageRuntimeRegistry Registry = BuildSettingsPageRuntimeRegistry();

	EXPECT_FALSE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_LANGUAGE));
	EXPECT_FALSE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_PLAYER));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_TEE));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_GENERAL));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_CONTROLS));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_GRAPHICS));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_SOUND));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_DDNET));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_TCLIENT));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_QMCLIENT));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_APPEARANCE));
	EXPECT_TRUE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_ASSETS));
	EXPECT_FALSE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_PROFILES));
	EXPECT_FALSE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_CONFIGS));
	EXPECT_FALSE(SettingsPageRuntimeRegistryContains(Registry, CMenus::SETTINGS_CONTRIBUTORS));
}

TEST(SettingsWarmup, TeePageWarmupStartsSkinSourcePrewarm)
{
	std::ifstream File("src/game/client/components/menus.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t PrewarmPos = Source.find("bool CMenus::PrewarmSettingsPageResources(int Page, int Tab, const CUIRect &ContentView)");
	ASSERT_NE(PrewarmPos, std::string::npos);
	const size_t PrewarmEnd = Source.find("bool CMenus::DrawSettingsPageRuntimeCache", PrewarmPos);
	ASSERT_NE(PrewarmEnd, std::string::npos);
	const std::string PrewarmBody = Source.substr(PrewarmPos, PrewarmEnd - PrewarmPos);

	const size_t TeeBranchPos = PrewarmBody.find("else if(Page == SETTINGS_TEE)");
	ASSERT_NE(TeeBranchPos, std::string::npos);
	const size_t AssetsBranchPos = PrewarmBody.find("else if(Page == SETTINGS_ASSETS)", TeeBranchPos);
	ASSERT_NE(AssetsBranchPos, std::string::npos);
	const std::string TeeBranch = PrewarmBody.substr(TeeBranchPos, AssetsBranchPos - TeeBranchPos);

	EXPECT_NE(TeeBranch.find("SettingsTeeSkinListFirstPageWarmupEntries(ContentView.h)"), std::string::npos);
	EXPECT_EQ(TeeBranch.find("SettingsSkinListFirstPageWarmupEntries("), std::string::npos);
	EXPECT_NE(TeeBranch.find("PrewarmPlayerPreviewReady"), std::string::npos);
}

TEST(SettingsWarmup, SettingsFrameBudgetResetsBeforeUpdatePhaseConsumers)
{
	std::ifstream GameClientFile("src/game/client/gameclient.cpp");
	ASSERT_TRUE(GameClientFile.good());
	std::stringstream GameClientBuffer;
	GameClientBuffer << GameClientFile.rdbuf();
	const std::string GameClientSource = GameClientBuffer.str();

	const size_t OnUpdatePos = GameClientSource.find("void CGameClient::OnUpdate()");
	ASSERT_NE(OnUpdatePos, std::string::npos);
	const size_t OnRenderPos = GameClientSource.find("void CGameClient::OnRender()");
	ASSERT_NE(OnRenderPos, std::string::npos);
	const std::string OnUpdateBody = GameClientSource.substr(OnUpdatePos, OnRenderPos - OnUpdatePos);
	EXPECT_NE(OnUpdateBody.find("const bool TeeSettingsActive = m_Menus.IsSettingsPageActive() && g_Config.m_UiSettingsPage == CMenus::SETTINGS_TEE;"), std::string::npos);
	EXPECT_NE(OnUpdateBody.find("m_Skins.PrepareSettingsThroughputForFrame();"), std::string::npos);
	EXPECT_NE(OnUpdateBody.find("m_Menus.ResetSettingsFrameBudgetForFrame(TeeSettingsActive, FrameSkinUploadBudget);"), std::string::npos);

	std::ifstream MenusFile("src/game/client/components/menus.cpp");
	ASSERT_TRUE(MenusFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusFile.rdbuf();
	const std::string MenusSource = MenusBuffer.str();
	const size_t MenusOnRenderPos = MenusSource.find("void CMenus::OnRender()");
	ASSERT_NE(MenusOnRenderPos, std::string::npos);
	const size_t MenusOnRenderEnd = MenusSource.find("if(Client()->State() != IClient::STATE_ONLINE", MenusOnRenderPos);
	ASSERT_NE(MenusOnRenderEnd, std::string::npos);
	const std::string MenusOnRenderPreamble = MenusSource.substr(MenusOnRenderPos, MenusOnRenderEnd - MenusOnRenderPos);
	EXPECT_EQ(MenusOnRenderPreamble.find("m_SettingsFrameBudget = {};"), std::string::npos);

	std::ifstream MenusHeaderFile("src/game/client/components/menus.h");
	ASSERT_TRUE(MenusHeaderFile.good());
	std::stringstream MenusHeaderBuffer;
	MenusHeaderBuffer << MenusHeaderFile.rdbuf();
	const std::string MenusHeaderSource = MenusHeaderBuffer.str();
	EXPECT_NE(MenusHeaderSource.find("m_SettingsFrameBudget = SSettingsWarmupFrameBudget{};"), std::string::npos);
	EXPECT_NE(MenusHeaderSource.find("SettingsApplyActiveTeeSkinFrameBudget(m_SettingsFrameBudget, TeeSettingsActive);"), std::string::npos);
}

TEST(SettingsWarmup, LoadingPrewarmDoesNotPumpResourceWork)
{
	std::ifstream GameClientFile("src/game/client/gameclient.cpp");
	ASSERT_TRUE(GameClientFile.good());
	std::stringstream GameClientBuffer;
	GameClientBuffer << GameClientFile.rdbuf();
	const std::string GameClientSource = GameClientBuffer.str();

	const size_t PrewarmPos = GameClientSource.find("void CGameClient::PrewarmSettingsRuntimeCachesDuringLoading(const char *pLoadingCaption, const char *pLoadingMessage)");
	ASSERT_NE(PrewarmPos, std::string::npos);
	const size_t OnUpdatePos = GameClientSource.find("void CGameClient::OnUpdate()", PrewarmPos);
	ASSERT_NE(OnUpdatePos, std::string::npos);
	const std::string PrewarmBody = GameClientSource.substr(PrewarmPos, OnUpdatePos - PrewarmPos);

	EXPECT_NE(PrewarmBody.find("(void)pLoadingCaption;"), std::string::npos);
	EXPECT_NE(PrewarmBody.find("(void)pLoadingMessage;"), std::string::npos);
	EXPECT_NE(PrewarmBody.find("return;"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("m_Menus.PrewarmSettingsRuntimeCaches(MainView);"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("m_Skins.UpdateForSettingsWarmup();"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("m_Menus.RenderLoading(pLoadingCaption, pLoadingMessage, 0);"), std::string::npos);
	EXPECT_NE(GameClientSource.find("PrewarmSettingsRuntimeCachesDuringLoading(pLoadingDDNetCaption, pLoadingMessageAssets);"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("maximum(CMenus::SettingsRuntimeCacheWarmupSteps() * 4, 1)"), std::string::npos);
	EXPECT_EQ(PrewarmBody.find("m_Skins.OnUpdate();"), std::string::npos);
}

TEST(SettingsWarmup, RuntimePrewarmCallsitesRequireVisibleIdleSettingsPage)
{
	std::ifstream MenusFile("src/game/client/components/menus.cpp");
	ASSERT_TRUE(MenusFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusFile.rdbuf();
	const std::string MenusSource = MenusBuffer.str();

	EXPECT_EQ(MenusSource.find("const bool CanPrewarmSettings = SettingsRuntimeWarmupShouldRun(\n\t\t\t\tSettingsRuntimeCachingEnabled(g_Config.m_QmSettingsPrewarm, g_Config.m_QmSettingsFboCache, g_Config.m_QmNewUi),\n\t\t\t\ttrue,"), std::string::npos);
	EXPECT_NE(MenusSource.find("SettingsRuntimeWarmupShouldRun(\n\t\t\t\tSettingsRuntimeCachingEnabled(g_Config.m_QmSettingsPrewarm, g_Config.m_QmSettingsFboCache, g_Config.m_QmNewUi),\n\t\t\t\tm_MenuPage == PAGE_SETTINGS,"), std::string::npos);
	EXPECT_NE(MenusSource.find("SettingsRuntimeWarmupShouldRun(\n\t\t\t\tSettingsRuntimeCachingEnabled(g_Config.m_QmSettingsPrewarm, g_Config.m_QmSettingsFboCache, g_Config.m_QmNewUi),\n\t\t\t\tm_GamePage == PAGE_SETTINGS,"), std::string::npos);
	EXPECT_NE(MenusSource.find("m_SettingsPageSwitchActive || TransitionActive"), std::string::npos);
}

TEST(SettingsWarmup, QmClientRuntimePrewarmDoesNotTriggerTabSwitchAnimation)
{
	std::ifstream MenusFile("src/game/client/components/menus.cpp");
	ASSERT_TRUE(MenusFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusFile.rdbuf();
	const std::string MenusSource = MenusBuffer.str();

	std::ifstream QmMenusFile("src/game/client/components/qmclient/menus_qmclient.cpp");
	ASSERT_TRUE(QmMenusFile.good());
	std::stringstream QmMenusBuffer;
	QmMenusBuffer << QmMenusFile.rdbuf();
	const std::string QmMenusSource = QmMenusBuffer.str();

	EXPECT_NE(MenusSource.find("RenderSettingsQmClient(CacheView, false, true);"), std::string::npos);
	EXPECT_NE(QmMenusSource.find("else if(!PrewarmOnly && m_QmClientSettingsTab != s_PrevQmTab)"), std::string::npos);
	EXPECT_NE(QmMenusSource.find("const float TransitionStrength = PrewarmOnly ? 0.0f : ReadUiSwitchAnimation(QmClientTabSwitchNode);"), std::string::npos);
	EXPECT_NE(QmMenusSource.find("if(!PrewarmOnly)\n\t\t\t\tRenderSettingsTClientConfigs(ContentView);"), std::string::npos);
}

TEST(SettingsRuntimeCache, CanonicalizesMergedSettingsPages)
{
	EXPECT_EQ(SettingsCanonicalPage(CMenus::SETTINGS_LANGUAGE), CMenus::SETTINGS_GENERAL);
	EXPECT_EQ(SettingsCanonicalPage(CMenus::SETTINGS_PLAYER), CMenus::SETTINGS_TEE);
	EXPECT_EQ(SettingsCanonicalPage(CMenus::SETTINGS_CONFIGS), CMenus::SETTINGS_QMCLIENT);
	EXPECT_EQ(SettingsCanonicalPage(CMenus::SETTINGS_CONTRIBUTORS), CMenus::SETTINGS_QMCLIENT);
	EXPECT_EQ(SettingsCanonicalPage(CMenus::SETTINGS_ASSETS), CMenus::SETTINGS_ASSETS);

	EXPECT_FALSE(SettingsPageVisibleInRightTabBar(CMenus::SETTINGS_LANGUAGE));
	EXPECT_FALSE(SettingsPageVisibleInRightTabBar(CMenus::SETTINGS_PLAYER));
	EXPECT_FALSE(SettingsPageVisibleInRightTabBar(CMenus::SETTINGS_PROFILES));
	EXPECT_TRUE(SettingsPageVisibleInRightTabBar(CMenus::SETTINGS_GENERAL));
	EXPECT_TRUE(SettingsPageVisibleInRightTabBar(CMenus::SETTINGS_ASSETS));
}

TEST(SettingsRuntimeCache, BuildsStableKeysForPageSectionTextAndResource)
{
	EXPECT_EQ(SettingsPageCacheKey(CMenus::SETTINGS_TCLIENT, 2), "settings:tclient:tab:2");
	EXPECT_EQ(SettingsSectionCacheKey(CMenus::SETTINGS_TCLIENT, 2, "binds"), "settings:tclient:tab:2:section:binds");
	EXPECT_EQ(SettingsTextCacheKey(CMenus::SETTINGS_LANGUAGE, -1, "simplified_chinese.txt"), "settings:language:text:simplified_chinese.txt");
	EXPECT_NE(SettingsTextCacheKey(CMenus::SETTINGS_LANGUAGE, -1, "simplified_chinese.txt"), SettingsTextCacheKey(CMenus::SETTINGS_GENERAL, -1, "language-title"));
	EXPECT_EQ(SettingsResourceCacheKey(CMenus::SETTINGS_ASSETS, "entity_bg"), "settings:assets:resource:entity_bg");
}

TEST(SettingsRuntimeCache, BudgetStopsEveryMainThreadCost)
{
	SSettingsWarmupFrameBudget Budget;
	Budget.m_MaxTextContainers = 1;
	Budget.m_MaxRenderTargetRecords = 1;
	Budget.m_MaxGpuUploads = 1;
	Budget.m_MaxJobResultMerges = 1;

	EXPECT_TRUE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::TEXT_CONTAINER));
	EXPECT_FALSE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::TEXT_CONTAINER));
	EXPECT_EQ(Budget.m_StopReason, ESettingsWarmupStopReason::TEXT_BUDGET);

	Budget.m_StopReason = ESettingsWarmupStopReason::NONE;
	EXPECT_TRUE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::RENDER_TARGET_RECORD));
	EXPECT_FALSE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::RENDER_TARGET_RECORD));
	EXPECT_EQ(Budget.m_StopReason, ESettingsWarmupStopReason::FBO_BUDGET);

	Budget.m_StopReason = ESettingsWarmupStopReason::NONE;
	EXPECT_TRUE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::GPU_UPLOAD));
	EXPECT_FALSE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::GPU_UPLOAD));
	EXPECT_EQ(Budget.m_StopReason, ESettingsWarmupStopReason::GPU_UPLOAD_BUDGET);

	Budget.m_StopReason = ESettingsWarmupStopReason::NONE;
	EXPECT_TRUE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::JOB_RESULT_MERGE));
	EXPECT_FALSE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::JOB_RESULT_MERGE));
	EXPECT_EQ(Budget.m_StopReason, ESettingsWarmupStopReason::MERGE_BUDGET);
}

TEST(SettingsRuntimeCache, DefaultGpuBudgetAllowsOneSkinUploadBatch)
{
	SSettingsWarmupFrameBudget Budget;
	for(int Upload = 0; Upload < 14; ++Upload)
		EXPECT_TRUE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::GPU_UPLOAD));
	EXPECT_FALSE(SettingsWarmupConsumeBudget(Budget, ESettingsWarmupCost::GPU_UPLOAD));
	EXPECT_EQ(Budget.m_StopReason, ESettingsWarmupStopReason::GPU_UPLOAD_BUDGET);
}

TEST(SettingsRuntimeCache, BudgetStopReasonsMapToProductionMissReasons)
{
	EXPECT_STREQ(SettingsWarmupBudgetStopMissReasonName(ESettingsWarmupStopReason::TEXT_BUDGET), "text_budget");
	EXPECT_STREQ(SettingsWarmupBudgetStopMissReasonName(ESettingsWarmupStopReason::FBO_BUDGET), "fbo_budget");
	EXPECT_STREQ(SettingsWarmupBudgetStopMissReasonName(ESettingsWarmupStopReason::NONE), "none");
}

TEST(SettingsRuntimeCache, PageCacheSlotsExistForEveryRegisteredPage)
{
	const SSettingsPageRuntimeRegistry Registry = BuildSettingsPageRuntimeRegistry();
	for(int Page : Registry.m_vPages)
	{
		const int Tab = Page == CMenus::SETTINGS_QMCLIENT ? 0 : -1;
		EXPECT_GE(SettingsPageRuntimeCacheSlot(Page, Tab), 0) << Page;
	}
	EXPECT_NE(SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_TCLIENT, 0), SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_TCLIENT, 1));
	EXPECT_NE(SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_ASSETS, ASSETS_TAB_ENTITIES), SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_ASSETS, ASSETS_TAB_ENTITY_BG));
}

TEST(SettingsRuntimeCache, TClientPerfStageNamesAreStable)
{
	EXPECT_STREQ(SettingsTClientPerfStageName(ETClientSettingsPerfStage::TAB_SHELL), "tclient_tab_shell");
	EXPECT_STREQ(SettingsTClientPerfStageName(ETClientSettingsPerfStage::SECTION_LAYOUT), "tclient_section_layout");
	EXPECT_STREQ(SettingsTClientPerfStageName(ETClientSettingsPerfStage::TEXT_CACHE), "tclient_text_cache");
	EXPECT_STREQ(SettingsTClientPerfStageName(ETClientSettingsPerfStage::RESOURCE_PRETRIGGER), "tclient_resource_pretrigger");
	EXPECT_STREQ(SettingsTClientPerfStageName(ETClientSettingsPerfStage::STATIC_LAYER), "tclient_static_layer");
	EXPECT_STREQ(SettingsTClientPerfStageName(ETClientSettingsPerfStage::INTERACTIVE_LAYER), "tclient_interactive_layer");
}

TEST(SettingsRuntimeCache, PerfReasonNamesAreStable)
{
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::NONE), "none");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::PAGE_FBO_UNSUPPORTED), "page_fbo_unsupported");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::PAGE_FBO_NOT_READY), "page_fbo_not_ready");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::SECTION_FBO_NOT_READY), "section_fbo_not_ready");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::DEPENDENCY_NOT_READY), "dependency_not_ready");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::RESOURCE_PLAN_PENDING), "resource_plan_pending");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::JOB_RESULT_PENDING), "job_result_pending");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::GPU_UPLOAD_BUDGET), "gpu_upload_budget");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::SHARED_HEAVY_BUDGET), "shared_heavy_budget");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::UPLOAD_BYTES_BUDGET), "upload_bytes_budget");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::OVERSIZED_UPLOAD_DEFERRED), "oversized_upload_deferred");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::TEXT_BUDGET), "text_budget");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::ACTIVE_ITEM), "active_item");
	EXPECT_STREQ(SettingsWarmupMissReasonName(ESettingsWarmupMissReason::INVALID_RUNTIME_KEY), "invalid_runtime_key");
}

TEST(SettingsRuntimeCache, InvalidationReasonNamesAreStable)
{
	EXPECT_STREQ(SettingsInvalidationReasonName(ESettingsInvalidationReason::LANGUAGE_CHANGED), "language_changed");
	EXPECT_STREQ(SettingsInvalidationReasonName(ESettingsInvalidationReason::FONT_CHANGED), "font_changed");
	EXPECT_STREQ(SettingsInvalidationReasonName(ESettingsInvalidationReason::BACKEND_CHANGED), "backend_changed");
	EXPECT_STREQ(SettingsInvalidationReasonName(ESettingsInvalidationReason::WINDOW_OR_SCALE_CHANGED), "window_or_scale_changed");
	EXPECT_STREQ(SettingsInvalidationReasonName(ESettingsInvalidationReason::CONFIG_HASH_CHANGED), "config_hash_changed");
	EXPECT_STREQ(SettingsInvalidationReasonName(ESettingsInvalidationReason::SECTION_SIZE_CHANGED), "section_size_changed");
	EXPECT_STREQ(SettingsInvalidationReasonName(ESettingsInvalidationReason::RESOURCE_DIRECTORY_CHANGED), "resource_directory_changed");
}

TEST(SettingsRuntimeCache, CompactVisibleTextIsRejected)
{
	EXPECT_FALSE(SettingsRuntimeCacheAllowsVisibleCompactText("TClientPetSection"));
	EXPECT_FALSE(SettingsRuntimeCacheAllowsVisibleCompactText("Controls:Mouse"));
	EXPECT_FALSE(SettingsRuntimeCacheAllowsVisibleCompactText(nullptr));
	EXPECT_FALSE(SettingsRuntimeCacheAllowsVisibleCompactText("TClientDeferredSummary"));
	EXPECT_FALSE(SettingsRuntimeCacheAllowsVisibleCompactText("TClientCompactSummary"));
	EXPECT_FALSE(SettingsRuntimeCacheAllowsVisibleCompactText("TClientSummaryBlock"));
}

TEST(SettingsRuntimeCache, RuntimeKeyInvalidatesOnLanguageFontBackendWindowScaleAndConfig)
{
	SSettingsRuntimeCacheKey A;
	A.m_LanguageHash = 1;
	A.m_FontGeneration = 2;
	A.m_BackendGeneration = 3;
	A.m_WindowWidth = 1280;
	A.m_WindowHeight = 720;
	A.m_UiScale = 100;
	A.m_ConfigHash = 4;

	SSettingsRuntimeCacheKey B = A;
	EXPECT_TRUE(SettingsRuntimeCacheKeyMatches(A, B));

	B.m_LanguageHash = 9;
	EXPECT_FALSE(SettingsRuntimeCacheKeyMatches(A, B));
	B = A;
	B.m_FontGeneration = 9;
	EXPECT_FALSE(SettingsRuntimeCacheKeyMatches(A, B));
	B = A;
	B.m_BackendGeneration = 9;
	EXPECT_FALSE(SettingsRuntimeCacheKeyMatches(A, B));
	B = A;
	B.m_WindowWidth = 1920;
	EXPECT_FALSE(SettingsRuntimeCacheKeyMatches(A, B));
	B = A;
	B.m_UiScale = 125;
	EXPECT_FALSE(SettingsRuntimeCacheKeyMatches(A, B));
	B = A;
	B.m_ConfigHash = 9;
	EXPECT_FALSE(SettingsRuntimeCacheKeyMatches(A, B));
}

TEST(SettingsRuntimeCache, ScrollWarmupOnlyUsesMatchingScrollPage)
{
	SSettingsRuntimeCacheMetadata Metadata;
	Metadata.m_LastPage = CMenus::SETTINGS_QMCLIENT;
	Metadata.m_LastQmTab = CMenus::QMCLIENT_SETTINGS_TAB_CONFIG;
	Metadata.m_LastScrollPage = CMenus::SETTINGS_TCLIENT;
	Metadata.m_LastScrollY = 128.0f;
	Metadata.m_Valid = true;

	const SSettingsPageRuntimeRegistry Registry = BuildSettingsPageRuntimeRegistry();
	const SSettingsWarmupStartupPlan Plan = BuildSettingsWarmupStartupPlan(Metadata, Registry);

	ASSERT_FALSE(Plan.m_vPageJobs.empty());
	EXPECT_EQ(Plan.m_vPageJobs.front().m_Page, CMenus::SETTINGS_QMCLIENT);
	EXPECT_FLOAT_EQ(Plan.m_vPageJobs.front().m_ScrollY, 0.0f);

	auto It = std::find_if(Plan.m_vPageJobs.begin(), Plan.m_vPageJobs.end(), [](const SSettingsWarmupPageJob &Job) {
		return Job.m_Page == CMenus::SETTINGS_TCLIENT;
	});
	ASSERT_NE(It, Plan.m_vPageJobs.end());
	EXPECT_FLOAT_EQ(It->m_ScrollY, 0.0f);

	Metadata.m_LastPage = CMenus::SETTINGS_TCLIENT;
	const SSettingsWarmupStartupPlan TClientPlan = BuildSettingsWarmupStartupPlan(Metadata, Registry);
	ASSERT_FALSE(TClientPlan.m_vPageJobs.empty());
	EXPECT_EQ(TClientPlan.m_vPageJobs.front().m_Page, CMenus::SETTINGS_TCLIENT);
	EXPECT_FLOAT_EQ(TClientPlan.m_vPageJobs.front().m_ScrollY, 128.0f);
}

TEST(SettingsRuntimeCache, StartupPlanCoversLastPageAndAllRegisteredPages)
{
	SSettingsRuntimeCacheMetadata Metadata;
	Metadata.m_LastPage = CMenus::SETTINGS_ASSETS;
	Metadata.m_LastTClientTab = 2;
	Metadata.m_LastQmTab = 1;
	Metadata.m_LastScrollPage = CMenus::SETTINGS_TCLIENT;
	Metadata.m_LastScrollY = 128.0f;

	const SSettingsWarmupStartupPlan Plan = BuildSettingsWarmupStartupPlan(Metadata, BuildSettingsPageRuntimeRegistry());

	ASSERT_FALSE(Plan.m_vPageJobs.empty());
	EXPECT_EQ(Plan.m_vPageJobs[0].m_Page, CMenus::SETTINGS_ASSETS);
	for(int Page : BuildSettingsPageRuntimeRegistry().m_vPages)
		EXPECT_TRUE(SettingsWarmupPlanContainsPage(Plan, Page));
}

TEST(SettingsRuntimeCache, StartupPlanIncludesTeeWarmupPage)
{
	SSettingsRuntimeCacheMetadata Metadata;
	Metadata.m_LastPage = CMenus::SETTINGS_PLAYER;

	const SSettingsWarmupStartupPlan Plan = BuildSettingsWarmupStartupPlan(Metadata, BuildSettingsPageRuntimeRegistry());

	EXPECT_TRUE(SettingsWarmupPlanContainsPage(Plan, CMenus::SETTINGS_TEE));
	EXPECT_FALSE(SettingsWarmupPlanContainsPage(Plan, CMenus::SETTINGS_PLAYER));
}

TEST(SettingsRuntimeCache, RuntimeMetadataCarriesRuntimeKeyAlongsideWarmupContext)
{
	SSettingsRuntimeCacheMetadata Metadata;
	Metadata.m_LastPage = CMenus::SETTINGS_TCLIENT;
	Metadata.m_LastTClientTab = 2;
	Metadata.m_LastScrollPage = CMenus::SETTINGS_TCLIENT;
	Metadata.m_LastScrollY = 64.0f;
	Metadata.m_RuntimeKey.m_LanguageHash = 11;
	Metadata.m_RuntimeKey.m_FontGeneration = 22;
	Metadata.m_RuntimeKey.m_BackendGeneration = 33;
	Metadata.m_RuntimeKey.m_WindowWidth = 1600;
	Metadata.m_RuntimeKey.m_WindowHeight = 900;
	Metadata.m_RuntimeKey.m_UiScale = 100;
	Metadata.m_RuntimeKey.m_ConfigHash = 44;
	Metadata.m_Valid = true;

	const SSettingsWarmupStartupPlan Plan = BuildSettingsWarmupStartupPlan(Metadata, BuildSettingsPageRuntimeRegistry());

	ASSERT_FALSE(Plan.m_vPageJobs.empty());
	EXPECT_EQ(Plan.m_vPageJobs.front().m_Page, CMenus::SETTINGS_TCLIENT);
	EXPECT_EQ(Plan.m_vPageJobs.front().m_Tab, 2);
	EXPECT_FLOAT_EQ(Plan.m_vPageJobs.front().m_ScrollY, 64.0f);
	EXPECT_EQ(Metadata.m_RuntimeKey.m_WindowWidth, 1600);
	EXPECT_EQ(Metadata.m_RuntimeKey.m_ConfigHash, 44u);
}

TEST(SettingsRuntimeCache, PageCacheSlotsRejectInvalidPersistedTabs)
{
	EXPECT_EQ(SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_TCLIENT, -1), 10);
	EXPECT_EQ(SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_TCLIENT, 99), -1);
	EXPECT_EQ(SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_QMCLIENT, -1), 16);
	EXPECT_EQ(SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_QMCLIENT, CMenus::NUMBER_OF_QMCLIENT_SETTINGS_TABS), -1);
	EXPECT_EQ(SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_ASSETS, -1), 9);
	EXPECT_EQ(SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_ASSETS, NUMBER_OF_ASSETS_TABS), -1);
}

TEST(SettingsRuntimeCache, MergedPageRuntimeCacheSlotsAreCanonical)
{
	EXPECT_EQ(SettingsPageRuntimeCacheSlot(SettingsCanonicalPage(CMenus::SETTINGS_LANGUAGE), -1), SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_GENERAL, -1));
	EXPECT_EQ(SettingsPageRuntimeCacheSlot(SettingsCanonicalPage(CMenus::SETTINGS_PLAYER), -1), SettingsPageRuntimeCacheSlot(CMenus::SETTINGS_TEE, -1));
}

TEST(SettingsRuntimeCache, SectionRegistryCoversComplexPages)
{
	SSettingsSectionRegistry Registry = BuildSettingsSectionRegistry();

	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_TCLIENT, "binds"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_QMCLIENT, "general"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_CONTROLS, "movement"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_ASSETS, "resource-list"));
	EXPECT_FALSE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_LANGUAGE, "language-list"));
	EXPECT_FALSE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_PLAYER, "skin-list"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_GENERAL, "language-list"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_TEE, "country-list"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_TEE, "skin-list"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_GENERAL, "body"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_APPEARANCE, "body"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_GRAPHICS, "body"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_SOUND, "body"));
	EXPECT_TRUE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_DDNET, "body"));
	EXPECT_FALSE(SettingsSectionRegistryContains(Registry, CMenus::SETTINGS_GENERAL, "missing"));
}

TEST(SettingsRuntimeCache, TextCacheKeysAreStableAcrossFrames)
{
	EXPECT_EQ(SettingsTextCacheKey(CMenus::SETTINGS_TCLIENT, 1, "auto-reply-title"), SettingsTextCacheKey(CMenus::SETTINGS_TCLIENT, 1, "auto-reply-title"));
	EXPECT_NE(SettingsTextCacheKey(CMenus::SETTINGS_TCLIENT, 1, "auto-reply-title"), SettingsTextCacheKey(CMenus::SETTINGS_TCLIENT, 2, "auto-reply-title"));
}

TEST(SettingsRuntimeCache, SectionRegistryRequiresBothLayersForStaticFbo)
{
	const SSettingsSectionRegistry Registry = BuildSettingsSectionRegistry();
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TCLIENT, 3, "binds"));
	EXPECT_TRUE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TCLIENT, 0, "auto-reply"));
	EXPECT_TRUE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TCLIENT, 0, "pet"));
	EXPECT_TRUE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TCLIENT, 0, "theme"));
	EXPECT_TRUE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TCLIENT, 0, "misc"));
	EXPECT_TRUE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TCLIENT, 0, "hud"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TCLIENT, 1, "auto-reply"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_QMCLIENT, CMenus::QMCLIENT_SETTINGS_TAB_VISUAL, "general"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_QMCLIENT, CMenus::QMCLIENT_SETTINGS_TAB_CONFIG, "config"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_QMCLIENT, CMenus::QMCLIENT_SETTINGS_TAB_CONTRIBUTORS, "contributors"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_CONTROLS, -1, "movement"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_CONTROLS, -1, "weapons"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_CONTROLS, -1, "voting"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_ASSETS, -1, "resource-list"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_ASSETS, -1, "preview"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_GENERAL, -1, "language-list"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TEE, -1, "country-list"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TEE, -1, "skin-list"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TEE, -1, "identity"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_GENERAL, -1, "body"));
	EXPECT_FALSE(SettingsSectionCanRecordStaticFbo(Registry, CMenus::SETTINGS_TCLIENT, 0, "missing"));
}

TEST(SettingsRuntimeCache, InvalidationReasonTargetsExpectedCaches)
{
	EXPECT_TRUE(SettingsInvalidationClearsTextPool(ESettingsInvalidationReason::LANGUAGE_CHANGED));
	EXPECT_TRUE(SettingsInvalidationClearsSectionFbo(ESettingsInvalidationReason::LANGUAGE_CHANGED));
	EXPECT_TRUE(SettingsInvalidationClearsPageFbo(ESettingsInvalidationReason::LANGUAGE_CHANGED));

	EXPECT_FALSE(SettingsInvalidationClearsTextPool(ESettingsInvalidationReason::RESOURCE_DIRECTORY_CHANGED));
	EXPECT_TRUE(SettingsInvalidationClearsResourcePlan(ESettingsInvalidationReason::RESOURCE_DIRECTORY_CHANGED));
	EXPECT_TRUE(SettingsInvalidationClearsSectionFbo(ESettingsInvalidationReason::RESOURCE_DIRECTORY_CHANGED));
	EXPECT_FALSE(SettingsInvalidationClearsPageFbo(ESettingsInvalidationReason::RESOURCE_DIRECTORY_CHANGED));
	EXPECT_TRUE(SettingsInvalidationClearsPageFbo(ESettingsInvalidationReason::RESOURCE_DIRECTORY_CHANGED, CMenus::SETTINGS_ASSETS, CMenus::SETTINGS_ASSETS));
	EXPECT_FALSE(SettingsInvalidationClearsPageFbo(ESettingsInvalidationReason::RESOURCE_DIRECTORY_CHANGED, CMenus::SETTINGS_TCLIENT, CMenus::SETTINGS_ASSETS));

	EXPECT_TRUE(SettingsInvalidationClearsTextPool(ESettingsInvalidationReason::FONT_CHANGED));
	EXPECT_FALSE(SettingsInvalidationClearsResourcePlan(ESettingsInvalidationReason::FONT_CHANGED));
	EXPECT_TRUE(SettingsInvalidationClearsPageFbo(ESettingsInvalidationReason::WINDOW_OR_SCALE_CHANGED));
	EXPECT_FALSE(SettingsInvalidationClearsTextPool(ESettingsInvalidationReason::SECTION_SIZE_CHANGED));
	EXPECT_TRUE(SettingsInvalidationClearsSectionFbo(ESettingsInvalidationReason::SECTION_SIZE_CHANGED));
}

TEST(SettingsRuntimeCache, DisabledConfigBypassesWarmupAndFbo)
{
	EXPECT_FALSE(SettingsWarmupEnabled(0, 1));
	EXPECT_FALSE(SettingsWarmupEnabled(1, 0));
	EXPECT_FALSE(SettingsWarmupEnabled(0, 0));
	EXPECT_TRUE(SettingsWarmupEnabled(1, 1));
}

TEST(SettingsRuntimeCache, LegacySettingsUiBypassesRuntimeFboCaching)
{
	EXPECT_FALSE(SettingsRuntimeCachingEnabled(1, 1, 0));
	EXPECT_FALSE(SettingsRuntimeCachingEnabled(0, 1, 1));
	EXPECT_FALSE(SettingsRuntimeCachingEnabled(1, 0, 1));
	EXPECT_TRUE(SettingsRuntimeCachingEnabled(1, 1, 1));
}

TEST(SettingsResourceJobs, SkinPlanKeepsSelectedFavoritesThenSorted)
{
	std::vector<SSettingsSkinListEntry> vEntries = {
		{"zeta", false, false},
		{"alpha", false, true},
		{"selected", true, false},
	};
	const SSettingsSkinListPlan Plan = BuildSettingsSkinListPlan(vEntries);

	ASSERT_EQ(Plan.m_vNames.size(), 3u);
	EXPECT_EQ(Plan.m_vNames[0], "selected");
	EXPECT_EQ(Plan.m_vNames[1], "alpha");
	EXPECT_EQ(Plan.m_vNames[2], "zeta");
}

TEST(SettingsResourceJobs, SkinPreviewFitsInsideListRow)
{
	EXPECT_FLOAT_EQ(SettingsSkinPreviewSize(50.0f, 60.0f, 50.0f), 40.0f);
	EXPECT_FLOAT_EQ(SettingsSkinPreviewSize(34.0f, 60.0f, 50.0f), 24.0f);
	EXPECT_FLOAT_EQ(SettingsSkinPreviewSize(50.0f, 24.0f, 50.0f), 14.0f);
	EXPECT_FLOAT_EQ(SettingsSkinPreviewSize(50.0f, 60.0f, 50.0f, 50.0f, 70.0f), 50.0f * (40.0f / 70.0f));
	EXPECT_FLOAT_EQ(SettingsSkinPreviewSize(50.0f, 35.0f, 50.0f, 70.0f, 40.0f), 50.0f * (25.0f / 70.0f));
	EXPECT_FLOAT_EQ(SettingsSkinPreviewCenterOffset(-10.0f, 30.0f), -10.0f);
	EXPECT_FLOAT_EQ(SettingsSkinPreviewCenterOffset(-35.0f, 15.0f), 10.0f);
}

TEST(SettingsResourceJobs, CachedSkinPreviewCountsAsReady)
{
	EXPECT_FALSE(SettingsSkinListEntryReady(false, false, false));
	EXPECT_TRUE(SettingsSkinListEntryReady(true, false, false));
	EXPECT_TRUE(SettingsSkinListEntryReady(false, true, false));
	EXPECT_TRUE(SettingsSkinListEntryReady(false, false, true));
}

TEST(SettingsResourceJobs, CountryFlagPlanDeduplicatesAndKeepsOrder)
{
	const std::vector<int> vPlan = BuildSettingsCountryFlagWarmupPlan({156, 840, 156, -1});
	ASSERT_EQ(vPlan.size(), 3u);
	EXPECT_EQ(vPlan[0], 156);
	EXPECT_EQ(vPlan[1], 840);
	EXPECT_EQ(vPlan[2], -1);
}

TEST(SettingsResourceJobs, ResourceMergeBudgetStopsBatchWork)
{
	SSettingsResourceMergeBudget Budget;
	Budget.m_MaxListEntries = 2;
	Budget.m_MaxGpuUploads = 1;

	EXPECT_TRUE(SettingsResourceConsumeMergeEntry(Budget));
	EXPECT_TRUE(SettingsResourceConsumeMergeEntry(Budget));
	EXPECT_FALSE(SettingsResourceConsumeMergeEntry(Budget));
	EXPECT_EQ(Budget.m_StopReason, ESettingsWarmupStopReason::MERGE_BUDGET);

	Budget.m_StopReason = ESettingsWarmupStopReason::NONE;
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(Budget));
	EXPECT_FALSE(SettingsResourceConsumeGpuUpload(Budget));
	EXPECT_EQ(Budget.m_StopReason, ESettingsWarmupStopReason::GPU_UPLOAD_BUDGET);
}

TEST(SettingsResourceJobs, ResourceMergeBudgetHonorsUnifiedFrameBudget)
{
	SSettingsResourceMergeBudget ResourceBudget;
	ResourceBudget.m_MaxListEntries = 8;
	SSettingsWarmupFrameBudget FrameBudget;
	FrameBudget.m_MaxJobResultMerges = 1;

	EXPECT_TRUE(SettingsResourceConsumeMergeEntry(ResourceBudget, &FrameBudget));
	EXPECT_TRUE(SettingsResourceConsumeMergeEntry(ResourceBudget, &FrameBudget));
	EXPECT_EQ(ResourceBudget.m_MaxListEntries, 6);
	EXPECT_EQ(ResourceBudget.m_StopReason, ESettingsWarmupStopReason::NONE);

	SSettingsResourceMergeBudget NextBatchBudget;
	NextBatchBudget.m_MaxListEntries = 8;
	EXPECT_FALSE(SettingsResourceConsumeMergeEntry(NextBatchBudget, &FrameBudget));
	EXPECT_EQ(NextBatchBudget.m_MaxListEntries, 8);
	EXPECT_EQ(NextBatchBudget.m_StopReason, ESettingsWarmupStopReason::MERGE_BUDGET);
	EXPECT_EQ(FrameBudget.m_StopReason, ESettingsWarmupStopReason::MERGE_BUDGET);
}

TEST(SettingsResourceJobs, ResourceGpuUploadBudgetHonorsUnifiedFrameBudget)
{
	SSettingsResourceMergeBudget ResourceBudget;
	ResourceBudget.m_MaxGpuUploads = 8;
	SSettingsWarmupFrameBudget FrameBudget;
	FrameBudget.m_MaxGpuUploads = 1;

	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(ResourceBudget, &FrameBudget));
	EXPECT_FALSE(SettingsResourceConsumeGpuUpload(ResourceBudget, &FrameBudget));
	EXPECT_EQ(ResourceBudget.m_StopReason, ESettingsWarmupStopReason::GPU_UPLOAD_BUDGET);
	EXPECT_EQ(FrameBudget.m_StopReason, ESettingsWarmupStopReason::GPU_UPLOAD_BUDGET);
	EXPECT_EQ(ResourceBudget.m_MaxGpuUploads, 7);
}

TEST(SettingsResourceJobs, ResourceGpuUploadBudgetCanReserveMultipleUploads)
{
	SSettingsResourceMergeBudget ResourceBudget;
	ResourceBudget.m_MaxGpuUploads = 8;
	SSettingsWarmupFrameBudget FrameBudget;
	FrameBudget.m_MaxGpuUploads = 3;

	EXPECT_TRUE(SettingsResourceConsumeGpuUploads(ResourceBudget, &FrameBudget, 3));
	EXPECT_EQ(ResourceBudget.m_MaxGpuUploads, 5);
	EXPECT_FALSE(SettingsResourceConsumeGpuUploads(ResourceBudget, &FrameBudget, 1));
	EXPECT_EQ(ResourceBudget.m_MaxGpuUploads, 5);
	EXPECT_EQ(FrameBudget.m_StopReason, ESettingsWarmupStopReason::GPU_UPLOAD_BUDGET);
}

TEST(SettingsResourceJobs, DefaultSettingsGpuBudgetAllowsOneSkinUploadBatch)
{
	SSettingsResourceMergeBudget ResourceBudget;
	ResourceBudget.m_MaxGpuUploads = 42;
	SSettingsWarmupFrameBudget FrameBudget;

	EXPECT_TRUE(SettingsResourceConsumeGpuUploads(ResourceBudget, &FrameBudget, 14));
	EXPECT_FALSE(SettingsResourceConsumeGpuUploads(ResourceBudget, &FrameBudget, 14));
	EXPECT_EQ(ResourceBudget.m_MaxGpuUploads, 28);
	EXPECT_EQ(FrameBudget.m_StopReason, ESettingsWarmupStopReason::GPU_UPLOAD_BUDGET);
}

TEST(SettingsResourceJobs, AssetListLoadingDoesNotBlockVisibleEntries)
{
	EXPECT_TRUE(SettingsAssetListShouldShowBlockingLoading(true, 0));
	EXPECT_FALSE(SettingsAssetListShouldShowBlockingLoading(true, 1));
	EXPECT_FALSE(SettingsAssetListShouldShowBlockingLoading(false, 0));
}

TEST(SettingsResourceJobs, AssetPreviewBudgetedTextureSizeFitsUploadBudget)
{
	const int TextureSize = SettingsAssetPreviewBudgetedTextureSize(
		LOCAL_ASSET_PREVIEW_MAX_TEXTURE_SIZE,
		ASSET_PREVIEW_MIN_TEXTURE_SIZE,
		512ull * 1024ull * 1024ull,
		0,
		0);
	EXPECT_EQ(TextureSize, ASSET_PREVIEW_MIN_TEXTURE_SIZE);
	EXPECT_LE(PreviewTextureSizeBytesEstimate(TextureSize), ASSET_PREVIEW_UPLOAD_MAX_BYTES_PER_FRAME);
}

TEST(SettingsResourceJobs, AssetPreviewDecodeCanStartWhileMerging)
{
	EXPECT_FALSE(SettingsAssetListCanStartPreviewDecode(true, false, false));
	EXPECT_FALSE(SettingsAssetListCanStartPreviewDecode(false, true, false));
	EXPECT_TRUE(SettingsAssetListCanStartPreviewDecode(false, false, true));
	EXPECT_FALSE(SettingsAssetListCanStartPreviewDecode(false, false, false));
}

TEST(SettingsResourceJobs, AssetPreviewFinalizeBudgetDefersAfterLimit)
{
	EXPECT_FALSE(SettingsAssetPreviewShouldDeferFinalize(0, 10.0, 2, 4.0));
	EXPECT_FALSE(SettingsAssetPreviewShouldDeferFinalize(1, 3.5, 2, 4.0));
	EXPECT_TRUE(SettingsAssetPreviewShouldDeferFinalize(1, 4.0, 2, 4.0));
	EXPECT_TRUE(SettingsAssetPreviewShouldDeferFinalize(2, 0.0, 2, 4.0));
}

TEST(SettingsResourceJobs, InactiveWindowBlocksAllAssetStarts)
{
	EXPECT_TRUE(SettingsAssetWorkAllowedWhileWindowInactive(true, false));
	EXPECT_TRUE(SettingsAssetWorkAllowedWhileWindowInactive(true, true));
	EXPECT_FALSE(SettingsAssetWorkAllowedWhileWindowInactive(false, true));
	EXPECT_FALSE(SettingsAssetWorkAllowedWhileWindowInactive(false, false));
}

TEST(SettingsResourceJobs, AssetPreviewPrioritizesCurrentVisibleRange)
{
	EXPECT_FALSE(SettingsAssetPreviewShouldPrioritizeVisibleRange(9, 10, 20));
	EXPECT_TRUE(SettingsAssetPreviewShouldPrioritizeVisibleRange(10, 10, 20));
	EXPECT_TRUE(SettingsAssetPreviewShouldPrioritizeVisibleRange(15, 10, 20));
	EXPECT_TRUE(SettingsAssetPreviewShouldPrioritizeVisibleRange(20, 10, 20));
	EXPECT_FALSE(SettingsAssetPreviewShouldPrioritizeVisibleRange(21, 10, 20));
	EXPECT_FALSE(SettingsAssetPreviewShouldPrioritizeVisibleRange(10, -1, 20));
}

TEST(SettingsResourceJobs, WorkshopThumbDecodePrioritizesVisibleDownloadableItems)
{
	EXPECT_TRUE(SettingsWorkshopThumbShouldStartHighPriority(0, 0, 3));
	EXPECT_TRUE(SettingsWorkshopThumbShouldStartHighPriority(3, 0, 3));
	EXPECT_FALSE(SettingsWorkshopThumbShouldStartHighPriority(4, 0, 3));
	EXPECT_FALSE(SettingsWorkshopThumbShouldStartHighPriority(0, -1, 3));
}

TEST(SettingsResourceJobs, VisibleResourceStartsCanUsePriorityBudget)
{
	EXPECT_TRUE(SettingsResourceCanUseHighPriorityBudget(5, 6, 12, false));
	EXPECT_FALSE(SettingsResourceCanUseHighPriorityBudget(6, 6, 12, false));
	EXPECT_TRUE(SettingsResourceCanUseHighPriorityBudget(6, 6, 12, true));
	EXPECT_FALSE(SettingsResourceCanUseHighPriorityBudget(12, 6, 12, true));
}

TEST(SettingsResourceJobs, UploadByteBudgetRejectsOversizedFirstUpload)
{
	EXPECT_FALSE(SettingsResourceUploadWithinByteBudget(0, 0, 4 * 1024 * 1024, 1 * 1024 * 1024));
	EXPECT_TRUE(SettingsResourceUploadWithinByteBudget(0, 0, 512 * 1024, 1 * 1024 * 1024));
	EXPECT_FALSE(SettingsResourceUploadWithinByteBudget(1, 512 * 1024, 768 * 1024, 1 * 1024 * 1024));
}

TEST(SettingsResourceJobs, OversizedUploadAlwaysDeferredToAvoidFocusBurst)
{
	SSettingsResourceFrameContext IdleVisible{};
	IdleVisible.m_ScrollActive = false;
	IdleVisible.m_PostScrollRecoveryFrames = 0;

	SSettingsResourceFrameContext ScrollActive = IdleVisible;
	ScrollActive.m_ScrollActive = true;

	SSettingsResourceFrameContext Recovery = IdleVisible;
	Recovery.m_PostScrollRecoveryFrames = 2;

	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(IdleVisible, false, ESettingsResourcePriority::VISIBLE, 0, 4 * 1024 * 1024, 1 * 1024 * 1024));
	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(IdleVisible, true, ESettingsResourcePriority::VISIBLE, 0, 4 * 1024 * 1024, 1 * 1024 * 1024));
	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(ScrollActive, false, ESettingsResourcePriority::VISIBLE, 0, 4 * 1024 * 1024, 1 * 1024 * 1024));
	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(Recovery, false, ESettingsResourcePriority::VISIBLE, 0, 4 * 1024 * 1024, 1 * 1024 * 1024));
	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(IdleVisible, false, ESettingsResourcePriority::BACKGROUND, 0, 4 * 1024 * 1024, 1 * 1024 * 1024));
	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(IdleVisible, false, ESettingsResourcePriority::VISIBLE, 0, 512 * 1024, 1 * 1024 * 1024));
}

TEST(SettingsResourceJobs, OversizedUploadNeverGetsStableFrameException)
{
	SSettingsResourceFrameContext Stable{};
	Stable.m_ScrollActive = false;
	Stable.m_PostScrollRecoveryFrames = 0;

	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(Stable, false, ESettingsResourcePriority::VISIBLE, 0, 2 * 1024 * 1024, 1 * 1024 * 1024));
	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(Stable, false, ESettingsResourcePriority::VISIBLE, 1, 2 * 1024 * 1024, 1 * 1024 * 1024));
}

TEST(SettingsResourceJobs, VisibleNormalSizedUploadStillUsesByteBudget)
{
	SSettingsResourceFrameContext Stable{};
	Stable.m_ScrollActive = false;
	Stable.m_PostScrollRecoveryFrames = 0;

	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(Stable, false, ESettingsResourcePriority::VISIBLE, 1, 512 * 1024, 1 * 1024 * 1024));
	EXPECT_FALSE(SettingsResourceUploadWithinByteBudget(1, 768 * 1024, 512 * 1024, 1 * 1024 * 1024));
}

TEST(SettingsResourceJobs, ScrollActiveResourceStageBudgetBlocksBackgroundUploads)
{
	SSettingsResourceFrameContext ScrollActive{};
	ScrollActive.m_ScrollActive = true;
	ScrollActive.m_PostScrollRecoveryFrames = 0;

	EXPECT_EQ(SettingsResourceFrameStageBudget(ScrollActive, ESettingsResourcePriority::BACKGROUND, 4, 1), 0);
	EXPECT_EQ(SettingsResourceFrameStageBudget(ScrollActive, ESettingsResourcePriority::VISIBLE, 4, 1), 1);
}

TEST(SettingsResourceJobs, PostScrollRecoveryDefersOversizedUploads)
{
	SSettingsResourceFrameContext Recovery{};
	Recovery.m_ScrollActive = false;
	Recovery.m_PostScrollRecoveryFrames = 3;

	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(Recovery, false, ESettingsResourcePriority::VISIBLE, 0, 2 * 1024 * 1024, 1 * 1024 * 1024));
}

TEST(SettingsResourceJobs, ImmediateScrollInputBlocksHeavyAssetsWorkBeforePersistentStateCatchesUp)
{
	const SSettingsResourceFrameContext Idle = SettingsBuildFrameContext(false, false, 0);
	const SSettingsResourceFrameContext ImmediateScroll = SettingsBuildFrameContext(false, true, 0);

	EXPECT_EQ(SettingsResourceSharedHeavyBudget(Idle, 4, 1), 4);
	EXPECT_EQ(SettingsResourceSharedHeavyBudget(ImmediateScroll, 4, 1), 0);
	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(ImmediateScroll, false, ESettingsResourcePriority::VISIBLE, 0, 2 * 1024 * 1024, 1 * 1024 * 1024));
}

TEST(SettingsResourceJobs, PostListScrollStateClampsStaleIdleHeavyBudgetBeforeAssetsFinalize)
{
	const SSettingsResourceFrameContext PreListIdle = SettingsBuildFrameContext(false, false, 0);
	const int PreListHeavyBudget = SettingsResourceSharedHeavyBudget(PreListIdle, 4, 1);
	const SSettingsResourceFrameContext PostListScroll = SettingsBuildFrameContext(false, true, 0);

	EXPECT_EQ(PreListHeavyBudget, 4);
	EXPECT_EQ(SettingsResourceClampSharedHeavyBudget(PreListHeavyBudget, PostListScroll, 4, 1), 0);
}

TEST(SettingsResourceJobs, PostListRecoveryStateClampsStaleIdleHeavyBudgetForWorkshopThumbs)
{
	const SSettingsResourceFrameContext PreListIdle = SettingsBuildFrameContext(false, false, 0);
	const int PreListHeavyBudget = SettingsResourceSharedHeavyBudget(PreListIdle, 4, 1);
	const SSettingsResourceFrameContext PostListRecovery = SettingsBuildFrameContext(false, false, 2);

	EXPECT_EQ(PreListHeavyBudget, 4);
	EXPECT_EQ(SettingsResourceClampSharedHeavyBudget(PreListHeavyBudget, PostListRecovery, 4, 1), 1);
}

TEST(SettingsResourceJobs, AssetsLocalListFinalizesHeavyPreviewWorkOnlyAfterListEnd)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t LocalListPos = Source.find("if(!UsesCombinedAssetList(pCurrentCategory))");
	ASSERT_NE(LocalListPos, std::string::npos);
	const size_t LocalListEnd = Source.find("auto ResetSelectedAssetToDefault = [&](const char *pDeletedName) {", LocalListPos);
	ASSERT_NE(LocalListEnd, std::string::npos);
	const std::string LocalListBody = Source.substr(LocalListPos, LocalListEnd - LocalListPos);

	const size_t DoEndPos = LocalListBody.find("const int NewSelected = s_ListBox.DoEnd();");
	const size_t ScrollPos = LocalListBody.find("const bool ListScrollActive = s_ListBox.ScrollbarActive() || s_ListBox.ScrollbarAnimating();");
	const size_t FrameContextPos = LocalListBody.find("const SSettingsResourceFrameContext PreviewUploadFrameContext = SettingsBuildFrameContext(");
	const size_t ClampPos = LocalListBody.find("RemainingHeavyResourceBatches = SettingsResourceClampSharedHeavyBudget(");
	const size_t FinalizePos = LocalListBody.find("FinalizeReadyPreviewDecodes(PreviewUploadFrameContext);");
	const size_t DrainPos = LocalListBody.find("DrainReadyPreviewUploadsAfterList(PreviewUploadFrameContext);");

	ASSERT_NE(DoEndPos, std::string::npos);
	ASSERT_NE(ScrollPos, std::string::npos);
	ASSERT_NE(FrameContextPos, std::string::npos);
	ASSERT_NE(ClampPos, std::string::npos);
	ASSERT_NE(FinalizePos, std::string::npos);
	ASSERT_NE(DrainPos, std::string::npos);

	EXPECT_LT(DoEndPos, ScrollPos);
	EXPECT_LT(ScrollPos, FrameContextPos);
	EXPECT_LT(FrameContextPos, ClampPos);
	EXPECT_LT(ClampPos, FinalizePos);
	EXPECT_LT(FinalizePos, DrainPos);
}

TEST(SettingsResourceJobs, AssetsWorkshopListFinalizesPreviewAndThumbWorkOnlyAfterListEnd)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t WorkshopListPos = Source.find("static CListBox s_WorkshopAssetsListBox;");
	ASSERT_NE(WorkshopListPos, std::string::npos);
	const size_t WorkshopListEnd = Source.find("if(DeleteLocalRequested)", WorkshopListPos);
	ASSERT_NE(WorkshopListEnd, std::string::npos);
	const std::string WorkshopListBody = Source.substr(WorkshopListPos, WorkshopListEnd - WorkshopListPos);

	const size_t DoEndPos = WorkshopListBody.find("const int NewCombinedSelected = s_WorkshopAssetsListBox.DoEnd();");
	const size_t ScrollPos = WorkshopListBody.find("const bool WorkshopListScrollActive = s_WorkshopAssetsListBox.ScrollbarActive() || s_WorkshopAssetsListBox.ScrollbarAnimating();");
	const size_t FrameContextPos = WorkshopListBody.find("const SSettingsResourceFrameContext WorkshopUploadFrameContext = SettingsBuildFrameContext(");
	const size_t ClampPos = WorkshopListBody.find("RemainingHeavyResourceBatches = SettingsResourceClampSharedHeavyBudget(");
	const size_t PreviewFinalizePos = WorkshopListBody.find("FinalizeReadyPreviewDecodes(WorkshopUploadFrameContext);");
	const size_t PreviewDrainPos = WorkshopListBody.find("DrainReadyPreviewUploadsAfterList(WorkshopUploadFrameContext);");
	const size_t ThumbFinalizePos = WorkshopListBody.find("FinalizeWorkshopReadyThumbs(WorkshopUploadFrameContext);");
	const size_t ThumbDrainPos = WorkshopListBody.find("DrainWorkshopReadyThumbUploads(WorkshopUploadFrameContext);");

	ASSERT_NE(DoEndPos, std::string::npos);
	ASSERT_NE(ScrollPos, std::string::npos);
	ASSERT_NE(FrameContextPos, std::string::npos);
	ASSERT_NE(ClampPos, std::string::npos);
	ASSERT_NE(PreviewFinalizePos, std::string::npos);
	ASSERT_NE(PreviewDrainPos, std::string::npos);
	ASSERT_NE(ThumbFinalizePos, std::string::npos);
	ASSERT_NE(ThumbDrainPos, std::string::npos);

	EXPECT_LT(DoEndPos, ScrollPos);
	EXPECT_LT(ScrollPos, FrameContextPos);
	EXPECT_LT(FrameContextPos, ClampPos);
	EXPECT_LT(ClampPos, PreviewFinalizePos);
	EXPECT_LT(PreviewFinalizePos, PreviewDrainPos);
	EXPECT_LT(PreviewDrainPos, ThumbFinalizePos);
	EXPECT_LT(ThumbFinalizePos, ThumbDrainPos);
}

TEST(SettingsResourceJobs, AssetsListsBuildFrameContextFromJumpScrollStateBeforeHeavyStages)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("const bool ListJumpScrollActive ="), std::string::npos);
	EXPECT_NE(Source.find("const bool WorkshopListJumpScrollActive ="), std::string::npos);
	EXPECT_NE(Source.find("s_AssetsScrollCooldownFrames > 0, ListScrollActive, ListJumpScrollActive, s_AssetsPostScrollRecoveryFrames"), std::string::npos);
	EXPECT_NE(Source.find("s_AssetsScrollCooldownFrames > 0, WorkshopListScrollActive, WorkshopListJumpScrollActive, s_AssetsPostScrollRecoveryFrames"), std::string::npos);
	EXPECT_NE(Source.find("frame_context=%s jump_scroll=%d"), std::string::npos);
}

TEST(SettingsResourceJobs, VisibleReadyPreviewKeepsUploadPriority)
{
	EXPECT_TRUE(SettingsAssetPreviewShouldPrioritizeVisibleRange(3, 3, 5));
	EXPECT_FALSE(SettingsAssetPreviewShouldPrioritizeVisibleRange(2, 3, 5));
	EXPECT_TRUE(SettingsAssetPreviewShouldUploadHighPriorityFirst(false, true));
	EXPECT_FALSE(SettingsAssetPreviewShouldUploadHighPriorityFirst(true, false));
	EXPECT_FALSE(SettingsAssetPreviewShouldUploadHighPriorityFirst(false, false));
}

TEST(SettingsResourceJobs, AssetsFocusHandlingDoesNotUseWindowRecoveryFrames)
{
	std::ifstream MenusHeaderFile("src/game/client/components/menus.h");
	ASSERT_TRUE(MenusHeaderFile.good());
	std::stringstream MenusHeaderBuffer;
	MenusHeaderBuffer << MenusHeaderFile.rdbuf();
	const std::string MenusHeaderSource = MenusHeaderBuffer.str();

	EXPECT_EQ(MenusHeaderSource.find("m_LastWindowActive"), std::string::npos);
	EXPECT_EQ(MenusHeaderSource.find("m_WindowRecoveryFrames"), std::string::npos);

	std::ifstream MenusSourceFile("src/game/client/components/menus.cpp");
	ASSERT_TRUE(MenusSourceFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusSourceFile.rdbuf();
	const std::string MenusSource = MenusBuffer.str();

	EXPECT_EQ(MenusSource.find("m_WindowRecoveryFrames = 10"), std::string::npos);
	EXPECT_EQ(MenusSource.find("m_LastWindowActive = CurrentWindowActive"), std::string::npos);
}

TEST(SettingsResourceJobs, AssetsInactiveWindowBehaviorSkipsRecoveryPurgeAndUsesDirectWindowGate)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_EQ(Source.find("Graphics()->UnloadTexture(&Entity.m_RenderTexture);"), std::string::npos);
	EXPECT_EQ(Source.find("EffectiveMaxPreviewUploadsPerFrame = m_WindowRecoveryFrames > 0 ? 0 : MaxPreviewUploadsPerFrame"), std::string::npos);
	EXPECT_EQ(Source.find("EffectiveMaxWorkshopThumbUploadsPerFrame = m_WindowRecoveryFrames > 0 ? 0 : MaxWorkshopThumbUploadsPerFrame"), std::string::npos);
	EXPECT_NE(Source.find("const bool WindowActive = pEngineGraphics == nullptr || pEngineGraphics->WindowActive() != 0;"), std::string::npos);
	EXPECT_NE(Source.find("if(!SettingsAssetWorkAllowedWhileWindowInactive(WindowActive, HighPriority))"), std::string::npos);
	EXPECT_NE(Source.find("if(!SettingsAssetWorkAllowedWhileWindowInactive(WindowActive, Asset.m_ThumbHighPriority))"), std::string::npos);
	EXPECT_NE(Source.find("if(!WindowActive)\n\t\t\treturn;"), std::string::npos);
	EXPECT_NE(Source.find("LogAssetsPerfStage(\"assets_window_focus\""), std::string::npos);
}

TEST(SettingsResourceJobs, InactiveWindowBlocksAllNewAssetWorkStarts)
{
	EXPECT_FALSE(SettingsAssetWorkAllowedWhileWindowInactive(false, false));
	EXPECT_FALSE(SettingsAssetWorkAllowedWhileWindowInactive(false, true));
	EXPECT_TRUE(SettingsAssetWorkAllowedWhileWindowInactive(true, false));
	EXPECT_TRUE(SettingsAssetWorkAllowedWhileWindowInactive(true, true));
}

TEST(SettingsResourceJobs, AssetsFocusLogsIncludeTextureMemoryAndResidentPreviewBytes)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("TextureMemoryUsage()"), std::string::npos);
	EXPECT_NE(Source.find("resident_preview_bytes"), std::string::npos);
	EXPECT_NE(Source.find("workshop_resident_preview_bytes"), std::string::npos);
}

TEST(SettingsResourceJobs, EntityBgCorruptInstallProbeReadsOnlyFileHeader)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("IOHANDLE File = pStorage->OpenFile(Asset.m_InstallPath.c_str(), IOFLAG_READ, IStorage::TYPE_SAVE);"), std::string::npos);
	EXPECT_NE(Source.find("unsigned char aHeader[16] = {};"), std::string::npos);
	EXPECT_NE(Source.find("const unsigned BytesRead = io_read(File, aHeader, sizeof(aHeader));"), std::string::npos);
	EXPECT_EQ(Source.find("pStorage->ReadFile(Asset.m_InstallPath.c_str(), IStorage::TYPE_SAVE, &pFileData, &FileSize)"), std::string::npos);
}

TEST(SettingsResourceJobs, AssetsFocusObservationUsesResumeFrameContextAndSwapTelemetry)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("focus_resume=%d"), std::string::npos);
	EXPECT_NE(Source.find("graphics_swap"), std::string::npos);
	EXPECT_NE(Source.find("LogAssetsPerfStageForClient(Client(), \"assets_focus_observation\""), std::string::npos);
}

TEST(SettingsResourceJobs, WorkshopThumbStartAvoidsDuplicateQueuePushForMatchingState)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("Asset.m_ThumbQueuedTier"), std::string::npos);
	EXPECT_NE(Source.find("Asset.m_ThumbQueuedEpoch"), std::string::npos);
	EXPECT_NE(Source.find("Asset.m_ThumbQueuedTab"), std::string::npos);
}

TEST(SettingsResourceJobs, PreviewTierUpgradeReplacesExistingTexturesInsteadOfLeakingOrDropping)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("static void ReplaceCustomItemPreviewTexture"), std::string::npos);
	EXPECT_NE(Source.find("static void ReplaceWorkshopThumbTexture"), std::string::npos);
	EXPECT_NE(Source.find("pGraphics->UnloadTexture(&Item.m_RenderTexture);"), std::string::npos);
	EXPECT_NE(Source.find("pGraphics->UnloadTexture(&Asset.m_ThumbTexture);"), std::string::npos);
	EXPECT_NE(Source.find("Item.m_PreviewResidentBytes = 0;"), std::string::npos);
	EXPECT_NE(Source.find("SettingsAssetPreviewResidentTextureSatisfiesRequest(\n\t\t\t\t\t\ttrue,\n\t\t\t\t\t\tpAsset->m_ThumbResidentBytes,\n\t\t\t\t\t\tpAsset->m_ThumbRequestedTextureSize)"), std::string::npos);
}

TEST(SettingsResourceJobs, WorkshopRefreshPreservesPreviewRuntimeMetadata)
{
	std::ifstream File("src/game/client/components/menus_settings_assets.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	EXPECT_NE(Source.find("NewAsset.m_ThumbRequestedTextureSize = ExistingAsset.m_ThumbRequestedTextureSize;"), std::string::npos);
	EXPECT_NE(Source.find("NewAsset.m_ThumbResidentBytes = ExistingAsset.m_ThumbResidentBytes;"), std::string::npos);
	EXPECT_NE(Source.find("NewAsset.m_ThumbQueuedTier = ExistingAsset.m_ThumbQueuedTier;"), std::string::npos);
	EXPECT_NE(Source.find("NewAsset.m_ThumbQueuedEpoch = ExistingAsset.m_ThumbQueuedEpoch;"), std::string::npos);
	EXPECT_NE(Source.find("NewAsset.m_ThumbQueuedTab = ExistingAsset.m_ThumbQueuedTab;"), std::string::npos);
}

TEST(SettingsResourceJobs, BudgetedPreviewCanUpgradeTierWhenHigherBudgetReturns)
{
	EXPECT_FALSE(SettingsAssetPreviewResidentTextureSatisfiesRequest(true, PreviewTextureSizeBytesEstimate(512), 1024));
	EXPECT_TRUE(SettingsAssetPreviewResidentTextureSatisfiesRequest(true, PreviewTextureSizeBytesEstimate(1024), 1024));
	EXPECT_TRUE(SettingsAssetPreviewDecodeStartNeeded(false, true, PreviewTextureSizeBytesEstimate(512), 1024, false));
	EXPECT_FALSE(SettingsAssetPreviewDecodeStartNeeded(false, true, PreviewTextureSizeBytesEstimate(1024), 1024, false));
	EXPECT_FALSE(SettingsAssetPreviewDecodeStartNeeded(true, false, 0, 1024, false));
	EXPECT_FALSE(SettingsAssetPreviewDecodeStartNeeded(false, false, 0, 1024, true));
}

TEST(SettingsResourceJobs, PageCacheRejectsRecordedFrameWithoutReadyResources)
{
	EXPECT_FALSE(SettingsPageCacheCanUseRecordedResources(true, true, false));
	EXPECT_FALSE(SettingsPageCacheCanUseRecordedResources(true, false, true));
	EXPECT_FALSE(SettingsPageCacheCanUseRecordedResources(false, true, true));
	EXPECT_TRUE(SettingsPageCacheCanUseRecordedResources(true, true, true));
}

TEST(SettingsResourceJobs, PageCacheRejectsRecordedFrameWithoutReadyDependentSubcaches)
{
	EXPECT_FALSE(SettingsPageCacheCanUseRecordedResources(true, true, true, false));
	EXPECT_EQ(SettingsPageRecordedCacheMissReason(true, true, true, false), ESettingsWarmupMissReason::DEPENDENCY_NOT_READY);
}

TEST(SettingsResourceJobs, TClientSectionInvalidationAlsoInvalidatesPageCache)
{
	std::ifstream File("src/game/client/components/tclient/menus_tclient.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();
	EXPECT_NE(Source.find("InvalidateSettingsPageRuntimeCache(SETTINGS_TCLIENT"), std::string::npos);
}

TEST(SettingsResourceJobs, DrawSettingsPageRuntimeCacheUsesRecordedDependencyFlag)
{
	std::ifstream File("src/game/client/components/menus.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();
	EXPECT_NE(Source.find("pCache->m_State.m_DependentSubcachesReadyAtRecord"), std::string::npos);
	EXPECT_NE(Source.find("SettingsPageRecordedCacheMissReason("), std::string::npos);
}

TEST(SettingsResourceJobs, PrewarmSettingsPageRuntimeCacheLogsSuccessfulRecord)
{
	std::ifstream File("src/game/client/components/menus.cpp");
	ASSERT_TRUE(File.good());
	std::stringstream Buffer;
	Buffer << File.rdbuf();
	const std::string Source = Buffer.str();

	const size_t RecordPos = Source.find("pCache->m_State.m_DependentSubcachesReadyAtRecord = DependentSubcachesReady;");
	ASSERT_NE(RecordPos, std::string::npos);
	const size_t LogPos = Source.find("LogSettingsWarmupPerf(Page, Tab, \"miss\", \"n/a\", SettingsPageRecordedCacheMissReason(true, true, ResourcesReady, DependentSubcachesReady), PerfDebugElapsedMs(PerfStartTime));", RecordPos);
	ASSERT_NE(LogPos, std::string::npos);
}

TEST(SettingsResourceJobs, AssetsPageRejectsWholePageFbo)
{
	EXPECT_FALSE(SettingsPageCanUsePageFbo(CMenus::SETTINGS_ASSETS, CMenus::SETTINGS_ASSETS));
	EXPECT_FALSE(SettingsPageCanUsePageFbo(CMenus::SETTINGS_TEE, CMenus::SETTINGS_ASSETS));
	EXPECT_FALSE(SettingsPageCanUsePageFbo(CMenus::SETTINGS_PLAYER, CMenus::SETTINGS_ASSETS));
	EXPECT_FALSE(SettingsPageCanUsePageFbo(CMenus::SETTINGS_GRAPHICS, CMenus::SETTINGS_ASSETS));
	EXPECT_FALSE(SettingsPageCanUsePageFbo(CMenus::SETTINGS_QMCLIENT, CMenus::SETTINGS_ASSETS));
	EXPECT_TRUE(SettingsPageCanUsePageFbo(CMenus::SETTINGS_TCLIENT, CMenus::SETTINGS_ASSETS));
	EXPECT_TRUE(SettingsPageCanUsePageFbo(CMenus::SETTINGS_LANGUAGE, CMenus::SETTINGS_ASSETS));
}

TEST(SettingsWarmup, GraphicsPageRuntimeWarmupDoesNotRenderSystemControls)
{
	std::ifstream MenusSourceFile("src/game/client/components/menus.cpp");
	ASSERT_TRUE(MenusSourceFile.good());
	std::stringstream MenusBuffer;
	MenusBuffer << MenusSourceFile.rdbuf();
	const std::string MenusSource = MenusBuffer.str();

	const size_t WarmupPos = MenusSource.find("bool CMenus::PrewarmSettingsPageRuntimeCache(CUIRect ContentView, int Page, int Tab, float ScrollY, bool ResourcesReady)");
	ASSERT_NE(WarmupPos, std::string::npos);
	const size_t DrawPos = MenusSource.find("bool CMenus::DrawSettingsPageRuntimeCache", WarmupPos);
	ASSERT_NE(DrawPos, std::string::npos);
	const std::string WarmupBody = MenusSource.substr(WarmupPos, DrawPos - WarmupPos);

	EXPECT_NE(WarmupBody.find("if(!SettingsPageCanUsePageFbo(Page, SETTINGS_ASSETS))"), std::string::npos);
	EXPECT_NE(WarmupBody.find("RenderSettingsGraphics(CacheView);"), std::string::npos);
	EXPECT_FALSE(SettingsPageCanUsePageFbo(CMenus::SETTINGS_GRAPHICS, CMenus::SETTINGS_ASSETS));
}

TEST(SettingsResourceJobs, AssetWarmupTracksAllTabsAndCycles)
{
	bool aReadyTabs[] = {true, false, true};
	EXPECT_FALSE(SettingsAssetWarmupAllTabsReady(aReadyTabs, 3));
	aReadyTabs[1] = true;
	EXPECT_TRUE(SettingsAssetWarmupAllTabsReady(aReadyTabs, 3));
	EXPECT_TRUE(SettingsAssetWarmupAllTabsReady(nullptr, 0));

	EXPECT_EQ(SettingsAssetWarmupNextTab(-1, 3), 0);
	EXPECT_EQ(SettingsAssetWarmupNextTab(0, 3), 1);
	EXPECT_EQ(SettingsAssetWarmupNextTab(2, 3), 0);
	EXPECT_EQ(SettingsAssetWarmupNextTab(0, 0), -1);
}

TEST(SettingsResourceJobs, SkinSnapshotRejectsStaleGeneration)
{
	SSettingsSkinListPlanResult Result;
	Result.m_Generation = 7;
	EXPECT_TRUE(SettingsSkinListPlanGenerationMatches(Result, 7));
	EXPECT_FALSE(SettingsSkinListPlanGenerationMatches(Result, 8));
}

TEST(SettingsResourceJobs, AssetListRejectsStaleJobGeneration)
{
	EXPECT_TRUE(SettingsAssetListJobGenerationMatches(4, 4));
	EXPECT_FALSE(SettingsAssetListJobGenerationMatches(4, 5));
}

TEST(SettingsResourceJobs, SkinListPublishesOnlyCompleteMergedList)
{
	EXPECT_FALSE(SettingsSkinListShouldPublishMergedList(0, 3));
	EXPECT_FALSE(SettingsSkinListShouldPublishMergedList(2, 3));
	EXPECT_TRUE(SettingsSkinListShouldPublishMergedList(3, 3));
	EXPECT_TRUE(SettingsSkinListShouldPublishMergedList(0, 0));
}

TEST(SettingsResourceJobs, SkinListReplacesPublishedEntriesAfterStableDirectory)
{
	EXPECT_TRUE(SettingsSkinListShouldReplacePublishedEntries(0, 3, true, true));
	EXPECT_FALSE(SettingsSkinListShouldReplacePublishedEntries(0, 3, false, false));
	EXPECT_TRUE(SettingsSkinListShouldReplacePublishedEntries(0, 3, false, true));
	EXPECT_FALSE(SettingsSkinListShouldReplacePublishedEntries(10, 20, false, false));
	EXPECT_FALSE(SettingsSkinListShouldReplacePublishedEntries(20, 10, false, false));
}

TEST(SettingsResourceJobs, SkinListKeepsPublishedEntriesUntilDirectoryScanSettles)
{
	EXPECT_FALSE(SettingsSkinListShouldReplacePublishedEntries(0, 1, true, false));
	EXPECT_FALSE(SettingsSkinListShouldReplacePublishedEntries(0, 3, true, false));
	EXPECT_FALSE(SettingsSkinListShouldReplacePublishedEntries(3, 1, true, false));
}

TEST(SettingsResourceJobs, SkinListSkeletonReadyDoesNotRequirePreviewResources)
{
	SSkinListPlanState State{};
	State.m_DirectoryScanPending = false;
	State.m_MergeComplete = true;
	State.m_ItemCount = 120;
	EXPECT_TRUE(SettingsSkinListSkeletonReady(State));
	EXPECT_FALSE(SettingsSkinListResourcesSettled(State));
}

TEST(SettingsResourceJobs, SkinListReadyMigrationKeepsStructureStable)
{
	SSkinListPlanSnapshot Snapshot{};
	Snapshot.m_ItemCount = 120;

	SSkinListPlanState Loading{};
	Loading.m_DirectoryScanPending = false;
	Loading.m_MergeComplete = true;
	Loading.m_ItemCount = 120;
	Loading.m_BackgroundBacklog = 5;
	Loading.m_VisibleBacklog = 1;

	SSkinListPlanState Settled = Loading;
	Settled.m_BackgroundBacklog = 0;
	Settled.m_VisibleBacklog = 0;

	EXPECT_FALSE(SettingsSkinListResourcesSettled(Loading));
	EXPECT_TRUE(SettingsSkinListResourcesSettled(Settled));
	EXPECT_EQ(Snapshot.m_ItemCount, 120);
}

TEST(SettingsResourceJobs, SourceAdmissionAllowsVisiblePromotionWhenDrainInactive)
{
	const auto Decision = SettingsSkinSourceAdmissionDecision({
		true,
		ESettingsResourcePriority::VISIBLE,
		false,
		24,
		192,
		256,
	});

	EXPECT_TRUE(Decision.m_PromoteAllowed);
	EXPECT_EQ(Decision.m_PromotePriority, ESettingsResourcePriority::VISIBLE);
	EXPECT_EQ(Decision.m_BlockReason, ESettingsSkinSourceAdmissionBlockReason::NONE);
	EXPECT_FALSE(Decision.m_CountFuseApplies);
}

TEST(SettingsResourceJobs, SourceAdmissionBlocksBackgroundPromotionWhenDrainInactive)
{
	const auto Decision = SettingsSkinSourceAdmissionDecision({
		true,
		ESettingsResourcePriority::BACKGROUND,
		false,
		24,
		192,
		256,
	});

	EXPECT_FALSE(Decision.m_PromoteAllowed);
	EXPECT_EQ(Decision.m_BlockReason, ESettingsSkinSourceAdmissionBlockReason::DRAIN_INACTIVE);
	EXPECT_TRUE(Decision.m_CountFuseApplies);
}

TEST(SettingsResourceJobs, SourceAdmissionUsesVisibleReserveForVisibleRequests)
{
	const auto Decision = SettingsSkinSourceAdmissionDecision({
		true,
		ESettingsResourcePriority::VISIBLE,
		true,
		256,
		192,
		256,
	});

	EXPECT_FALSE(Decision.m_PromoteAllowed);
	EXPECT_EQ(Decision.m_BlockReason, ESettingsSkinSourceAdmissionBlockReason::VISIBLE_RESERVE);
	EXPECT_FALSE(Decision.m_CountFuseApplies);
}

TEST(SettingsResourceJobs, AllVisibleReadyLoggingRequiresVisibleRows)
{
	EXPECT_FALSE(SettingsSkinListShouldLogAllVisibleReady(true, false, 0));
	EXPECT_FALSE(SettingsSkinListShouldLogAllVisibleReady(false, false, 28));
	EXPECT_FALSE(SettingsSkinListShouldLogAllVisibleReady(true, true, 28));
	EXPECT_TRUE(SettingsSkinListShouldLogAllVisibleReady(true, false, 28));
}

TEST(SettingsResourceJobs, SkeletonReadyPublishedCountMatchesSnapshotCount)
{
	SSkinListPlanSnapshot Snapshot{};
	Snapshot.m_ItemCount = 120;
	std::vector<int> vPublished(120, 0);
	EXPECT_EQ(static_cast<int>(vPublished.size()), Snapshot.m_ItemCount);
}

TEST(SettingsResourceJobs, SkinListKeepsPendingPlanAliveUntilPublishGateOpens)
{
	EXPECT_TRUE(SettingsSkinListHasPendingMergeWork(true, 3, 3, 3));
	EXPECT_TRUE(SettingsSkinListHasPendingMergeWork(true, 0, 0, 0));
	EXPECT_FALSE(SettingsSkinListHasPendingMergeWork(false, 3, 3, 3));
}

TEST(SettingsResourceJobs, VisibleSkinListEntriesRequestImmediateLoad)
{
	EXPECT_TRUE(SettingsSkinListShouldRequestImmediateLoad(true));
	EXPECT_FALSE(SettingsSkinListShouldRequestImmediateLoad(false));
}

TEST(SettingsResourceJobs, RuntimeWarmupOnlyRunsOnSettingsPageWhenIdle)
{
	EXPECT_TRUE(SettingsRuntimeWarmupShouldRun(true, true, false, false, false, false, false));
	EXPECT_FALSE(SettingsRuntimeWarmupShouldRun(true, false, false, false, false, false, false));
	EXPECT_FALSE(SettingsRuntimeWarmupShouldRun(true, true, true, false, false, false, false));
	EXPECT_FALSE(SettingsRuntimeWarmupShouldRun(true, true, false, true, false, false, false));
	EXPECT_FALSE(SettingsRuntimeWarmupShouldRun(true, true, false, false, true, false, false));
	EXPECT_FALSE(SettingsRuntimeWarmupShouldRun(true, true, false, false, false, true, false));
	EXPECT_FALSE(SettingsRuntimeWarmupShouldRun(true, true, false, false, false, false, true));
	EXPECT_FALSE(SettingsRuntimeWarmupShouldRun(false, true, false, false, false, false, false));
}

TEST(SettingsResourceJobs, SkinListWarmupCountsCoverVisibleAndPrefetchRows)
{
	EXPECT_EQ(SettingsSkinListFirstPageWarmupEntries(180.0f, 50.0f, 1, 2), 6);
	EXPECT_EQ(SettingsSkinListFirstPageWarmupEntries(300.0f, 50.0f, 4, 1), 28);
	EXPECT_EQ(SettingsSkinListFirstPageWarmupEntries(0.0f, 50.0f, 1, 2), 0);
	EXPECT_EQ(SettingsTeeSkinListFirstPageWarmupEntries(300.0f), 28);
	EXPECT_EQ(SettingsTeeSkinListFirstPageWarmupEntries(120.0f), 24);
	EXPECT_EQ(SettingsSkinListPrefetchCount(0, 2, 1, 2, 10), 2);
	EXPECT_EQ(SettingsSkinListPrefetchCount(7, 9, 1, 2, 10), 0);
	EXPECT_EQ(SettingsSkinListBackgroundWarmupCount(20, 6), 6);
	EXPECT_EQ(SettingsSkinListBackgroundWarmupCount(3, 6), 3);
}

TEST(SettingsResourceJobs, LoadingPrewarmAttemptBudgetReservesExtraTeeSourceSettlePasses)
{
	EXPECT_EQ(SettingsLoadingPrewarmMaxAttempts(0, 0), 33);
	EXPECT_EQ(SettingsLoadingPrewarmMaxAttempts(19, 28), 132);
	EXPECT_EQ(SettingsLoadingPrewarmMaxAttempts(19, 8), 108);
}

TEST(SettingsResourceJobs, LoadingPrewarmBudgetOnlyStopsAfterBudgetAndStall)
{
	EXPECT_TRUE(SettingsLoadingPrewarmShouldKeepPumping(false, 0, 108, 0));
	EXPECT_TRUE(SettingsLoadingPrewarmShouldKeepPumping(false, 108, 108, 0));
	EXPECT_TRUE(SettingsLoadingPrewarmShouldKeepPumping(false, 108, 108, 7));
	EXPECT_FALSE(SettingsLoadingPrewarmShouldKeepPumping(false, 108, 108, 8));
	EXPECT_FALSE(SettingsLoadingPrewarmShouldKeepPumping(true, 20, 108, 0));
}

TEST(SettingsResourceJobs, SkinListBackgroundWarmupWaitsForIdleVisibleBacklog)
{
	EXPECT_TRUE(SettingsSkinBackgroundWarmupShouldRun(true, false, false));
	EXPECT_FALSE(SettingsSkinBackgroundWarmupShouldRun(true, true, false));
	EXPECT_FALSE(SettingsSkinBackgroundWarmupShouldRun(true, false, true));
	EXPECT_FALSE(SettingsSkinBackgroundWarmupShouldRun(false, false, false));
	EXPECT_FALSE(SettingsSkinBackgroundWarmupWindowFull(0, 20, 10, 64));
	EXPECT_TRUE(SettingsSkinBackgroundWarmupWindowFull(0, 40, 24, 64));
}

TEST(SettingsResourceJobs, TeeSkinSourceLoadWindowCapsActiveDecodeConcurrency)
{
	const SSettingsResourceFrameContext Idle = SettingsBuildFrameContext(false, false, 0);
	const SSettingsResourceFrameContext Recovery = SettingsBuildFrameContext(false, false, 2);
	const SSettingsResourceFrameContext Scroll = SettingsBuildFrameContext(true, false, 0);
	EXPECT_EQ(SettingsSkinSourceLoadNormalWindow(Idle, true, 1600), 256);
	EXPECT_EQ(SettingsSkinSourceLoadVisibleWindow(Idle, true, 1600), 256);
	EXPECT_EQ(SettingsSkinSourceLoadNormalWindow(Recovery, true, 1600), 128);
	EXPECT_EQ(SettingsSkinSourceLoadVisibleWindow(Recovery, true, 1600), 192);
	EXPECT_EQ(SettingsSkinSourceLoadNormalWindow(Scroll, true, 1600), 48);
	EXPECT_EQ(SettingsSkinSourceLoadVisibleWindow(Scroll, true, 1600), 128);
	EXPECT_EQ(SettingsSkinSourceLoadVisibleWindow(Idle, true, 32), 32);
}

TEST(SettingsResourceJobs, VisibleSkinFinalizeDefersBackgroundSweepsAfterPriorityWork)
{
	EXPECT_TRUE(SettingsSkinFinalizeShouldDeferBackgroundSweep(true, 1, 2));
	EXPECT_FALSE(SettingsSkinFinalizeShouldDeferBackgroundSweep(false, 1, 2));
	EXPECT_FALSE(SettingsSkinFinalizeShouldDeferBackgroundSweep(true, 0, 2));
	EXPECT_FALSE(SettingsSkinFinalizeShouldDeferBackgroundSweep(true, 2, 2));
}

TEST(SettingsResourceJobs, VisibleSkinFinalizeAllowsBackgroundSweepOnNextFrame)
{
	EXPECT_TRUE(SettingsSkinFinalizeShouldDeferBackgroundSweep(true, 1, 12));
	EXPECT_FALSE(SettingsSkinFinalizeShouldDeferBackgroundSweep(false, 0, 12));
}

TEST(SettingsResourceJobs, TeeSkinFinalizeBudgetDefersDuringScrollAndRecovery)
{
	const SSettingsResourceFrameContext Idle = {false, false, 0};
	const SSettingsResourceFrameContext Scrolling = {true, false, 0};
	const SSettingsResourceFrameContext Recovering = {false, false, 2};

	EXPECT_EQ(SettingsSkinFinalizeMaxPerFrame(true), 64);
	EXPECT_EQ(SettingsSkinGpuUploadUnits(true), 8);
	EXPECT_EQ(SettingsSkinFinalizeFrameBudget(Idle, true), SettingsSkinFinalizeMaxPerFrame(true));
	EXPECT_EQ(SettingsSkinGpuUploadFrameUnits(Idle, true), SettingsSkinGpuUploadUnits(true));
	EXPECT_EQ(SettingsSkinFinalizeFrameBudget(Scrolling, true), 16);
	EXPECT_EQ(SettingsSkinGpuUploadFrameUnits(Scrolling, true), 4);
	EXPECT_EQ(SettingsSkinFinalizeFrameBudget(Recovering, true), 48);
	EXPECT_EQ(SettingsSkinGpuUploadFrameUnits(Recovering, true), 8);
}

TEST(SettingsResourceJobs, ActiveTeeSkinFrameBudgetAllowsEightSourceUploadsPerFrame)
{
	SSettingsWarmupFrameBudget Budget;
	SettingsApplyActiveTeeSkinFrameBudget(Budget, true);

	SSettingsResourceMergeBudget UploadBudget;
	UploadBudget.m_MaxGpuUploads = 8;
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_TRUE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_FALSE(SettingsResourceConsumeGpuUpload(UploadBudget, &Budget));
	EXPECT_EQ(UploadBudget.m_StopReason, ESettingsWarmupStopReason::GPU_UPLOAD_BUDGET);
}

TEST(SettingsResourceJobs, TeeSkinGpuUploadLimiterBudgetTracksFrameContext)
{
	const SSettingsResourceFrameContext IdleVisible = SettingsBuildFrameContext(false, false, 0);
	const SSettingsResourceFrameContext Recovery = SettingsBuildFrameContext(false, false, 2);
	const SSettingsResourceFrameContext Scroll = SettingsBuildFrameContext(true, false, 0);
	SSettingsResourceFrameContext IdleDrain = SettingsBuildFrameContext(false, false, 0);
	IdleDrain.m_HighPrioritySettled = true;

	EXPECT_EQ(SettingsSkinGpuUploadLimiterUnits(IdleVisible, true), 192);
	EXPECT_EQ(SettingsSkinGpuUploadLimiterUnits(Recovery, true), 192);
	EXPECT_EQ(SettingsSkinGpuUploadLimiterUnits(Scroll, true), 96);
	EXPECT_EQ(SettingsSkinGpuUploadLimiterUnits(IdleDrain, true), 288);
}

TEST(SettingsResourceJobs, ThroughputControllerKeepsVisibleBacklogOutOfIdleDrain)
{
	SSettingsSkinThroughputControllerState State;
	const auto VisibleBacklog = SettingsSkinThroughputControllerStep({
										 {false, false, 0, true},
										 true,
										 6.5f,
										 6.0f,
										 512,
										 28,
										 19,
										 9,
										 0,
										 9,
										 0,
										 3,
										 21,
										 29,
										 24,
										 0,
										 0,
										 0,
										 0,
										 192,
										 false,
										 "none",
										 "drain_inactive",
									 },
		State);
	EXPECT_EQ(VisibleBacklog.m_Mode, ESettingsSkinThroughputControllerMode::IDLE_VISIBLE);
	EXPECT_FALSE(VisibleBacklog.m_BackgroundDrainActive);
	EXPECT_EQ(VisibleBacklog.m_BackgroundRequestBudget, 6);

	const auto Settled = SettingsSkinThroughputControllerStep({
									  {false, false, 0, true},
									  true,
									  6.5f,
									  6.0f,
									  512,
									  28,
									  28,
									  0,
									  0,
									  0,
									  0,
									  0,
									  20,
									  40,
									  20,
									  12,
									  12,
									  0,
									  0,
									  288,
									  false,
									  "none",
									  "none",
								  },
		State);
	EXPECT_EQ(Settled.m_Mode, ESettingsSkinThroughputControllerMode::IDLE_DRAIN);
	EXPECT_TRUE(Settled.m_BackgroundDrainActive);
	EXPECT_EQ(Settled.m_BackgroundRequestBudget, 24);
}

TEST(SettingsResourceJobs, ThroughputControllerRelaxesReserveAndExpandsWindowsWhenAdmissionUnderfed)
{
	SSettingsSkinThroughputControllerState State;
	State.m_Initialized = true;
	State.m_Mode = ESettingsSkinThroughputControllerMode::IDLE_VISIBLE;
	State.m_GpuUploadLimitUnits = 192;
	State.m_GpuUploadFrameBudget = 8;
	State.m_FinalizeBudgetLimit = 48;
	State.m_NormalLoadingWindow = 128;
	State.m_VisibleLoadingWindow = 192;
	State.m_VisibleReserve = 2;

	const auto Output = SettingsSkinThroughputControllerStep({
									 {false, 0, false},
									 true,
									 6.9f,
									 6.7f,
									 512,
									 28,
									 19,
									 9,
									 0,
									 9,
									 0,
									 3,
									 21,
									 29,
									 30,
									 0,
									 0,
									 0,
									 0,
									 192,
									 false,
									 "visible_reserve",
									 "drain_inactive",
								 },
		State);

	EXPECT_EQ(Output.m_Reason, ESettingsSkinThroughputControllerReason::ADMISSION);
	EXPECT_EQ(Output.m_VisibleReserve, 0);
	EXPECT_GT(Output.m_NormalLoadingWindow, 128);
	EXPECT_GT(Output.m_VisibleLoadingWindow, 192);
	EXPECT_TRUE(Output.m_AdmissionUnderfed);
	EXPECT_EQ(Output.m_UnderfedStreak, 1);
}

TEST(SettingsResourceJobs, ThroughputControllerReducesOnlyUploadBudgetOnGpuPressure)
{
	SSettingsSkinThroughputControllerState State;
	State.m_Initialized = true;
	State.m_Mode = ESettingsSkinThroughputControllerMode::IDLE_VISIBLE;
	State.m_GpuUploadLimitUnits = 240;
	State.m_GpuUploadFrameBudget = 10;
	State.m_FinalizeBudgetLimit = 64;
	State.m_NormalLoadingWindow = 192;
	State.m_VisibleLoadingWindow = 224;
	State.m_VisibleReserve = 2;

	const auto Output = SettingsSkinThroughputControllerStep({
									 {false, 0, false},
									 true,
									 7.0f,
									 6.8f,
									 512,
									 28,
									 20,
									 8,
									 0,
									 8,
									 0,
									 2,
									 18,
									 30,
									 20,
									 0,
									 0,
									 0,
									 0,
									 0,
									 false,
									 "gpu_upload_budget",
									 "none",
								 },
		State);

	EXPECT_EQ(Output.m_Reason, ESettingsSkinThroughputControllerReason::GPU);
	EXPECT_LT(Output.m_GpuUploadLimitUnits, 240);
	EXPECT_EQ(Output.m_FinalizeBudgetLimit, 64);
	EXPECT_EQ(Output.m_NormalLoadingWindow, 192);
}

TEST(SettingsResourceJobs, ThroughputControllerReducesOnlyFinalizeBudgetOnFinalizePressure)
{
	SSettingsSkinThroughputControllerState State;
	State.m_Initialized = true;
	State.m_Mode = ESettingsSkinThroughputControllerMode::IDLE_VISIBLE;
	State.m_GpuUploadLimitUnits = 240;
	State.m_GpuUploadFrameBudget = 10;
	State.m_FinalizeBudgetLimit = 64;
	State.m_NormalLoadingWindow = 192;
	State.m_VisibleLoadingWindow = 224;
	State.m_VisibleReserve = 2;

	const auto Output = SettingsSkinThroughputControllerStep({
									 {false, 0, false},
									 true,
									 7.0f,
									 6.8f,
									 512,
									 28,
									 20,
									 8,
									 0,
									 8,
									 0,
									 2,
									 18,
									 30,
									 20,
									 0,
									 0,
									 0,
									 0,
									 96,
									 false,
									 "max_per_frame",
									 "none",
								 },
		State);

	EXPECT_EQ(Output.m_Reason, ESettingsSkinThroughputControllerReason::FINALIZE);
	EXPECT_EQ(Output.m_GpuUploadLimitUnits, 240);
	EXPECT_LT(Output.m_FinalizeBudgetLimit, 64);
	EXPECT_EQ(Output.m_NormalLoadingWindow, 192);
}

TEST(SettingsResourceJobs, NonTeeSkinFinalizeBudgetKeepsLegacyLimits)
{
	const SSettingsResourceFrameContext Scrolling = {true, false, 2};
	EXPECT_EQ(SettingsSkinFinalizeFrameBudget(Scrolling, false), SettingsSkinFinalizeMaxPerFrame(false));
	EXPECT_EQ(SettingsSkinGpuUploadFrameUnits(Scrolling, false), SettingsSkinGpuUploadUnits(false));
}

TEST(SettingsResourceJobs, ImmediateScrollInputKeepsReducedTeeThroughputBeforePersistentStateCatchesUp)
{
	const SSettingsResourceFrameContext Idle = SettingsBuildFrameContext(false, false, 0);
	const SSettingsResourceFrameContext ImmediateScroll = SettingsBuildFrameContext(false, true, 0);
	const SSettingsResourceFrameContext PersistentScroll = SettingsBuildFrameContext(true, false, 0);

	EXPECT_FALSE(Idle.m_ScrollActive);
	EXPECT_TRUE(ImmediateScroll.m_ScrollActive);
	EXPECT_TRUE(PersistentScroll.m_ScrollActive);
	EXPECT_GT(SettingsSkinFinalizeFrameBudget(ImmediateScroll, true), 0);
	EXPECT_GT(SettingsSkinGpuUploadFrameUnits(ImmediateScroll, true), 0);
	EXPECT_LT(SettingsSkinFinalizeFrameBudget(ImmediateScroll, true), SettingsSkinFinalizeFrameBudget(Idle, true));
	EXPECT_LT(SettingsSkinGpuUploadFrameUnits(ImmediateScroll, true), SettingsSkinGpuUploadFrameUnits(Idle, true));
	EXPECT_EQ(SettingsSkinFinalizeFrameBudget(ImmediateScroll, true), SettingsSkinFinalizeFrameBudget(PersistentScroll, true));
	EXPECT_EQ(SettingsSkinGpuUploadFrameUnits(ImmediateScroll, true), SettingsSkinGpuUploadFrameUnits(PersistentScroll, true));
}

TEST(SettingsResourceJobs, JumpScrollUsesSameHeavyBudgetGateAsImmediateScroll)
{
	const SSettingsResourceFrameContext Idle = SettingsBuildFrameContext(false, false, false, 0);
	const SSettingsResourceFrameContext JumpScroll = SettingsBuildFrameContext(false, false, true, 0);
	const SSettingsResourceFrameContext ImmediateScroll = SettingsBuildFrameContext(false, true, false, 0);

	EXPECT_FALSE(Idle.m_ScrollActive);
	EXPECT_FALSE(Idle.m_JumpScrollActive);
	EXPECT_TRUE(JumpScroll.m_JumpScrollActive);
	EXPECT_FALSE(JumpScroll.m_ScrollActive);
	EXPECT_TRUE(ImmediateScroll.m_ScrollActive);
	EXPECT_FALSE(ImmediateScroll.m_JumpScrollActive);
	EXPECT_EQ(SettingsResourceSharedHeavyBudget(JumpScroll, 4, 1), 0);
	EXPECT_EQ(SettingsResourceFrameStageBudget(JumpScroll, ESettingsResourcePriority::BACKGROUND, 4, 1), 0);
	EXPECT_EQ(SettingsResourceFrameStageBudget(JumpScroll, ESettingsResourcePriority::VISIBLE, 4, 1), 1);
	EXPECT_FALSE(SettingsResourceOversizedUploadAllowed(JumpScroll, false, ESettingsResourcePriority::VISIBLE, 0, 2 * 1024 * 1024, 1 * 1024 * 1024));
}

TEST(SettingsResourceJobs, TeeSkinBackgroundRequestBudgetOnlyRunsOnIdleFrames)
{
	const SSettingsResourceFrameContext Idle = {false, false, 0};
	const SSettingsResourceFrameContext Scrolling = {true, false, 0};
	const SSettingsResourceFrameContext Recovering = {false, false, 2};

	EXPECT_GT(SettingsSkinBackgroundRequestFrameBudget(Idle, true), 0);
	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(Scrolling, true), 0);
	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(Recovering, true), 0);
	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(Idle, false), 0);
}

TEST(SettingsResourceJobs, TeeSkinBackgroundDrainRaisesIdleThroughputBudgets)
{
	SSettingsResourceFrameContext IdleSettled = {false, false, 0};
	IdleSettled.m_HighPrioritySettled = true;
	SSettingsResourceFrameContext RecoveringSettled = {false, false, 2};
	RecoveringSettled.m_HighPrioritySettled = true;
	SSettingsResourceFrameContext ScrollingSettled = {true, false, 0};
	ScrollingSettled.m_HighPrioritySettled = true;

	EXPECT_TRUE(SettingsSkinBackgroundDrainActive(IdleSettled, true));
	EXPECT_FALSE(SettingsSkinBackgroundDrainActive(RecoveringSettled, true));
	EXPECT_FALSE(SettingsSkinBackgroundDrainActive(ScrollingSettled, true));
	EXPECT_FALSE(SettingsSkinBackgroundDrainActive(IdleSettled, false));

	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(IdleSettled, true), 24);
	EXPECT_EQ(SettingsSkinSourceLoadNormalWindow(IdleSettled, true, 64), 256);
	EXPECT_EQ(SettingsSkinSourceLoadVisibleWindow(IdleSettled, true, 64), 256);
	EXPECT_EQ(SettingsSkinSourceCountFuseLimit(IdleSettled, true, 64), 128);
}

TEST(SettingsResourceJobs, TeeBackgroundRequestsStayBlockedThroughScrollCooldownAndRecovery)
{
	int CooldownFrames = 0;
	int RecoveryFrames = 0;

	CooldownFrames = SettingsScrollInteractionCooldown(true, CooldownFrames, 3);
	RecoveryFrames = SettingsScrollInteractionRecovery(true, 0, CooldownFrames, RecoveryFrames, 2);
	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(SettingsBuildFrameContext(true || CooldownFrames > 0, false, RecoveryFrames), true), 0);

	int PreviousCooldownFrames = CooldownFrames;
	CooldownFrames = SettingsScrollInteractionCooldown(false, CooldownFrames, 3);
	RecoveryFrames = SettingsScrollInteractionRecovery(false, PreviousCooldownFrames, CooldownFrames, RecoveryFrames, 2);
	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(SettingsBuildFrameContext(false || CooldownFrames > 0, false, RecoveryFrames), true), 0);

	PreviousCooldownFrames = CooldownFrames;
	CooldownFrames = SettingsScrollInteractionCooldown(false, CooldownFrames, 3);
	RecoveryFrames = SettingsScrollInteractionRecovery(false, PreviousCooldownFrames, CooldownFrames, RecoveryFrames, 2);
	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(SettingsBuildFrameContext(false || CooldownFrames > 0, false, RecoveryFrames), true), 0);

	PreviousCooldownFrames = CooldownFrames;
	CooldownFrames = SettingsScrollInteractionCooldown(false, CooldownFrames, 3);
	RecoveryFrames = SettingsScrollInteractionRecovery(false, PreviousCooldownFrames, CooldownFrames, RecoveryFrames, 2);
	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(SettingsBuildFrameContext(false || CooldownFrames > 0, false, RecoveryFrames), true), 0);

	PreviousCooldownFrames = CooldownFrames;
	CooldownFrames = SettingsScrollInteractionCooldown(false, CooldownFrames, 3);
	RecoveryFrames = SettingsScrollInteractionRecovery(false, PreviousCooldownFrames, CooldownFrames, RecoveryFrames, 2);
	EXPECT_EQ(SettingsSkinBackgroundRequestFrameBudget(SettingsBuildFrameContext(false || CooldownFrames > 0, false, RecoveryFrames), true), 0);

	PreviousCooldownFrames = CooldownFrames;
	CooldownFrames = SettingsScrollInteractionCooldown(false, CooldownFrames, 3);
	RecoveryFrames = SettingsScrollInteractionRecovery(false, PreviousCooldownFrames, CooldownFrames, RecoveryFrames, 2);
	EXPECT_GT(SettingsSkinBackgroundRequestFrameBudget(SettingsBuildFrameContext(false || CooldownFrames > 0, false, RecoveryFrames), true), 0);
}

TEST(SettingsResourceJobs, TeeBackgroundRequestBudgetTracksRealInflightHeadroom)
{
	SSettingsSkinBackgroundRequestBudgetInput Input;
	Input.m_DefaultBudget = 24;
	Input.m_Pending = 40;
	Input.m_Loading = 60;
	Input.m_BackgroundRequested = 0;
	Input.m_CountFuseLimit = 128;
	Input.m_VisibleReserve = 8;
	Input.m_RecentLoadedDelta = 2;
	Input.m_RecentAdmittedDelta = 2;
	Input.m_DrainActive = true;

	const auto Decision = SettingsSkinBackgroundRequestBudgetDecision(Input);
	EXPECT_EQ(Decision.m_RealInflight, 100);
	EXPECT_EQ(Decision.m_RequestBudget, 20);
	EXPECT_EQ(Decision.m_BlockReason, ESettingsSkinBackgroundRequestBlockReason::NONE);
}

TEST(SettingsResourceJobs, TeeBackgroundRequestBudgetPausesWhenHeadroomIsReservedForVisible)
{
	SSettingsSkinBackgroundRequestBudgetInput Input;
	Input.m_DefaultBudget = 24;
	Input.m_Pending = 64;
	Input.m_Loading = 56;
	Input.m_BackgroundRequested = 0;
	Input.m_CountFuseLimit = 128;
	Input.m_VisibleReserve = 8;
	Input.m_RecentLoadedDelta = 1;
	Input.m_RecentAdmittedDelta = 1;
	Input.m_DrainActive = true;

	const auto Decision = SettingsSkinBackgroundRequestBudgetDecision(Input);
	EXPECT_EQ(Decision.m_RealInflight, 120);
	EXPECT_EQ(Decision.m_RequestBudget, 0);
	EXPECT_EQ(Decision.m_BlockReason, ESettingsSkinBackgroundRequestBlockReason::VISIBLE_RESERVE);
}

TEST(SettingsResourceJobs, TeeBackgroundRequestBudgetSlowsStalledProducerWithLargeBacklog)
{
	SSettingsSkinBackgroundRequestBudgetInput Input;
	Input.m_DefaultBudget = 24;
	Input.m_Pending = 4;
	Input.m_Loading = 4;
	Input.m_BackgroundRequested = 300;
	Input.m_CountFuseLimit = 128;
	Input.m_VisibleReserve = 8;
	Input.m_RecentLoadedDelta = 0;
	Input.m_RecentAdmittedDelta = 3;
	Input.m_DrainActive = true;

	const auto Decision = SettingsSkinBackgroundRequestBudgetDecision(Input);
	EXPECT_EQ(Decision.m_RequestBudget, 0);
	EXPECT_EQ(Decision.m_BlockReason, ESettingsSkinBackgroundRequestBlockReason::STALL_BACKPRESSURE);
}

TEST(SettingsResourceJobs, TeeBackgroundWindowGrowsSlowlyWhenDrainIsHealthy)
{
	SSettingsSkinBackgroundWindowInput Input;
	Input.m_CurrentLimit = 64;
	Input.m_MinLimit = 32;
	Input.m_MaxLimit = 120;
	Input.m_HealthyFrames = 3;
	Input.m_HealthyFramesToGrow = 4;
	Input.m_DrainActive = true;
	Input.m_FrameStable = true;
	Input.m_VisibleWaiting = false;
	Input.m_GpuBudgetExhausted = false;
	Input.m_FinalizeBudgetExhausted = false;
	Input.m_DecodeJobsSaturated = false;
	Input.m_LoadedProgress = true;
	Input.m_ConsumerStalled = false;

	const auto Update = SettingsSkinBackgroundWindowUpdate(Input);
	EXPECT_EQ(Update.m_NextLimit, 65);
	EXPECT_EQ(Update.m_NextHealthyFrames, 0);
	EXPECT_EQ(Update.m_Decision, ESettingsSkinBackgroundWindowDecision::INCREASE);
}

TEST(SettingsResourceJobs, TeeBackgroundWindowShrinksFastWhenVisibleNeedsHeadroom)
{
	SSettingsSkinBackgroundWindowInput Input;
	Input.m_CurrentLimit = 96;
	Input.m_MinLimit = 32;
	Input.m_MaxLimit = 120;
	Input.m_HealthyFrames = 2;
	Input.m_HealthyFramesToGrow = 4;
	Input.m_DrainActive = true;
	Input.m_FrameStable = true;
	Input.m_VisibleWaiting = true;
	Input.m_GpuBudgetExhausted = false;
	Input.m_FinalizeBudgetExhausted = false;
	Input.m_DecodeJobsSaturated = false;
	Input.m_LoadedProgress = false;
	Input.m_ConsumerStalled = false;

	const auto Update = SettingsSkinBackgroundWindowUpdate(Input);
	EXPECT_EQ(Update.m_NextLimit, 48);
	EXPECT_EQ(Update.m_NextHealthyFrames, 0);
	EXPECT_EQ(Update.m_Decision, ESettingsSkinBackgroundWindowDecision::DECREASE);
}

TEST(SettingsResourceJobs, TeeBackgroundWindowShrinksWhenAdmittedWorkStopsCompleting)
{
	SSettingsSkinBackgroundWindowInput Input;
	Input.m_CurrentLimit = 80;
	Input.m_MinLimit = 32;
	Input.m_MaxLimit = 120;
	Input.m_HealthyFrames = 1;
	Input.m_HealthyFramesToGrow = 4;
	Input.m_DrainActive = true;
	Input.m_FrameStable = true;
	Input.m_VisibleWaiting = false;
	Input.m_GpuBudgetExhausted = false;
	Input.m_FinalizeBudgetExhausted = false;
	Input.m_DecodeJobsSaturated = false;
	Input.m_LoadedProgress = false;
	Input.m_ConsumerStalled = true;

	const auto Update = SettingsSkinBackgroundWindowUpdate(Input);
	EXPECT_EQ(Update.m_NextLimit, 40);
	EXPECT_EQ(Update.m_Decision, ESettingsSkinBackgroundWindowDecision::DECREASE);
}

TEST(SettingsResourceJobs, TeeBackgroundWindowShrinksWhenDecodeJobsSaturate)
{
	SSettingsSkinBackgroundWindowInput Input;
	Input.m_CurrentLimit = 72;
	Input.m_MinLimit = 32;
	Input.m_MaxLimit = 120;
	Input.m_HealthyFrames = 2;
	Input.m_HealthyFramesToGrow = 4;
	Input.m_DrainActive = true;
	Input.m_FrameStable = true;
	Input.m_VisibleWaiting = false;
	Input.m_GpuBudgetExhausted = false;
	Input.m_FinalizeBudgetExhausted = false;
	Input.m_DecodeJobsSaturated = true;
	Input.m_LoadedProgress = false;
	Input.m_ConsumerStalled = false;

	const auto Update = SettingsSkinBackgroundWindowUpdate(Input);
	EXPECT_EQ(Update.m_NextLimit, 36);
	EXPECT_EQ(Update.m_NextHealthyFrames, 0);
	EXPECT_EQ(Update.m_Decision, ESettingsSkinBackgroundWindowDecision::DECREASE);
}

TEST(SettingsResourceJobs, SourceBytesEstimateExceedsZeroForLoadedSkin)
{
	EXPECT_GT(SettingsSkinSourceBytesEstimate(256, 128, 2), 0u);
}

TEST(SettingsResourceJobs, BytesBudgetCanTriggerReclaimBeforeCountFuse)
{
	EXPECT_TRUE(SettingsSkinResidencyShouldReclaim(true, false));
}

TEST(SettingsResourceJobs, CountFuseStillAppliesWhenBytesBudgetIsWithinLimit)
{
	EXPECT_TRUE(SettingsSkinResidencyShouldReclaim(false, true));
}

TEST(SettingsResourceJobs, WorkshopInstalledAssetCanUseWorkshopCatalogAndLocalBytes)
{
	EXPECT_STREQ(SettingsWorkshopCatalogSourceName(ESettingsWorkshopCatalogSource::WORKSHOP_CACHE), "workshop-cache");
	EXPECT_STREQ(SettingsWorkshopBytesSourceName(ESettingsWorkshopBytesSource::LOCAL_INSTALL), "local-install");
}

TEST(SettingsResourceJobs, CountryFlagPlanHandlesEmptyInput)
{
	const std::vector<int> vPlan = BuildSettingsCountryFlagWarmupPlan({});
	EXPECT_TRUE(vPlan.empty());
}

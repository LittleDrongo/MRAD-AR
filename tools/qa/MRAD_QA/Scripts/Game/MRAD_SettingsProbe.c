// Development-only integration checks. Call Run(true) only with an isolated
// QA -profile. No MRAD_Settings singleton or input binding is modified.
class MRAD_SettingsProbeInstance : MRAD_Settings
{
	void ReloadForProbe()
	{
		Load();
	}

	void NormalizeExtremesForProbe()
	{
		m_fReferenceWindSpeed = -1;
		m_iMaxRange = 5432;
		m_iRowsPerPage = 50;
		m_fFineStepMrad = -1;
		m_fCoarseStepMrad = 50;
		m_fElevationLimitMrad = -1;
		m_fWindageLimitMrad = 50.4;
		Sanitize();
	}
}

class MRAD_SettingsProbe
{
	protected static const string PROFILE_FILE = "$profile:MRAD/settings.json";
	protected static const string BACKUP_FILE = "$profile:MRAD/qa-settings-backup.json";
	protected static const ResourceName NOTEBOOK_LAYOUT = "{BFDD1C34CD9D8C2D}UI/layouts/NotepadSheet/NotepadSheet.layout";
	protected static const ResourceName HUD_LAYOUT = "{A7DE1002B62C4731}UI/layouts/MRAD/MRAD_ZeroingHUD.layout";
	protected static const ResourceName KEYBIND_CONFIG = "{4EE7794C9A3F11EF}Configs/System/keyBindingMenu.conf";
	protected static const ResourceName KEYBIND_ROW = "{75B1F7B766CA8C91}UI/layouts/Menus/SettingsMenu/BindingMenu/KeybindRow.layout";

	protected int m_iChecks;
	protected int m_iFailures;
	protected bool m_bHadProfile;
	protected string m_sProfileStatus = "SKIP_NOT_REQUESTED";
	protected string m_sUIStatus = "SKIP_NOT_RUN";
	protected ref array<Widget> m_aRoots = {};

	protected void Check(bool passed, string name)
	{
		m_iChecks++;
		if (passed)
			Print("MRAD_SETTINGS_PASS " + name);
		else
		{
			m_iFailures++;
			Print("MRAD_SETTINGS_FAIL " + name, LogLevel.ERROR);
		}
	}

	protected bool Near(float first, float second)
	{
		return Math.AbsFloat(first - second) < 0.0001;
	}

	protected bool HasDefaults(MRAD_Settings settings, int expectedRange = 1500)
	{
		return Near(settings.GetReferenceWindSpeed(), 4.0) && !settings.GetUseActualWind() && settings.GetMaxRange() == expectedRange && settings.GetRowsPerPage() == 10 && Near(settings.GetFineStepMrad(), 0.1) && Near(settings.GetCoarseStepMrad(), 1.0) && Near(settings.GetElevationLimitMrad(), 50.0) && Near(settings.GetWindageLimitMrad(), 50.0) && settings.GetShowHUD();
	}

	protected void CheckInMemoryDefaults()
	{
		MRAD_SettingsProbeInstance settings = new MRAD_SettingsProbeInstance();
		Check(HasDefaults(settings), "new independent settings instance has the specified defaults");
		settings.NormalizeExtremesForProbe();
		Check(Near(settings.GetReferenceWindSpeed(), 0) && settings.GetMaxRange() == 5000 && settings.GetRowsPerPage() == 12, "protected Sanitize clamps wind, range and row count");
		Check(Near(settings.GetFineStepMrad(), 0.1) && Near(settings.GetCoarseStepMrad(), 10), "protected Sanitize clamps the two step sizes");
		Check(Near(settings.GetElevationLimitMrad(), 1) && Near(settings.GetWindageLimitMrad(), 50), "protected Sanitize clamps the two limits");
	}

	protected bool PrepareProfileFixture()
	{
		// A previous interrupted run's backup must never be overwritten.
		if (FileIO.FileExists(BACKUP_FILE))
		{
			Check(false, "profile fixture refused because a previous QA backup exists");
			return false;
		}
		m_bHadProfile = FileIO.FileExists(PROFILE_FILE);
		if (!m_bHadProfile)
			return true;

		bool copied = FileIO.CopyFile(PROFILE_FILE, BACKUP_FILE);
		Check(copied, "existing profile JSON backed up before fixture writes");
		if (!copied)
		{
			// CopyFile may create an empty destination even when it returns false.
			if (FileIO.FileExists(BACKUP_FILE))
				FileIO.DeleteFile(BACKUP_FILE);
			return false;
		}
		return true;
	}

	protected void RestoreProfileFixture()
	{
		if (FileIO.FileExists(PROFILE_FILE) && !FileIO.DeleteFile(PROFILE_FILE))
		{
			Check(false, "fixture JSON could not be removed; QA backup retained");
			return;
		}
		if (!m_bHadProfile)
		{
			Check(!FileIO.FileExists(PROFILE_FILE), "fixture removed; originally absent profile remains absent");
			return;
		}

		bool restored = FileIO.CopyFile(BACKUP_FILE, PROFILE_FILE);
		Check(restored, "original profile JSON restored from its byte-copy backup");
		if (restored)
			Check(FileIO.DeleteFile(BACKUP_FILE), "temporary QA profile backup removed");
	}

	protected void CheckProfileRoundTrip()
	{
		int failuresBefore = m_iFailures;
		m_sProfileStatus = "FAIL";
		if (!PrepareProfileFixture())
			return;

		MRAD_SettingsProbeInstance settings = new MRAD_SettingsProbeInstance();
		int revision = settings.GetRevision();
		bool saved = settings.Apply(6.74, true, 2370, 7, 0.26, 2.24, 12.34, 14.56, false);
		Check(saved && FileIO.FileExists(PROFILE_FILE), "real Apply writes isolated settings.json");
		Check(settings.GetRevision() == revision + 1, "Apply advances revision for live consumers");
		if (saved)
		{
			MRAD_SettingsProbeInstance loaded = new MRAD_SettingsProbeInstance();
			loaded.ReloadForProbe();
			Check(Near(loaded.GetReferenceWindSpeed(), 6.7) && loaded.GetUseActualWind(), "JSON roundtrip preserves normalized wind and weather mode");
			Check(loaded.GetMaxRange() == 2300 && loaded.GetRowsPerPage() == 7, "JSON roundtrip preserves normalized range and row count");
			Check(Near(loaded.GetFineStepMrad(), 0.3) && Near(loaded.GetCoarseStepMrad(), 2.2), "JSON roundtrip preserves both normalized step sizes");
			Check(Near(loaded.GetElevationLimitMrad(), 12.3) && Near(loaded.GetWindageLimitMrad(), 14.6), "JSON roundtrip preserves both normalized limits");
			Check(!loaded.GetShowHUD(), "JSON roundtrip preserves disabled HUD");
		}

		JsonSaveContext partial = new JsonSaveContext();
		partial.WriteValue("version", 1);
		partial.WriteValue("maxRange", 1800);
		bool partialSaved = partial.SaveToFile(PROFILE_FILE);
		Check(partialSaved, "partial older-profile fixture writes successfully");
		if (partialSaved)
		{
			MRAD_SettingsProbeInstance older = new MRAD_SettingsProbeInstance();
			older.ReloadForProbe();
			Check(HasDefaults(older, 1800), "real Load supplies defaults for all absent JSON properties");
		}

		revision = settings.GetRevision();
		bool defaultsSaved = settings.RestoreDefaults();
		Check(defaultsSaved && settings.GetRevision() == revision + 1, "RestoreDefaults saves and advances revision");
		if (defaultsSaved)
		{
			MRAD_SettingsProbeInstance defaults = new MRAD_SettingsProbeInstance();
			defaults.ReloadForProbe();
			Check(HasDefaults(defaults), "restored defaults survive a fresh instance Load");
		}

		RestoreProfileFixture();
		if (m_iFailures == failuresBefore)
			m_sProfileStatus = "PASS";
	}

	protected Widget CreateLayout(WorkspaceWidget workspace, ResourceName resource, string name)
	{
		Widget root = workspace.CreateWidgets(resource);
		Check(root != null, name + " creates real widgets from the packaged layout");
		if (root)
		{
			// The probe checks resource/type integration, not rendered appearance.
			root.SetVisible(false);
			m_aRoots.Insert(root);
		}
		return root;
	}

	protected void CheckText(Widget root, string name)
	{
		Check(TextWidget.Cast(root.FindAnyWidget(name)) != null, name + " is a TextWidget");
	}

	protected void CheckNotebook(Widget root)
	{
		if (!root)
			return;
		Check(RTTextureWidget.Cast(root.FindAnyWidget("drg_Main_RTTexture")) != null, "notebook has the expected render-target widget");
		Check(RichTextWidget.Cast(root.FindAnyWidget("drg_Text")) != null, "notebook retains the legacy static RichText widget");
		Check(FrameWidget.Cast(root.FindAnyWidget("mrad_Dynamic")) != null, "notebook has its dynamic content frame");
		array<string> fields = {"mrad_Bullet", "mrad_Suppressed", "mrad_RangeHeader", "mrad_DropHeader", "mrad_WindHeader", "mrad_Status", "mrad_Page"};
		foreach (string name : fields)
			CheckText(root, name);
		// 12 explicit rows for each column prevent multiline RT overdraw.
		for (int i = 0; i < 12; i++)
		{
			CheckText(root, "mrad_Range" + i.ToString());
			CheckText(root, "mrad_Drop" + i.ToString());
			CheckText(root, "mrad_Wind" + i.ToString());
		}
	}

	protected void CheckSettingsLayout(Widget root)
	{
		if (!root)
			return;
		Check(MRAD_SettingsMenu.Cast(root.FindHandler(MRAD_SettingsMenu)) != null, "settings layout attaches the real MRAD_SettingsMenu handler");
		array<string> edits = {"MRAD_Wind", "MRAD_Range", "MRAD_Rows", "MRAD_Fine", "MRAD_Coarse", "MRAD_Elevation", "MRAD_Windage"};
		foreach (string name : edits)
			Check(EditBoxWidget.Cast(root.FindAnyWidget(name)) != null, name + " is an EditBoxWidget");
		Check(CheckBoxWidget.Cast(root.FindAnyWidget("MRAD_ActualWind")) != null, "actual-wind option is a CheckBoxWidget");
		Check(CheckBoxWidget.Cast(root.FindAnyWidget("MRAD_ShowHUD")) != null, "HUD option is a CheckBoxWidget");
		Check(ButtonWidget.Cast(root.FindAnyWidget("MRAD_Apply")) != null, "Apply is a ButtonWidget");
		Check(ButtonWidget.Cast(root.FindAnyWidget("MRAD_Defaults")) != null, "Defaults is a ButtonWidget");
		CheckText(root, "MRAD_Status");
		Check(ScrollLayoutWidget.Cast(root.FindAnyWidget("MRAD_SettingsScroll")) != null, "settings parameters have a scroll container");
		Check(ScrollLayoutWidget.Cast(root.FindAnyWidget("MRAD_KeybindScroll")) != null, "settings bindings have a scroll container");
		Check(VerticalLayoutWidget.Cast(root.FindAnyWidget("MRAD_KeybindRows")) != null, "native keybind rows have the expected parent type");
	}

	protected void CheckRegisteredLanguages()
	{
		string originalLanguage;
		WidgetManager.GetLanguage(originalLanguage);
		Print("MRAD_SETTINGS_LANGUAGE original=" + originalLanguage);
		array<string> languages = {"en_us", "ru_ru", "uk_ua", "de_de", "fr_fr", "it_it", "es_es", "cs_cz", "pl_pl", "pt_br", "ja_jp", "ko_kr", "zh_cn"};
		array<string> titles = {"MRAD settings", "Настройки MRAD", "Налаштування MRAD", "MRAD-Einstellungen", "Paramètres MRAD", "Impostazioni MRAD", "Ajustes de MRAD", "Nastavení MRAD", "Ustawienia MRAD", "Configurações MRAD", "MRAD設定", "MRAD 설정", "MRAD 设置"};
		array<string> applyLabels = {"Apply", "Применить", "Застосувати", "Anwenden", "Appliquer", "Applica", "Aplicar", "Použít", "Zastosuj", "Aplicar", "適用", "적용", "应用"};
		foreach (int i, string language : languages)
		{
			WidgetManager.SetLanguage(language);
			Check(WidgetManager.Translate("#MRAD_SettingsTitle") == titles[i], "runtime language title matches source " + language);
			Check(WidgetManager.Translate("#MRAD_Apply") == applyLabels[i], "runtime language action label matches source " + language);
		}
		WidgetManager.SetLanguage(originalLanguage);
	}

	// -1 means unavailable input services, not a successful input/UI test.
	protected int CheckNativeRows(WorkspaceWidget workspace, Widget settingsRoot)
	{
		if (!settingsRoot)
			return 0;
		if (!GetGame().GetInputManager() || !GetGame().GetSettingsManager())
		{
			Print("MRAD_SETTINGS_UI_SKIP native input/settings services unavailable", LogLevel.WARNING);
			return -1;
		}
		SCR_SettingsManagerKeybindModule module = SCR_SettingsManagerKeybindModule.Cast(GetGame().GetSettingsManager().GetModule(ESettingManagerModuleType.SETTINGS_MANAGER_KEYBINDING));
		if (!module || !module.GetInputBindings())
		{
			Print("MRAD_SETTINGS_UI_SKIP native InputBinding unavailable", LogLevel.WARNING);
			return -1;
		}
		InputBinding binding = module.GetInputBindings();
		SCR_KeyBindingMenuConfig config = SCR_KeyBindingMenuConfig.Cast(SCR_BaseContainerTools.CreateInstanceFromPrefab(KEYBIND_CONFIG, true));
		Check(config && config.m_KeyBindingCategories, "merged native keybinding config deserializes");
		if (!config || !config.m_KeyBindingCategories)
			return 0;
		Widget parent = settingsRoot.FindAnyWidget("MRAD_KeybindRows");
		if (!parent)
			return 0;

		array<string> expected = {"MRAD_ElevationUpFine", "MRAD_ElevationDownFine", "MRAD_WindageLeftFine", "MRAD_WindageRightFine", "MRAD_ElevationUpCoarse", "MRAD_ElevationDownCoarse", "MRAD_WindageLeftCoarse", "MRAD_WindageRightCoarse", "MRAD_ResetZeroing"};
		array<string> seen = {};
		int categories, rows;
		foreach (SCR_KeyBindingCategory category : config.m_KeyBindingCategories)
		{
			if (!category || category.m_sName != "MRAD")
				continue;
			categories++;
			foreach (SCR_KeyBindingEntry entry : category.m_KeyBindingEntries)
			{
				if (!entry || entry.m_sActionName.IsEmpty())
					continue;
				Check(expected.Find(entry.m_sActionName) >= 0 && seen.Find(entry.m_sActionName) == -1, "native category contains unique expected action " + entry.m_sActionName);
				seen.Insert(entry.m_sActionName);
				Widget widget = workspace.CreateWidgets(KEYBIND_ROW, parent);
				Check(widget != null, "native keybind row creates for " + entry.m_sActionName);
				if (!widget)
					continue;
				SCR_KeybindRowComponent row = SCR_KeybindRowComponent.FindComponent(widget);
				Check(row != null, "native keybind row component attaches for " + entry.m_sActionName);
				if (!row)
					continue;
				row.Init(binding, entry);
				Check(row.GetActionName() == entry.m_sActionName && row.GetActionPreset() == "click", "native row initializes its action and preset " + entry.m_sActionName);
				rows++;
				PrintFormat("MRAD_SETTINGS_NATIVE_BINDING action=%1 keyboardBindings=%2", entry.m_sActionName, binding.GetBindingsCount(entry.m_sActionName, EInputDeviceType.KEYBOARD, entry.m_sPreset));
			}
		}
		Check(categories == 1 && rows == 9 && seen.Count() == 9, "exactly one MRAD category creates all nine native keybind rows");
		return rows;
	}

	protected void CheckLayouts()
	{
		if (System.IsConsoleApp())
		{
			m_sUIStatus = "SKIP_HEADLESS";
			Print("MRAD_SETTINGS_UI_SKIP console/headless runtime has no rendering or input", LogLevel.WARNING);
			return;
		}
		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace)
		{
			m_sUIStatus = "SKIP_NO_WORKSPACE";
			Print("MRAD_SETTINGS_UI_SKIP WorkspaceWidget unavailable", LogLevel.WARNING);
			return;
		}

		int failuresBefore = m_iFailures;
		CheckRegisteredLanguages();
		// Real translation catches a missing runtime table/project registration;
		// successful CreateWidgets alone also accepts unresolved string IDs.
		array<string> localizedKeys = {"#MRAD_Tab", "#MRAD_Apply", "#MRAD_RestoreDefaults", "#MRAD_ReferenceWind", "#MRAD_ActualWind", "#MRAD_ZeroingHUD", "#MRAD_BindElevationUpFine", "#MRAD_BindResetZeroing"};
		foreach (string key : localizedKeys)
		{
			string translated = WidgetManager.Translate(key);
			Check(!translated.IsEmpty() && translated != key, "registered runtime string resolves " + key);
		}
		Widget notebook = CreateLayout(workspace, NOTEBOOK_LAYOUT, "notebook");
		CheckNotebook(notebook);
		Widget hud = CreateLayout(workspace, HUD_LAYOUT, "optics HUD");
		if (hud)
		{
			CheckText(hud, "MRAD_ElevationValue");
			CheckText(hud, "MRAD_WindageValue");
			CheckText(hud, "MRAD_ZeroingStatus");
		}
		Widget settings = CreateLayout(workspace, MRAD_SettingsMenu.LAYOUT, "settings");
		CheckSettingsLayout(settings);
		int rows = CheckNativeRows(workspace, settings);
		foreach (Widget root : m_aRoots)
			root.RemoveFromHierarchy();
		m_aRoots.Clear();

		if (m_iFailures != failuresBefore)
			m_sUIStatus = "FAIL";
		else if (rows == -1)
			m_sUIStatus = "PARTIAL_NO_NATIVE_INPUT";
		else
			m_sUIStatus = "LAYOUT_AND_ROWS_PASS";
		Print("MRAD_SETTINGS_VISUAL_SKIP hidden widget integration is not a visual or key-capture acceptance test", LogLevel.WARNING);
	}

	protected int RunChecks(bool allowProfileWrite)
	{
		CheckInMemoryDefaults();
		if (allowProfileWrite)
			CheckProfileRoundTrip();
		else
			Print("MRAD_SETTINGS_PROFILE_SKIP isolated-profile write opt-in was not supplied", LogLevel.WARNING);

		if (GetGame())
			CheckLayouts();
		else
		{
			m_sUIStatus = "SKIP_NO_GAME";
			Print("MRAD_SETTINGS_UI_SKIP Game unavailable", LogLevel.WARNING);
		}
		if (allowProfileWrite && GetGame())
			m_iFailures += MRAD_InputBindingProbe.Run();
		PrintFormat("MRAD_SETTINGS_RESULT checks=%1 failures=%2 profile=%3 ui=%4", m_iChecks, m_iFailures, m_sProfileStatus, m_sUIStatus);
		return m_iFailures;
	}

	static int Run(bool allowProfileWrite = false)
	{
		MRAD_SettingsProbe probe = new MRAD_SettingsProbe();
		return probe.RunChecks(allowProfileWrite);
	}
}

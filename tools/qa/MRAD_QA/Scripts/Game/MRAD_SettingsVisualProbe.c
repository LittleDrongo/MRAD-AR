// QA addon only. Opens the actual native Settings menu and the product MRAD tab.
// No simulated mouse/key events, binding capture, or settings mutation.
modded class SCR_SettingsSuperMenu
{
	SCR_TabViewComponent MRAD_QA_GetTabView()
	{
		if (!m_SuperMenuComponent)
			return null;
		return m_SuperMenuComponent.GetTabView();
	}
}

class MRAD_SettingsVisualProbe
{
	protected static ref MRAD_SettingsVisualProbe s_Instance;
	protected SCR_SettingsSuperMenu m_Menu;
	protected int m_iChecks;
	protected int m_iFailures;

	protected void Check(bool passed, string description)
	{
		m_iChecks++;
		if (passed)
			Print("MRAD_SETTINGS_VISUAL_PASS " + description);
		else
		{
			m_iFailures++;
			Print("MRAD_SETTINGS_VISUAL_FAIL " + description, LogLevel.ERROR);
		}
	}

	protected bool OpenMenu()
	{
		if (System.IsConsoleApp() || !GetGame().GetWorkspace())
		{
			Print("MRAD_SETTINGS_VISUAL_SKIP rendering workspace unavailable", LogLevel.WARNING);
			return false;
		}
		MenuManager manager = GetGame().GetMenuManager();
		Check(manager != null, "native MenuManager is available");
		if (!manager)
			return false;

		m_Menu = SCR_SettingsSuperMenu.Cast(manager.OpenMenu(ChimeraMenuPreset.SettingsSuperMenu, 0, true));
		Check(m_Menu != null, "native SettingsSuperMenu opens through MenuManager");
		if (!m_Menu)
			return false;
		SCR_TabViewComponent tabs = m_Menu.MRAD_QA_GetTabView();
		Check(tabs != null, "native Settings menu provides its real TabView");
		if (!tabs)
			return false;
		// Native overflow needs a laid-out tab bar. Follow exactly the native
		// OnTabRight path; a direct jump can skip its boundary-reveal behavior.
		GetGame().GetCallqueue().CallLater(SelectMRADUsingNativePaging, 500, false);
		return true;
	}

	protected void SelectMRADUsingNativePaging()
	{
		SCR_TabViewComponent tabs = m_Menu.MRAD_QA_GetTabView();
		int steps;
		for (int i = 0; i < tabs.GetTabCount(); i++)
		{
			SCR_TabViewContent selected = tabs.GetContent(tabs.GetShownTab());
			if (selected && selected.m_sTabIdentifier == "MRAD")
				break;
			int next = tabs.GetNextValidItem(false);
			if (next < 0)
				break;
			tabs.ShowTab(next, true, false);
			steps++;
		}
		PrintFormat("MRAD_SETTINGS_VISUAL_NAVIGATION nativePagingSteps=%1", steps);
		GetGame().GetCallqueue().CallLater(Inspect, 500, false);
	}

	protected void CheckGeometry(Widget root, string name)
	{
		Widget widget = root.FindAnyWidget(name);
		Check(widget != null, "native MRAD tab contains " + name);
		if (!widget)
			return;
		float x, y, width, height;
		widget.GetScreenPos(x, y);
		widget.GetScreenSize(width, height);
		Check(widget.IsVisibleInHierarchy() && width > 0 && height > 0, name + " has visible nonzero geometry");
		PrintFormat("MRAD_SETTINGS_VISUAL_RECT name=%1 x=%2 y=%3 width=%4 height=%5", name, x, y, width, height);
	}

	protected void Inspect()
	{
		SCR_TabViewComponent tabs = m_Menu.MRAD_QA_GetTabView();
		int count;
		for (int i = 0; i < tabs.GetTabCount(); i++)
		{
			SCR_TabViewContent item = tabs.GetContent(i);
			if (item && item.m_sTabIdentifier == "MRAD")
				count++;
		}
		Check(count == 1, "Settings contains exactly one MRAD tab");
		SCR_TabViewContent shown = tabs.GetContent(tabs.GetShownTab());
		Check(shown && shown.m_sTabIdentifier == "MRAD", "native TabView selects MRAD");
		Check(shown && shown.m_wButton && shown.m_wButton.IsVisibleInHierarchy(), "MRAD tab button is visible after native paging");
		if (shown && shown.m_wButton)
		{
			float x, y, width, height, screenWidth, screenHeight;
			shown.m_wButton.GetScreenPos(x, y);
			shown.m_wButton.GetScreenSize(width, height);
			GetGame().GetWorkspace().GetScreenSize(screenWidth, screenHeight);
			Check(x >= 0 && y >= 0 && width > 0 && height > 0 && x + width <= screenWidth && y + height <= screenHeight, "MRAD tab button lies inside the client viewport");
			PrintFormat("MRAD_SETTINGS_VISUAL_TAB_RECT x=%1 y=%2 width=%3 height=%4", x, y, width, height);
		}
		Widget root = tabs.GetContentWidget(tabs.GetShownTab());
		Check(root != null, "selected tab has its actual content widget");
		int rowCount;
		if (root)
		{
			Check(MRAD_SettingsMenu.Cast(root.FindHandler(MRAD_SettingsMenu)) != null, "selected native tab uses the product settings handler");
			array<string> names = {"MRAD_SettingsScroll", "MRAD_KeybindScroll", "MRAD_SettingsTitle", "MRAD_KeybindsTitle", "MRAD_Wind", "MRAD_Range", "MRAD_Apply", "MRAD_Defaults"};
			foreach (string name : names)
				CheckGeometry(root, name);
			Widget rows = root.FindAnyWidget("MRAD_KeybindRows");
			if (rows)
			{
				Widget child = rows.GetChildren();
				while (child)
				{
					SCR_KeybindRowComponent row = SCR_KeybindRowComponent.FindComponent(child);
					if (row && row.GetActionName().StartsWith("MRAD_"))
						rowCount++;
					child = child.GetSibling();
				}
			}
		}
		Check(rowCount == 9, "real OnTabShow creates nine native binding rows");
		string language;
		WidgetManager.GetLanguage(language);
		PrintFormat("MRAD_SETTINGS_VISUAL_RESULT checks=%1 failures=%2 tab=MRAD rows=%3 language=%4", m_iChecks, m_iFailures, rowCount, language);
		if (m_iFailures == 0)
			Print("MRAD_SETTINGS_VISUAL_READY actual native Settings/MRAD is ready for owned-window capture");
	}

	// Caller must keep the isolated GUI client alive for 10 seconds after true.
	static bool Open()
	{
		s_Instance = new MRAD_SettingsVisualProbe();
		return s_Instance.OpenMenu();
	}

	static void Close()
	{
		if (!s_Instance)
			return;
		if (s_Instance.m_Menu)
			s_Instance.m_Menu.Close();
		s_Instance.m_Menu = null;
		s_Instance = null;
	}
}

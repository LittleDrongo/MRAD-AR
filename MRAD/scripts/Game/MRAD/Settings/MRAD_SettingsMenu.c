// Собственная вкладка использует штатные строки биндов: захват сочетаний,
// показ конфликтов и сохранение выполняет тот же код, что в Controls.
class MRAD_SettingsMenu : SCR_SubMenuBase
{
	static const ResourceName LAYOUT = "{7AF9376463AB7151}UI/layouts/MRADSettings/MRAD_Settings.layout";
	protected static const ResourceName KEYBIND_CONFIG = "{4EE7794C9A3F11EF}Configs/System/keyBindingMenu.conf";
	protected static const ResourceName KEYBIND_ROW = "{75B1F7B766CA8C91}UI/layouts/Menus/SettingsMenu/BindingMenu/KeybindRow.layout";

	protected ref InputBinding m_Binding;
	protected ref array<ref MRAD_KeybindRowBinding> m_aKeybindRows = {};
	protected EditBoxWidget m_Wind;
	protected EditBoxWidget m_Range;
	protected EditBoxWidget m_Rows;
	protected EditBoxWidget m_Fine;
	protected EditBoxWidget m_Coarse;
	protected EditBoxWidget m_Elevation;
	protected EditBoxWidget m_Windage;
	protected CheckBoxWidget m_ActualWind;
	protected CheckBoxWidget m_ShowHUD;
	protected TextWidget m_Status;

	override void OnTabCreate(Widget menuRoot, ResourceName buttonsLayout, int index)
	{
		super.OnTabCreate(menuRoot, buttonsLayout, index);
		m_Wind = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_Wind"));
		m_Range = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_Range"));
		m_Rows = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_Rows"));
		m_Fine = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_Fine"));
		m_Coarse = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_Coarse"));
		m_Elevation = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_Elevation"));
		m_Windage = EditBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_Windage"));
		m_ActualWind = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_ActualWind"));
		m_ShowHUD = CheckBoxWidget.Cast(m_wRoot.FindAnyWidget("MRAD_ShowHUD"));
		m_Status = TextWidget.Cast(m_wRoot.FindAnyWidget("MRAD_Status"));
		LoadValues();
	}

	override void OnTabShow()
	{
		super.OnTabShow();
		GetGame().GetWorkspace().SetFocusedWidget(null);
		CreateKeybindRows();
		LoadValues();
	}

	override void OnTabHide()
	{
		if (m_Binding)
			m_Binding.Save();
		GetGame().SaveUserSettings();
		super.OnTabHide();
	}

	override void OnTabRemove()
	{
		GetGame().GetCallqueue().Remove(CreateKeybindRows);
		foreach (MRAD_KeybindRowBinding binding : m_aKeybindRows)
			binding.Detach();
		m_aKeybindRows.Clear();
		super.OnTabRemove();
	}

	void RefreshKeybindRows()
	{
		// Не удалять строку внутри ее собственного callback захвата клавиши.
		GetGame().GetCallqueue().Remove(CreateKeybindRows);
		GetGame().GetCallqueue().CallLater(CreateKeybindRows, 1, false);
	}

	protected void CreateKeybindRows()
	{
		Widget parent = m_wRoot.FindAnyWidget("MRAD_KeybindRows");
		if (!parent)
			return;

		foreach (MRAD_KeybindRowBinding oldBinding : m_aKeybindRows)
			oldBinding.Detach();
		m_aKeybindRows.Clear();
		Widget child = parent.GetChildren();
		while (child)
		{
			Widget next = child.GetSibling();
			child.RemoveFromHierarchy();
			child = next;
		}

		SCR_KeyBindingMenuConfig config = SCR_KeyBindingMenuConfig.Cast(SCR_BaseContainerTools.CreateInstanceFromPrefab(KEYBIND_CONFIG, true));
		if (!config || !config.m_KeyBindingCategories)
			return;

		SCR_SettingsManagerKeybindModule module = SCR_SettingsManagerKeybindModule.Cast(GetGame().GetSettingsManager().GetModule(ESettingManagerModuleType.SETTINGS_MANAGER_KEYBINDING));
		if (!module)
			return;
		m_Binding = module.GetInputBindings();
		foreach (SCR_KeyBindingCategory category : config.m_KeyBindingCategories)
		{
			if (!category || category.m_sName != "MRAD")
				continue;

			foreach (SCR_KeyBindingEntry entry : category.m_KeyBindingEntries)
			{
				if (!entry || entry.m_sActionName.IsEmpty())
					continue;

				Widget widget = GetGame().GetWorkspace().CreateWidgets(KEYBIND_ROW, parent);
				if (!widget)
					continue;

				SCR_KeybindRowComponent row = SCR_KeybindRowComponent.FindComponent(widget);
				if (row)
				{
					row.Init(m_Binding, entry);
					MRAD_KeybindRowBinding rowBinding = new MRAD_KeybindRowBinding();
					rowBinding.Init(row, m_Binding, this);
					m_aKeybindRows.Insert(rowBinding);
				}
			}
			break;
		}
	}

	protected bool HasControls()
	{
		return m_Wind && m_Range && m_Rows && m_Fine && m_Coarse && m_Elevation && m_Windage && m_ActualWind && m_ShowHUD;
	}

	protected void LoadValues()
	{
		if (!HasControls())
			return;

		MRAD_Settings settings = MRAD_Settings.Get();
		m_Wind.SetText(settings.GetReferenceWindSpeed().ToString(0, 1));
		m_Range.SetText(settings.GetMaxRange().ToString());
		m_Rows.SetText(settings.GetRowsPerPage().ToString());
		m_Fine.SetText(settings.GetFineStepMrad().ToString(0, 1));
		m_Coarse.SetText(settings.GetCoarseStepMrad().ToString(0, 1));
		m_Elevation.SetText(settings.GetElevationLimitMrad().ToString(0, 1));
		m_Windage.SetText(settings.GetWindageLimitMrad().ToString(0, 1));
		m_ActualWind.SetChecked(settings.GetUseActualWind());
		m_ShowHUD.SetChecked(settings.GetShowHUD());
	}

	// Неверный текст не превращается молча в ноль. Запятая поддерживается
	// как разделитель при вводе; формат сохранения и вывода остается с точкой.
	protected bool ReadNumber(EditBoxWidget field, float minimum, float maximum, out float value, bool integerOnly = false)
	{
		string text = field.GetText();
		text.TrimInPlace();
		text.Replace(",", ".");
		if (text.IsEmpty())
			return false;

		bool decimalPoint;
		bool digit;
		string digits = "0123456789";
		for (int i = 0; i < text.Length(); i++)
		{
			string character = text.Substring(i, 1);
			if (digits.Contains(character))
			{
				digit = true;
				continue;
			}
			if (character == "." && !decimalPoint && !integerOnly)
			{
				decimalPoint = true;
				continue;
			}
			return false;
		}
		if (!digit)
			return false;

		value = text.ToFloat();
		return value >= minimum && value <= maximum;
	}

	protected void ApplyValues()
	{
		if (!HasControls())
			return;

		float wind, distance, rows, fine, coarse, elevation, windage;
		if (!ReadNumber(m_Wind, 0, 30, wind) || !ReadNumber(m_Range, 100, 5000, distance, true) || !ReadNumber(m_Rows, 5, 12, rows, true) || !ReadNumber(m_Fine, 0.1, 5, fine) || !ReadNumber(m_Coarse, 0.1, 10, coarse) || !ReadNumber(m_Elevation, 1, 50, elevation) || !ReadNumber(m_Windage, 1, 50, windage))
		{
			SetStatus("#MRAD_SettingsInvalid");
			return;
		}

		int maxRange = distance;
		int rowsPerPage = rows;
		bool saved = MRAD_Settings.Get().Apply(wind, m_ActualWind.IsChecked(), maxRange, rowsPerPage, fine, coarse, elevation, windage, m_ShowHUD.IsChecked());
		LoadValues();
		ShowSaveResult(saved);
	}

	protected void SetStatus(string text)
	{
		if (m_Status)
			m_Status.SetText(WidgetManager.Translate(text));
	}

	protected void ShowSaveResult(bool saved)
	{
		if (saved)
			SetStatus("#MRAD_SettingsSaved");
		else
			SetStatus("#MRAD_SettingsSaveFailed");
	}

	override bool OnClick(Widget w, int x, int y, int button)
	{
		Widget target = w;
		while (target && target != m_wRoot)
		{
			if (target.GetName() == "MRAD_Apply")
			{
				ApplyValues();
				return true;
			}
			if (target.GetName() == "MRAD_Defaults")
			{
				bool saved = MRAD_Settings.Get().RestoreDefaults();
				LoadValues();
				ShowSaveResult(saved);
				return true;
			}
			target = target.GetParent();
		}
		return super.OnClick(w, x, y, button);
	}
}

// Штатная строка передает inline Reset/Unbind/Advanced родительскому меню.
// Отдельный обработчик сохраняет связь с конкретной строкой без общего focus-state.
class MRAD_KeybindRowBinding
{
	protected SCR_KeybindRowComponent m_Row;
	protected ref InputBinding m_Binding;
	protected MRAD_SettingsMenu m_Menu;
	protected SCR_AdvancedKeybindDialogUI m_AdvancedDialog;

	void Init(SCR_KeybindRowComponent row, InputBinding binding, MRAD_SettingsMenu menu)
	{
		m_Row = row;
		m_Binding = binding;
		m_Menu = menu;
		m_Row.GetOnInlineButton().Insert(OnInlineButton);
		m_Row.GetOnKeyCaptured().Insert(OnKeyCaptured);
	}

	void Detach()
	{
		if (m_Row)
		{
			m_Row.GetOnInlineButton().Remove(OnInlineButton);
			m_Row.GetOnKeyCaptured().Remove(OnKeyCaptured);
		}
		if (m_AdvancedDialog)
			m_AdvancedDialog.m_OnCancel.Remove(OnKeyCaptured);
		m_Row = null;
		m_Binding = null;
		m_Menu = null;
		m_AdvancedDialog = null;
	}

	protected void OnKeyCaptured()
	{
		if (m_Binding)
			m_Binding.Save();
		if (m_Menu)
			m_Menu.RefreshKeybindRows();
	}

	protected void OnInlineButton(string name)
	{
		if (!m_Row)
			return;

		if (name == SCR_KeybindRowComponent.BUTTON_RESET)
			m_Row.ResetAction();
		else if (name == SCR_KeybindRowComponent.BUTTON_UNBIND)
			m_Row.Unbind();
		else if (name == SCR_KeybindRowComponent.BUTTON_ADVANCED_KEYBIND)
		{
			m_AdvancedDialog = SCR_KeybindDialogs.CreateAdvancedKeybindDialog(m_Row.GetEntry(), m_Row.GetDisplayName(), m_Row.GetActionName(), m_Row.GetActionPreset(), m_Row.GetActionPrefixType());
			if (m_AdvancedDialog)
				m_AdvancedDialog.m_OnCancel.Insert(OnKeyCaptured);
		}
	}
}

// Публичный API имеет AddTab, но не реестр вкладок модов. Этот единственный
// UI hook только добавляет нашу вкладку; существующие вкладки не заменяются.
modded class SCR_SettingsSuperMenu
{
	override void OnMenuOpen()
	{
		super.OnMenuOpen();
		if (!m_SuperMenuComponent)
			return;

		SCR_TabViewComponent tabs = m_SuperMenuComponent.GetTabView();
		if (!tabs)
			return;

		for (int i = 0; i < tabs.GetTabCount(); i++)
		{
			SCR_TabViewContent content = tabs.GetContent(i);
			if (content && content.m_sTabIdentifier == "MRAD")
				return;
		}
		tabs.AddTab(MRAD_SettingsMenu.LAYOUT, "#MRAD_Tab", true, string.Empty, string.Empty, 32, 32, string.Empty, string.Empty, "MRAD");
	}
}

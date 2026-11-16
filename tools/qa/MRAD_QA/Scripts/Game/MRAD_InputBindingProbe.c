// QA only. Two separate GUI clients must share one isolated -profile.
// First: -mradBindingWrite. Second: -mradBindingRead. Never starts input capture.
class MRAD_InputBindingProbe
{
	protected static const string ACTION = "MRAD_ResetZeroing";
	protected static const string PRESET = "click";
	protected static const string BACKUP = "$profile:MRAD/qa-input-backup.json";
	protected ref InputBinding m_Binding;
	protected ref array<string> m_aOriginal = {};
	protected ref array<string> m_aOriginalActive = {};
	protected bool m_bOriginalActiveFound;
	protected int m_iOriginalCount;
	protected int m_iChecks;
	protected int m_iFailures;

	protected void Check(bool passed, string description)
	{
		m_iChecks++;
		if (passed)
			Print("MRAD_INPUT_PASS " + description);
		else
		{
			m_iFailures++;
			Print("MRAD_INPUT_FAIL " + description, LogLevel.ERROR);
		}
	}

	protected bool ContainsInput(array<string> inputs, string name)
	{
		foreach (string input : inputs)
		{
			if (input.Contains(name))
				return true;
		}
		return false;
	}

	protected bool SameInputs(array<string> first, array<string> second)
	{
		if (first.Count() != second.Count())
			return false;
		foreach (int i, string value : first)
		{
			if (value != second[i])
				return false;
		}
		return true;
	}

	protected void CheckMapping(InputBinding binding, string phase)
	{
		array<string> inputs = {};
		bool found = binding.GetBindings(ACTION, inputs, EInputDeviceType.KEYBOARD, PRESET, false);
		// 1.9 GetBindings returns false/empty for the observed InputSourceCombo.
		// The active InputManager API returns the complete postfix chord stack;
		// use that representation for the required readback, not a flat list.
		PrintFormat("MRAD_INPUT_LEGACY_LIST phase=%1 found=%2 count=%3", phase, found, inputs.Count());
		Check(!binding.IsDefault(ACTION, EInputDeviceType.KEYBOARD, PRESET), phase + " reads the custom binding flag");
		Check(binding.GetBindingsCount(ACTION, EInputDeviceType.KEYBOARD, PRESET) == 1, phase + " has exactly one chord binding");
		foreach (string input : inputs)
			PrintFormat("MRAD_INPUT_RAW phase=%1 input=%2", phase, input);

		array<string> activeKeys = {};
		array<BaseContainer> activeFilters = {};
		bool active = GetGame().GetInputManager().GetActionKeybinding(ACTION, activeKeys, activeFilters, EInputDeviceType.KEYBOARD, PRESET, 0);
		Check(active && ContainsInput(activeKeys, "KC_RCONTROL") && ContainsInput(activeKeys, "KC_NUMPAD6"), phase + " active InputManager map contains both chord keys");
		Check(activeKeys.Count() == 3 && activeKeys.Contains("+"), phase + " active stack combines exactly two keys as one chord");
		foreach (string activeKey : activeKeys)
			PrintFormat("MRAD_INPUT_ACTIVE phase=%1 input=%2", phase, activeKey);
	}

	protected bool WriteBackup()
	{
		// Only a default/unassigned target is accepted, so ResetDefault restores
		// precisely the previous state. Existing custom bindings are never edited.
		Check(!FileIO.FileExists(BACKUP), "no interrupted probe backup exists");
		Check(m_Binding.IsDefault(ACTION, EInputDeviceType.KEYBOARD, PRESET), "target action is still default before any write");
		if (m_iFailures != 0)
			return false;
		m_iOriginalCount = m_Binding.GetBindingsCount(ACTION, EInputDeviceType.KEYBOARD, PRESET);
		m_Binding.GetBindings(ACTION, m_aOriginal, EInputDeviceType.KEYBOARD, PRESET, false);
		array<BaseContainer> originalFilters = {};
		m_bOriginalActiveFound = GetGame().GetInputManager().GetActionKeybinding(ACTION, m_aOriginalActive, originalFilters, EInputDeviceType.KEYBOARD, PRESET, 0);
		FileIO.MakeDirectory("$profile:MRAD");
		JsonSaveContext save = new JsonSaveContext();
		save.WriteValue("probe", "MRAD_INPUT_V2");
		save.WriteValue("action", ACTION);
		save.WriteValue("wasDefault", true);
		save.WriteValue("count", m_iOriginalCount);
		save.WriteValue("inputs", m_aOriginal);
		save.WriteValue("activeInputs", m_aOriginalActive);
		save.WriteValue("activeFound", m_bOriginalActiveFound);
		bool saved = save.SaveToFile(BACKUP);
		Check(saved, "original target binding snapshot saved before mutation");
		return saved;
	}

	protected bool ReadBackup()
	{
		JsonLoadContext load = new JsonLoadContext();
		bool loaded = FileIO.FileExists(BACKUP) && load.LoadFromFile(BACKUP);
		Check(loaded, "write-phase backup exists for the second process");
		if (!loaded)
			return false;
		string probe, action;
		bool wasDefault;
		load.ReadValueDefault("probe", probe, string.Empty);
		load.ReadValueDefault("action", action, string.Empty);
		load.ReadValueDefault("wasDefault", wasDefault, false);
		load.ReadValueDefault("count", m_iOriginalCount, -1);
		bool inputsLoaded = load.ReadValue("inputs", m_aOriginal);
		bool activeLoaded = load.ReadValue("activeInputs", m_aOriginalActive);
		bool activeFoundLoaded = load.ReadValue("activeFound", m_bOriginalActiveFound);
		bool valid = probe == "MRAD_INPUT_V2" && action == ACTION && wasDefault && m_iOriginalCount >= 0 && inputsLoaded && activeLoaded && activeFoundLoaded;
		Check(valid, "backup identifies only the expected originally-default action");
		return valid;
	}

	protected void WritePhase()
	{
		if (!WriteBackup())
			return;
		bool created = m_Binding.CreateUserBinding(ACTION, EInputDeviceType.KEYBOARD, PRESET);
		Check(created, "native CreateUserBinding succeeds");
		if (!created)
			return;
		int count = m_Binding.GetBindingsCount(ACTION, EInputDeviceType.KEYBOARD, PRESET);
		for (int i = count - 1; i >= 0; i--)
			m_Binding.RemoveBinding(ACTION, EInputDeviceType.KEYBOARD, PRESET, i);
		m_Binding.AddBinding(ACTION, PRESET, "keyboard:KC_NUMPAD6", "InputFilterDown");
		m_Binding.InsertCombo(ACTION, PRESET, "keyboard:KC_RCONTROL", string.Empty, 0, 0);
		m_Binding.Save();
		CheckMapping(m_Binding, "WRITE");
		InputBinding fresh = GetGame().GetInputManager().CreateUserBinding();
		Check(fresh != null, "a new native InputBinding object can read the saved map");
		if (fresh)
			CheckMapping(fresh, "WRITE_FRESH");
		// Leave the fixture only inside the explicitly isolated profile. The
		// next separate client verifies startup loading before restoring it.
	}

	protected void ReadAndRestorePhase()
	{
		if (!ReadBackup())
			return;
		CheckMapping(m_Binding, "READ_AFTER_RESTART");
		m_Binding.ResetDefault(ACTION, EInputDeviceType.KEYBOARD, PRESET);
		m_Binding.Save();
		array<string> restored = {};
		m_Binding.GetBindings(ACTION, restored, EInputDeviceType.KEYBOARD, PRESET, false);
		array<string> restoredActive = {};
		array<BaseContainer> restoredFilters = {};
		bool restoredFound = GetGame().GetInputManager().GetActionKeybinding(ACTION, restoredActive, restoredFilters, EInputDeviceType.KEYBOARD, PRESET, 0);
		bool matches = m_Binding.IsDefault(ACTION, EInputDeviceType.KEYBOARD, PRESET) && m_Binding.GetBindingsCount(ACTION, EInputDeviceType.KEYBOARD, PRESET) == m_iOriginalCount && SameInputs(restored, m_aOriginal) && restoredFound == m_bOriginalActiveFound && SameInputs(restoredActive, m_aOriginalActive);
		Check(matches, "native ResetDefault and Save restore the backed-up raw binding state");
		if (matches)
			Check(FileIO.DeleteFile(BACKUP), "backup removed only after exact target restoration");
	}

	protected int RunChecks(bool write, bool read)
	{
		Check(write != read, "exactly one binding probe phase is requested");
		Check(!System.IsConsoleApp() && GetGame().GetInputManager() && GetGame().GetSettingsManager(), "binding probe has a real GUI input service");
		if (m_iFailures == 0)
		{
			SCR_SettingsManagerKeybindModule module = SCR_SettingsManagerKeybindModule.Cast(GetGame().GetSettingsManager().GetModule(ESettingManagerModuleType.SETTINGS_MANAGER_KEYBINDING));
			if (module)
				m_Binding = module.GetInputBindings();
			Check(m_Binding != null, "binding probe uses the same native object as Controls and MRAD");
			if (m_Binding)
			{
				if (write)
					WritePhase();
				else
					ReadAndRestorePhase();
			}
		}
		string phase = "READ_RESTORE";
		if (write)
			phase = "WRITE";
		PrintFormat("MRAD_INPUT_RESULT phase=%1 checks=%2 failures=%3", phase, m_iChecks, m_iFailures);
		return m_iFailures;
	}

	static int Run()
	{
		bool write = System.IsCLIParam("mradBindingWrite");
		bool read = System.IsCLIParam("mradBindingRead");
		if (!write && !read)
			return 0;
		MRAD_InputBindingProbe probe = new MRAD_InputBindingProbe();
		return probe.RunChecks(write, read);
	}
}

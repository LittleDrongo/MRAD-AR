// Wind / Beaufort indicator that stays on screen outside ADS as well.
// It is updated from the HUD manager phase (a pure UI phase): no per-frame input
// context is created for it and the character control phase is not touched. The only
// input read here is the Shift+K toggle, from a low-priority gameplay context.
class MRAD_WindHUDController
{
	protected static ref MRAD_WindHUD s_HUD;
	protected static bool s_bToggleHeld;

	//------------------------------------------------------------------------------------------------
	static void Update()
	{
		if (SCR_Global.IsEditMode() || System.IsConsoleApp())
			return;

		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
		{
			Release();
			return;
		}

		MenuManager menus = GetGame().GetMenuManager();
		if (menus && menus.IsAnyMenuOpen())
		{
			Release();
			return;
		}

		InputManager input = GetGame().GetInputManager();
		if (input)
		{
			input.ActivateContext("MRAD_HUDContext");

			bool held = input.GetActionValue("MRAD_ToggleWindHUD") > 0.5;
			if (held && !s_bToggleHeld)
			{
				MRAD_Settings settings = MRAD_Settings.Get();
				settings.SetShowWindHUD(!settings.GetShowWindHUD());
			}
			s_bToggleHeld = held;
		}

		if (!MRAD_Settings.Get().GetShowWindHUD())
		{
			Release();
			return;
		}

		if (!s_HUD)
			s_HUD = new MRAD_WindHUD();

		s_HUD.Update(character);
	}

	//------------------------------------------------------------------------------------------------
	static void Release()
	{
		if (s_HUD)
			s_HUD.Delete();

		s_HUD = null;
	}
}

//------------------------------------------------------------------------------------------------
// The HUD manager runs its update every frame on the local player; the wind widget is
// a plain HUD overlay, so this phase is the right place for it.
modded class SCR_HUDManagerComponent
{
	protected override void OnUpdate(IEntity owner)
	{
		super.OnUpdate(owner);
		MRAD_WindHUDController.Update();
		MRAD_BreathController.Update();
		MRAD_NotepadInputController.Update();
	}
}

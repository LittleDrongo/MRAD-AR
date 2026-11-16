// The wind indicator is a player HUD element, not part of the scope: it is driven
// from the character control phase of the local player, so it works in ADS, from
// the hip and with excluded optics alike, and it does not need a full-screen widget
// that could get in the way of the HUD.
modded class SCR_CharacterControllerComponent
{
	protected const string WIND_CONTEXT = "MRAD_WindHUDContext";

	protected ref MRAD_WindHUD m_MRADWindHUD;

	//------------------------------------------------------------------------------------------------
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		super.OnPrepareControls(owner, am, dt, player);

		if (!player || !am || SCR_Global.IsEditMode())
			return;

		// Only the locally controlled character: a dedicated server must not read
		// anybody else's input.
		PlayerController localPlayer = GetGame().GetPlayerController();
		if (!localPlayer || localPlayer.GetControlledEntity() != owner)
			return;

		// The context lives for this frame only.
		am.ActivateContext(WIND_CONTEXT);

		if (!m_MRADWindHUD)
			m_MRADWindHUD = new MRAD_WindHUD();

		bool toggleHeld = am.GetActionValue("MRAD_ToggleWindHUD") > 0.5;
		m_MRADWindHUD.Update(ChimeraCharacter.Cast(owner), toggleHeld);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_MRADWindHUD)
		{
			m_MRADWindHUD.Delete();
			m_MRADWindHUD = null;
		}

		super.OnDelete(owner);
	}
}

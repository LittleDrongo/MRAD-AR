// The wind indicator is a player HUD element, not part of the scope: it is drawn
// every frame from the character control phase of the local player. Key input is
// not read here on purpose: a context activated every frame takes inputs away
// from the character (camera and sprint), so the toggle lives in the scope context.
modded class SCR_CharacterControllerComponent
{
	protected ref MRAD_WindHUD m_MRADWindHUD;

	//------------------------------------------------------------------------------------------------
	override void OnPrepareControls(IEntity owner, ActionManager am, float dt, bool player)
	{
		super.OnPrepareControls(owner, am, dt, player);

		if (!player || !am || SCR_Global.IsEditMode())
			return;

		// Only the locally controlled character.
		PlayerController localPlayer = GetGame().GetPlayerController();
		if (!localPlayer || localPlayer.GetControlledEntity() != owner)
			return;

		if (!m_MRADWindHUD)
			m_MRADWindHUD = new MRAD_WindHUD();

		m_MRADWindHUD.Update(ChimeraCharacter.Cast(owner));
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

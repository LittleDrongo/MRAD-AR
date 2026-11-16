// Hide the stock metre-based zeroing indicator only while a MRAD scope is active.
// No global HUD disabling; ordinary weapon displays are left alone.
modded class SCR_WeaponInfo
{
	protected bool m_bMRADStockZeroHidden;

	protected override void DisplayUpdate(IEntity owner, float timeSlice)
	{
		super.DisplayUpdate(owner, timeSlice);
		if (!m_Widgets || !m_Widgets.m_wZeroing)
			return;
		SCR_2DSightsComponent activeSights = GetSights();
		bool isMRAD = MRAD_ZeroingStateComponent.FindForSights(activeSights) != null;
		if (isMRAD)
		{
			m_Widgets.m_wZeroing.SetVisible(false);
			m_bMRADStockZeroHidden = true;
		}
		else if (m_bMRADStockZeroHidden)
		{
			m_Widgets.m_wZeroing.SetVisible(true);
			m_bMRADStockZeroHidden = false;
		}
	}
}

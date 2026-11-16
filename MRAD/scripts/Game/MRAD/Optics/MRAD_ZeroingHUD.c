// Bottom-right weapon panel: for MRAD scopes the vanilla zeroing row ("300 m")
// shows the dialled corrections instead, in the same widget, font and zeroing icon:
//	(up)    1.3 MRAD
//	(left)  0.4 MRAD
modded class SCR_WeaponInfo
{
	protected static const string MRAD_ARROW_IMAGESET = "{3262679C50EF4F01}UI/Textures/Icons/icons_wrapperUI.imageset";
	protected string m_sMRADShownText;

	//------------------------------------------------------------------------------------------------
	protected SCR_2DOpticsComponent MRAD_GetOptic()
	{
		if (!m_WeaponManager)
			return null;

		SCR_2DOpticsComponent optic = SCR_2DOpticsComponent.Cast(m_WeaponManager.GetCurrentSights());
		if (!optic || !optic.MRAD_IsSniperOptic())
			return null;

		return optic;
	}

	//------------------------------------------------------------------------------------------------
	protected static string MRAD_Arrow(string imageName)
	{
		return string.Format("<image set='%1' name='%2' scale='1.2'/>", MRAD_ARROW_IMAGESET, imageName);
	}

	//------------------------------------------------------------------------------------------------
	static string MRAD_FormatHUD(float elevationMrad, float windageMrad)
	{
		string up = "sortArrowUp";
		if (elevationMrad < 0)
			up = "sortArrowDown";

		string side = "sortArrowRight";
		if (windageMrad < 0)
			side = "sortArrowLeft";

		return MRAD_Arrow(up) + " " + Math.AbsFloat(elevationMrad).ToString(-1, 1) + " MRAD\n" + MRAD_Arrow(side) + " " + Math.AbsFloat(windageMrad).ToString(-1, 1) + " MRAD";
	}

	//------------------------------------------------------------------------------------------------
	//! \return false when the current sights are not an MRAD scope
	protected bool MRAD_UpdateIndicator(bool flash)
	{
		if (!m_Widgets || !m_Widgets.m_wZeroing || !m_Widgets.m_wZeroingText)
			return false;

		SCR_2DOpticsComponent optic = MRAD_GetOptic();
		if (!optic)
		{
			m_sMRADShownText = string.Empty;
			return false;
		}

		if (!MRAD_Settings.Get().GetShowHUD())
		{
			m_Widgets.m_wZeroing.SetVisible(false);
			m_Widgets.m_wZeroingText.SetText(string.Empty);
			m_sMRADShownText = string.Empty;
			return true;
		}

		string text = MRAD_FormatHUD(optic.MRAD_GetElevationMrad(), optic.MRAD_GetWindageMrad());
		if (text == m_sMRADShownText && m_Widgets.m_wZeroing.IsVisible())
			return true;

		m_Widgets.m_wZeroing.SetVisible(true);
		m_Widgets.m_wZeroingText.SetText(text);
		m_sMRADShownText = text;

		if (flash)
		{
			m_eWeaponStateEvent |= EWeaponFeature.ZEROING;
			AnimateWidget_ColorFlash(m_Widgets.m_wZeroing, EWeaponFeature.ZEROING);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	override void UpdateZeroingIndicator(SCR_WeaponState state)
	{
		if (!MRAD_UpdateIndicator(true))
			super.UpdateZeroingIndicator(state);
	}

	//------------------------------------------------------------------------------------------------
	override protected void DisplayUpdate(IEntity owner, float timeSlice)
	{
		bool wasMRAD = !m_sMRADShownText.IsEmpty();
		bool isMRAD = MRAD_UpdateIndicator(wasMRAD);

		// Scope removed or switched to a non-MRAD sight: give the row back to vanilla.
		if (wasMRAD && !isMRAD && m_WeaponState)
			super.UpdateZeroingIndicator(m_WeaponState);

		super.DisplayUpdate(owner, timeSlice);
	}
}

//------------------------------------------------------------------------------------------------
// PgUp/PgDn (vanilla range table) do nothing on an MRAD scope: no "Adjust zeroing" hint for it.
modded class SCR_WeaponSightHasZeroingCondition
{
	override bool IsAvailable(notnull SCR_AvailableActionsConditionData data)
	{
		BaseWeaponComponent weapon = data.GetCurrentWeapon();
		if (weapon)
		{
			SCR_2DOpticsComponent optic = SCR_2DOpticsComponent.Cast(weapon.GetSights());
			if (optic && optic.MRAD_IsSniperOptic())
				return GetReturnResult(false);
		}

		return super.IsAvailable(data);
	}
}

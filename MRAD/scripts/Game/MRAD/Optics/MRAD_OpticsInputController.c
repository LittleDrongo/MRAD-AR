// Turret input of one scope while it is in ADS: arrows (fine step), Shift+arrows (coarse step),
// holding a key repeats the step. Every accepted step clicks like the vanilla zeroing.
class MRAD_OpticsInputController
{
	protected const string INPUT_CONTEXT = "MRAD_OpticsContext";
	protected const string SOUND_PROJECT = "{D8F675F3468362E2}Sounds/Weapons/_SharedData/Weapons_Sights_Actions.acp";
	protected const float FIRST_REPEAT_DELAY = 0.32;
	protected const float REPEAT_INTERVAL = 0.13;

	protected static ref ScriptInvoker s_OnChanged;

	protected SCR_2DOpticsComponent m_Optic;
	protected bool m_bActive;
	protected bool m_bListening;
	protected int m_iHeldElevation;
	protected int m_iHeldWindage;
	protected float m_fElevationRepeat;
	protected float m_fWindageRepeat;

	//------------------------------------------------------------------------------------------------
	//! Invoked after any change of a scope's MRAD values (HUD refresh).
	static ScriptInvoker GetOnChanged()
	{
		if (!s_OnChanged)
			s_OnChanged = new ScriptInvoker();

		return s_OnChanged;
	}

	//------------------------------------------------------------------------------------------------
	void MRAD_OpticsInputController(SCR_2DOpticsComponent optic)
	{
		m_Optic = optic;
	}

	//------------------------------------------------------------------------------------------------
	void Activate()
	{
		m_bActive = true;
		m_iHeldElevation = 0;
		m_iHeldWindage = 0;
	}

	//------------------------------------------------------------------------------------------------
	void Deactivate()
	{
		m_bActive = false;
		SetListening(false);
	}

	//------------------------------------------------------------------------------------------------
	protected bool IsMenuOpen()
	{
		MenuManager menus = GetGame().GetMenuManager();
		return menus && menus.IsAnyMenuOpen();
	}

	//------------------------------------------------------------------------------------------------
	void Update(float timeSlice)
	{
		if (!m_bActive || !m_Optic || !m_Optic.MRAD_IsActive() || !m_Optic.MRAD_IsLocalPlayerSights())
		{
			SetListening(false);
			return;
		}

		bool menuOpen = IsMenuOpen();
		SetListening(!menuOpen);
		if (menuOpen)
			return;

		// The context lives for this frame only.
		GetGame().GetInputManager().ActivateContext(INPUT_CONTEXT);
		PollHeldActions(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	protected void SetListening(bool enable)
	{
		if (enable == m_bListening || !GetGame())
			return;

		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		if (enable)
		{
			input.AddActionListener("MRAD_ResetZeroing", EActionTrigger.DOWN, ResetZeroing);
		}
		else
		{
			input.RemoveActionListener("MRAD_ResetZeroing", EActionTrigger.DOWN, ResetZeroing);
			m_iHeldElevation = 0;
			m_iHeldWindage = 0;
			m_fElevationRepeat = 0;
			m_fWindageRepeat = 0;
		}

		m_bListening = enable;
	}

	//------------------------------------------------------------------------------------------------
	//! Action values stay non-zero for the whole hold. Coarse wins over fine on the same axis.
	protected void PollHeldActions(float timeSlice)
	{
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;

		int elevation = ReadAxis(input, "MRAD_ElevationUpCoarse", "MRAD_ElevationDownCoarse", "MRAD_ElevationUpFine", "MRAD_ElevationDownFine");
		int windage = ReadAxis(input, "MRAD_WindageRightCoarse", "MRAD_WindageLeftCoarse", "MRAD_WindageRightFine", "MRAD_WindageLeftFine");
		RepeatAxis(elevation, true, timeSlice);
		RepeatAxis(windage, false, timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! \return +-2 coarse, +-1 fine, 0 none
	protected int ReadAxis(InputManager input, string plusCoarse, string minusCoarse, string plusFine, string minusFine)
	{
		bool plus = input.GetActionValue(plusCoarse) > 0.5;
		bool minus = input.GetActionValue(minusCoarse) > 0.5;
		if (plus != minus)
		{
			if (plus)
				return 2;

			return -2;
		}

		if (plus || minus)
			return 0;

		plus = input.GetActionValue(plusFine) > 0.5;
		minus = input.GetActionValue(minusFine) > 0.5;
		if (plus == minus)
			return 0;

		if (plus)
			return 1;

		return -1;
	}

	//------------------------------------------------------------------------------------------------
	protected void RepeatAxis(int action, bool elevation, float timeSlice)
	{
		int previous = m_iHeldWindage;
		float remaining = m_fWindageRepeat;
		if (elevation)
		{
			previous = m_iHeldElevation;
			remaining = m_fElevationRepeat;
		}

		bool step;
		if (action != previous)
		{
			step = action != 0;
			remaining = FIRST_REPEAT_DELAY;
		}
		else if (action != 0)
		{
			remaining -= Math.Clamp(timeSlice, 0, 0.25);
			if (remaining <= 0)
			{
				step = true;
				remaining = REPEAT_INTERVAL;
			}
		}

		if (elevation)
		{
			m_iHeldElevation = action;
			m_fElevationRepeat = remaining;
		}
		else
		{
			m_iHeldWindage = action;
			m_fWindageRepeat = remaining;
		}

		if (step)
			Step(action, elevation);
	}

	//------------------------------------------------------------------------------------------------
	protected void Step(int action, bool elevation)
	{
		MRAD_Settings settings = MRAD_Settings.Get();
		float stepMrad = settings.GetFineStepMrad();
		if (Math.AbsInt(action) == 2)
			stepMrad = settings.GetCoarseStepMrad();

		int ticks = Math.Max(1, Math.Round(stepMrad * SCR_2DOpticsComponent.MRAD_TICKS_PER_MRAD));
		if (action < 0)
			ticks = -ticks;

		int elevationTicks = m_Optic.MRAD_GetElevationTicks();
		int windageTicks = m_Optic.MRAD_GetWindageTicks();
		int newElevation = elevationTicks;
		int newWindage = windageTicks;
		if (elevation)
		{
			int limit = Math.Round(settings.GetElevationLimitMrad() * SCR_2DOpticsComponent.MRAD_TICKS_PER_MRAD);
			newElevation = Math.ClampInt(elevationTicks + ticks, -limit, limit);
		}
		else
		{
			int limit = Math.Round(settings.GetWindageLimitMrad() * SCR_2DOpticsComponent.MRAD_TICKS_PER_MRAD);
			newWindage = Math.ClampInt(windageTicks + ticks, -limit, limit);
		}

		// At the turret stop: no change, no click.
		if (newElevation == elevationTicks && newWindage == windageTicks)
			return;

		m_Optic.MRAD_SetTicks(newElevation, newWindage);
		PlayClick(ticks > 0);
		GetOnChanged().Invoke(m_Optic);
	}

	//------------------------------------------------------------------------------------------------
	protected void ResetZeroing(float value, EActionTrigger trigger)
	{
		if (!m_bActive || !m_Optic || !m_Optic.MRAD_IsActive() || IsMenuOpen())
			return;

		if (m_Optic.MRAD_GetElevationTicks() == 0 && m_Optic.MRAD_GetWindageTicks() == 0)
			return;

		m_Optic.MRAD_SetTicks(0, 0);
		PlayClick(false);
		GetOnChanged().Invoke(m_Optic);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla zeroing click (SOUND_ZEROING_UP/DOWN of Weapons_Sights_Actions.acp), local only.
	protected void PlayClick(bool up)
	{
		string eventName = "SOUND_ZEROING_DOWN";
		if (up)
			eventName = "SOUND_ZEROING_UP";

		BaseWeaponComponent weapon = m_Optic.MRAD_FindWeapon();
		IEntity weaponEntity;
		if (weapon)
			weaponEntity = weapon.GetOwner();

		if (!weaponEntity)
			weaponEntity = m_Optic.GetOwner();

		WeaponSoundComponent weaponSound = WeaponSoundComponent.Cast(weaponEntity.FindComponent(WeaponSoundComponent));
		if (weaponSound && weaponSound.SoundEvent(eventName) != AudioHandle.Invalid)
			return;

		// Weapon without the vanilla sights sound project: play it directly.
		SCR_AudioSourceConfiguration config = new SCR_AudioSourceConfiguration();
		config.m_sSoundProject = SOUND_PROJECT;
		config.m_sSoundEventName = eventName;
		config.m_eFlags = 0;
		SCR_SoundManagerModule.CreateAndPlayAudioSource(weaponEntity, config);
	}
}

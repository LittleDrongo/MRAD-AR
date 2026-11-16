// Общий ввод/HUD для двух наследников оптики. Никакого общего состояния поправок.
class MRAD_OpticsInputController
{
	protected static ref MRAD_OpticsInputController s_Active;
	protected const ResourceName HUD_LAYOUT = "{A7DE1002B62C4731}UI/layouts/MRAD/MRAD_ZeroingHUD.layout";
	protected const string INPUT_CONTEXT = "MRAD_OpticsContext";

	protected BaseSightsComponent m_Sights;
	protected MRAD_ZeroingStateComponent m_State;
	protected bool m_bSessionActive;
	protected bool m_bListening;
	protected int m_iSettingsRevision = -1;
	protected int m_iSyncRequestId;
	protected int m_iSyncAttempts;
	protected float m_fSyncRetryTime;
	protected Widget m_HUD;
	protected TextWidget m_HUDElevation;
	protected TextWidget m_HUDWindage;
	protected TextWidget m_HUDStatus;
	protected const float FIRST_REPEAT_DELAY = 0.32;
	protected const float REPEAT_INTERVAL = 0.13;
	protected int m_iHeldElevation;
	protected int m_iHeldWindage;
	protected float m_fElevationRepeat;
	protected float m_fWindageRepeat;
	protected string m_sLastHUDText;

	void MRAD_OpticsInputController(BaseSightsComponent sights)
	{
		m_Sights = sights;
	}

	void Activate()
	{
		m_bSessionActive = true;
		m_State = MRAD_ZeroingStateComponent.FindForSights(m_Sights);
		m_iSettingsRevision = -1;
		m_iSyncRequestId = 0;
		Update(0);
	}

	void Deactivate()
	{
		m_bSessionActive = false;
		m_iSyncRequestId = 0;
		SetListening(false);
		DeleteHUD();
		if (s_Active == this)
			s_Active = null;
	}

	protected bool IsLocalADS()
	{
		if (!GetGame() || !m_Sights || !m_State || !m_Sights.IsSightADSActive())
			return false;

		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(player.GetControlledEntity());
		if (!character)
			return false;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (!controller || !controller.IsWeaponADS() || controller.IsDead() || controller.IsUnconscious())
			return false;

		BaseWeaponManagerComponent manager = controller.GetWeaponManagerComponent();
		return manager && manager.GetCurrentSights() == m_Sights;
	}

	protected bool IsMenuOpen()
	{
		return GetGame().GetMenuManager() && GetGame().GetMenuManager().IsAnyMenuOpen();
	}

	void Update(float timeSlice)
	{
		if (!m_bSessionActive)
			return;

		if (!IsLocalADS())
		{
			SetListening(false);
			DeleteHUD();
			return;
		}

		if (s_Active != this)
		{
			if (s_Active)
				s_Active.Deactivate();
			s_Active = this;
		}

		bool menuOpen = IsMenuOpen();
		SetListening(!menuOpen);
		if (menuOpen)
		{
			if (m_HUD)
				m_HUD.SetVisible(false);
			return;
		}

		// duration=0: контекст существует только в этом кадре.
		GetGame().GetInputManager().ActivateContext(INPUT_CONTEXT);
		PollHeldActions(timeSlice);
		int settingsRevision = MRAD_Settings.Get().GetRevision();
		if (settingsRevision != m_iSettingsRevision)
		{
			SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
			if (!player)
				return;
			m_iSyncRequestId = player.MRAD_BeginZeroingSync();
			m_iSyncAttempts = 0;
			m_fSyncRetryTime = 0;
			m_iSettingsRevision = settingsRevision;
		}
		UpdateSettingsSync(timeSlice);
		UpdateHUD();
	}

	protected void UpdateSettingsSync(float timeSlice)
	{
		if (m_iSyncRequestId <= 0)
			return;
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player)
			return;
		if (player.MRAD_HasZeroingSyncAck(m_iSyncRequestId))
		{
			m_iSyncRequestId = 0;
			return;
		}
		m_fSyncRetryTime -= timeSlice;
		if (m_fSyncRetryTime > 0)
			return;
		if (m_iSyncAttempts >= 8)
		{
			Print("MRAD: sight settings synchronization was not acknowledged; it will retry on the next ADS activation or settings change", LogLevel.WARNING);
			m_iSyncRequestId = 0;
			return;
		}
		// A newly streamed item or the server ADS transition may lag the local
		// callback. Retry only the idempotent zero-delta request, never an input step.
		SendRequest(0, 0, false, m_iSyncRequestId);
		m_iSyncAttempts++;
		m_fSyncRetryTime = 0.5;
	}

	protected void SetListening(bool enable)
	{
		if (enable == m_bListening || !GetGame())
			return;
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		if (enable)
			input.AddActionListener("MRAD_ResetZeroing", EActionTrigger.DOWN, ResetZeroing);
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

	// InputSourceValue (not Click/Down) stays nonzero for the entire hold.
	// Coarse always takes precedence over the same direction's fine binding.
	// This also works with user-remapped bindings; no physical keys are polled.
	protected void PollHeldActions(float timeSlice)
	{
		if (!m_bListening || !IsLocalADS() || IsMenuOpen())
			return;
		InputManager input = GetGame().GetInputManager();
		if (!input)
			return;
		int elevation;
		int windage;
		bool upCoarse = input.GetActionValue("MRAD_ElevationUpCoarse") > 0.5;
		bool downCoarse = input.GetActionValue("MRAD_ElevationDownCoarse") > 0.5;
		bool leftCoarse = input.GetActionValue("MRAD_WindageLeftCoarse") > 0.5;
		bool rightCoarse = input.GetActionValue("MRAD_WindageRightCoarse") > 0.5;
		if (upCoarse != downCoarse)
			if (upCoarse) elevation = 2; else elevation = -2;
		else if (!upCoarse && !downCoarse)
		{
			bool upFine = input.GetActionValue("MRAD_ElevationUpFine") > 0.5;
			bool downFine = input.GetActionValue("MRAD_ElevationDownFine") > 0.5;
			if (upFine != downFine)
				if (upFine) elevation = 1; else elevation = -1;
		}
		if (leftCoarse != rightCoarse)
			if (leftCoarse) windage = -2; else windage = 2;
		else if (!leftCoarse && !rightCoarse)
		{
			bool leftFine = input.GetActionValue("MRAD_WindageLeftFine") > 0.5;
			bool rightFine = input.GetActionValue("MRAD_WindageRightFine") > 0.5;
			if (leftFine != rightFine)
				if (leftFine) windage = -1; else windage = 1;
		}
		RepeatAxis(elevation, true, timeSlice);
		RepeatAxis(windage, false, timeSlice);
	}

	protected void RepeatAxis(int action, bool elevation, float timeSlice)
	{
		int previous = m_iHeldElevation;
		float remaining = m_fElevationRepeat;
		if (!elevation)
		{
			previous = m_iHeldWindage;
			remaining = m_fWindageRepeat;
		}
		bool shouldStep;
		if (action != previous)
		{
			shouldStep = action != 0;
			remaining = FIRST_REPEAT_DELAY;
		}
		else if (action != 0)
		{
			remaining -= Math.Clamp(timeSlice, 0, 0.25);
			if (remaining <= 0)
			{
				shouldStep = true;
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
		if (!shouldStep)
			return;
		if (elevation)
			if (action > 0) RequestStep(1, 0, Math.AbsInt(action) == 2); else RequestStep(-1, 0, Math.AbsInt(action) == 2);
		else
			if (action > 0) RequestStep(0, 1, Math.AbsInt(action) == 2); else RequestStep(0, -1, Math.AbsInt(action) == 2);
	}

	protected void RequestStep(int elevationDirection, int windageDirection, bool coarse)
	{
		if (!IsLocalADS() || IsMenuOpen() || !m_State)
			return;
		MRAD_Settings settings = MRAD_Settings.Get();
		float step = settings.GetFineStepMrad(); if (coarse) step = settings.GetCoarseStepMrad();
		int ticks = Math.ClampInt(MRAD_ZeroingStateComponent.ToTicks(step), 1, MRAD_ZeroingStateComponent.MAX_STEP_TICKS);
		int elevationDelta = elevationDirection * ticks;
		int windageDelta = windageDirection * ticks;
		// Do not play a click for a direction already at its adjustment stop.
		if (elevationDelta > 0 && m_State.GetElevationTicks() >= m_State.GetElevationLimitTicks())
			return;
		if (elevationDelta < 0 && m_State.GetElevationTicks() <= -m_State.GetElevationLimitTicks())
			return;
		if (windageDelta > 0 && m_State.GetWindageTicks() >= m_State.GetWindageLimitTicks())
			return;
		if (windageDelta < 0 && m_State.GetWindageTicks() <= -m_State.GetWindageLimitTicks())
			return;
		if (SendRequest(elevationDelta, windageDelta, false))
			SCR_UISoundEntity.SoundEvent(SCR_SoundEvent.SOUND_SIGHT_ELEVATION);
	}

	protected bool SendRequest(int elevationDelta, int windageDelta, bool reset, int syncRequestId = 0)
	{
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!player)
			return false;

		MRAD_Settings settings = MRAD_Settings.Get();
		int elevationLimit = Math.ClampInt(MRAD_ZeroingStateComponent.ToTicks(settings.GetElevationLimitMrad()), 1, MRAD_ZeroingStateComponent.MAX_LIMIT_TICKS);
		int windageLimit = Math.ClampInt(MRAD_ZeroingStateComponent.ToTicks(settings.GetWindageLimitMrad()), 1, MRAD_ZeroingStateComponent.MAX_LIMIT_TICKS);
		return player.MRAD_RequestZeroing(m_State, elevationDelta, windageDelta, elevationLimit, windageLimit, reset, syncRequestId);
	}

	protected void ResetZeroing(float value, EActionTrigger trigger)
	{
		if (IsLocalADS() && !IsMenuOpen())
			SendRequest(0, 0, true);
	}

	protected void UpdateHUD()
	{
		bool showValue = MRAD_Settings.Get().GetShowHUD();
		bool atLimit = Math.AbsInt(m_State.GetElevationTicks()) >= m_State.GetElevationLimitTicks() || Math.AbsInt(m_State.GetWindageTicks()) >= m_State.GetWindageLimitTicks();
		if (!showValue && !atLimit)
		{
			DeleteHUD();
			return;
		}

		if (!m_HUD)
		{
			if (!GetGame().GetWorkspace())
				return;
			m_HUD = GetGame().GetWorkspace().CreateWidgets(HUD_LAYOUT);
			if (!m_HUD)
				return;
			m_HUDElevation = TextWidget.Cast(m_HUD.FindAnyWidget("MRAD_ElevationValue"));
			m_HUDWindage = TextWidget.Cast(m_HUD.FindAnyWidget("MRAD_WindageValue"));
			m_HUDStatus = TextWidget.Cast(m_HUD.FindAnyWidget("MRAD_ZeroingStatus"));
			if (!m_HUDElevation || !m_HUDWindage || !m_HUDStatus)
			{
				DeleteHUD();
				return;
			}
			m_HUDStatus.SetText(WidgetManager.Translate("#MRAD_ZeroingLimit"));
		}

		m_HUD.SetVisible(true);
		m_HUDElevation.SetVisible(showValue);
		m_HUDWindage.SetVisible(showValue);
		m_HUDStatus.SetVisible(atLimit);
		string verticalArrow = "↑"; if (m_State.GetElevationTicks() < 0) verticalArrow = "↓";
		string horizontalArrow = "←"; if (m_State.GetWindageTicks() > 0) horizontalArrow = "→";
		string elevationText = verticalArrow + " " + Math.AbsFloat(m_State.GetElevationMrad()).ToString(1, 1) + " MRAD";
		string windageText = horizontalArrow + " " + Math.AbsFloat(m_State.GetWindageMrad()).ToString(1, 1) + " MRAD";
		string signature = elevationText + "|" + windageText;
		if (signature != m_sLastHUDText)
		{
			m_HUDElevation.SetText(elevationText);
			m_HUDWindage.SetText(windageText);
			m_sLastHUDText = signature;
		}
	}

	protected void DeleteHUD()
	{
		if (m_HUD)
			m_HUD.RemoveFromHierarchy();
		m_HUD = null;
		m_HUDElevation = null;
		m_HUDWindage = null;
		m_HUDStatus = null;
		m_sLastHUDText = string.Empty;
	}
}

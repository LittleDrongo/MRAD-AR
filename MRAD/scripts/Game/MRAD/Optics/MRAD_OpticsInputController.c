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
	protected TextWidget m_HUDValue;
	protected TextWidget m_HUDStatus;
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
		{
			input.AddActionListener("MRAD_ElevationUpFine", EActionTrigger.DOWN, ElevationUpFine);
			input.AddActionListener("MRAD_ElevationDownFine", EActionTrigger.DOWN, ElevationDownFine);
			input.AddActionListener("MRAD_WindageLeftFine", EActionTrigger.DOWN, WindageLeftFine);
			input.AddActionListener("MRAD_WindageRightFine", EActionTrigger.DOWN, WindageRightFine);
			input.AddActionListener("MRAD_ElevationUpCoarse", EActionTrigger.DOWN, ElevationUpCoarse);
			input.AddActionListener("MRAD_ElevationDownCoarse", EActionTrigger.DOWN, ElevationDownCoarse);
			input.AddActionListener("MRAD_WindageLeftCoarse", EActionTrigger.DOWN, WindageLeftCoarse);
			input.AddActionListener("MRAD_WindageRightCoarse", EActionTrigger.DOWN, WindageRightCoarse);
			input.AddActionListener("MRAD_ResetZeroing", EActionTrigger.DOWN, ResetZeroing);
		}
		else
		{
			input.RemoveActionListener("MRAD_ElevationUpFine", EActionTrigger.DOWN, ElevationUpFine);
			input.RemoveActionListener("MRAD_ElevationDownFine", EActionTrigger.DOWN, ElevationDownFine);
			input.RemoveActionListener("MRAD_WindageLeftFine", EActionTrigger.DOWN, WindageLeftFine);
			input.RemoveActionListener("MRAD_WindageRightFine", EActionTrigger.DOWN, WindageRightFine);
			input.RemoveActionListener("MRAD_ElevationUpCoarse", EActionTrigger.DOWN, ElevationUpCoarse);
			input.RemoveActionListener("MRAD_ElevationDownCoarse", EActionTrigger.DOWN, ElevationDownCoarse);
			input.RemoveActionListener("MRAD_WindageLeftCoarse", EActionTrigger.DOWN, WindageLeftCoarse);
			input.RemoveActionListener("MRAD_WindageRightCoarse", EActionTrigger.DOWN, WindageRightCoarse);
			input.RemoveActionListener("MRAD_ResetZeroing", EActionTrigger.DOWN, ResetZeroing);
		}

		m_bListening = enable;
	}

	protected void RequestStep(int elevationDirection, int windageDirection, bool coarse)
	{
		if (!IsLocalADS() || IsMenuOpen())
			return;

		MRAD_Settings settings = MRAD_Settings.Get();
		float step = settings.GetFineStepMrad();
		if (coarse)
			step = settings.GetCoarseStepMrad();
		int ticks = Math.ClampInt(MRAD_ZeroingStateComponent.ToTicks(step), 1, MRAD_ZeroingStateComponent.MAX_STEP_TICKS);
		SendRequest(elevationDirection * ticks, windageDirection * ticks, false);
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

	protected void ElevationUpFine(float value, EActionTrigger trigger) { RequestStep(1, 0, false); }
	protected void ElevationDownFine(float value, EActionTrigger trigger) { RequestStep(-1, 0, false); }
	protected void WindageLeftFine(float value, EActionTrigger trigger) { RequestStep(0, -1, false); }
	protected void WindageRightFine(float value, EActionTrigger trigger) { RequestStep(0, 1, false); }
	protected void ElevationUpCoarse(float value, EActionTrigger trigger) { RequestStep(1, 0, true); }
	protected void ElevationDownCoarse(float value, EActionTrigger trigger) { RequestStep(-1, 0, true); }
	protected void WindageLeftCoarse(float value, EActionTrigger trigger) { RequestStep(0, -1, true); }
	protected void WindageRightCoarse(float value, EActionTrigger trigger) { RequestStep(0, 1, true); }

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
			m_HUDValue = TextWidget.Cast(m_HUD.FindAnyWidget("MRAD_ZeroingValue"));
			m_HUDStatus = TextWidget.Cast(m_HUD.FindAnyWidget("MRAD_ZeroingStatus"));
			if (!m_HUDValue || !m_HUDStatus)
			{
				DeleteHUD();
				return;
			}
			m_HUDStatus.SetText(WidgetManager.Translate("#MRAD_ZeroingLimit"));
		}

		m_HUD.SetVisible(true);
		m_HUDValue.SetVisible(showValue);
		m_HUDStatus.SetVisible(atLimit);
		string text = string.Format(WidgetManager.Translate("#MRAD_ZeroingHUD"), m_State.GetElevationMrad().ToString(1, 1), m_State.GetWindageMrad().ToString(1, 1));
		if (text != m_sLastHUDText)
		{
			m_HUDValue.SetText(text);
			m_sLastHUDText = text;
		}
	}

	protected void DeleteHUD()
	{
		if (m_HUD)
			m_HUD.RemoveFromHierarchy();
		m_HUD = null;
		m_HUDValue = null;
		m_HUDStatus = null;
		m_sLastHUDText = string.Empty;
	}
}

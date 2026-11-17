// Hold breath for any sight (vanilla or MRAD), driven from the HUD manager phase.
// Timeline: 0.0-1.5 s stabilising, 1.5-3.5 s stable, 3.5-5.0 s smoothly back to the
// normal sway, exhale at 5 s. The sway is switched by the stamina system (public
// hold-breath lever); both transitions blend the system off/on with a duty cycle,
// so the aim settles in and comes back without a one-frame snap.
class MRAD_BreathController
{
	protected const float STABILIZE_TIME = 1.5;
	protected const float STABLE_END = 3.5;
	protected const float HOLD_MAX = 5.0;
	protected const float RELEASE_TIME = 0.5;
	protected const float PWM_PERIOD = 0.15;
	protected const float BREATH_MIN_STAMINA = 0.35;
	protected const int BREATH_COOLDOWN_MS = 3000;
	protected const ResourceName BREATH_INHALE = "{A64C816333C70CBF}Sounds/MRAD/mrad_inhale.wav";
	protected const ResourceName BREATH_EXHALE = "{A41C50F08C9E87E4}Sounds/MRAD/mrad_exhale.wav";

	protected static bool s_bHeld;
	protected static bool s_bActive;
	protected static bool s_bReleasing;
	protected static bool s_bSystemEnabled;
	protected static float s_fElapsed;
	protected static float s_fReleaseEnd;
	protected static float s_fPwmPhase;
	protected static float s_fReadyTime;
	protected static StaminaSystem s_StaminaSystem;

	//------------------------------------------------------------------------------------------------
	static void Update()
	{
		if (SCR_Global.IsEditMode() || System.IsConsoleApp())
			return;

		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
		{
			Finish();
			return;
		}

		MenuManager menus = GetGame().GetMenuManager();
		if (menus && menus.IsAnyMenuOpen())
		{
			Finish();
			return;
		}

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (!controller)
		{
			Finish();
			return;
		}

		BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
		BaseWeaponComponent weapon;
		if (manager)
			weapon = manager.GetCurrentWeapon();

		bool aiming = weapon && weapon.IsSightADSActive();
		InputManager input = GetGame().GetInputManager();
		bool held = aiming && input && input.GetActionValue("MRAD_HoldBreath") > 0.5;

		if (!s_bActive)
		{
			if (held && !s_bHeld)
				Start(character, controller);

			s_bHeld = held;
			return;
		}

		float dt = 0.016;
		if (input)
			dt = input.GetLastTimeDelta();
		dt = Math.Clamp(dt, 0, 0.25);

		s_fElapsed += dt;

		if (s_bReleasing)
		{
			if (s_fElapsed >= s_fReleaseEnd)
				Finish();

			return;
		}

		s_fPwmPhase += dt;
		if (s_fPwmPhase >= PWM_PERIOD)
			s_fPwmPhase -= PWM_PERIOD;
		float phase = s_fPwmPhase / PWM_PERIOD;

		if (s_fElapsed < STABILIZE_TIME)
		{
			// The share of the frame where the stamina system is off grows 0 -> 1.
			float ramp = Math.Clamp(s_fElapsed / STABILIZE_TIME, 0, 1);
			SetStaminaEnabled(phase >= ramp);
		}
		else if (s_fElapsed < STABLE_END)
		{
			SetStaminaEnabled(false);
		}
		else if (s_fElapsed < HOLD_MAX)
		{
			// The share where it is off shrinks 1 -> 0: the sway comes back gradually.
			float ramp = Math.Clamp((s_fElapsed - STABLE_END) / (HOLD_MAX - STABLE_END), 0, 1);
			SetStaminaEnabled(phase >= 1.0 - ramp);
		}
		else
		{
			SetStaminaEnabled(true);
		}

		// Five seconds is the limit: forced exhale.
		if (s_fElapsed >= HOLD_MAX)
		{
			Release();
			return;
		}

		if (!held)
			Release();
	}

	//------------------------------------------------------------------------------------------------
	protected static void Start(ChimeraCharacter character, SCR_CharacterControllerComponent controller)
	{
		if (BreathNow() < s_fReadyTime)
			return;

		CharacterStaminaComponent stamina = controller.GetStaminaComponent();
		if (!stamina)
			return;

		float current = stamina.GetStamina();
		float normalized = current;
		if (current > 1.5)
			normalized = current * 0.01;

		// Low stamina (after a sprint): no hold, the character has to rest first.
		if (normalized < BREATH_MIN_STAMINA)
			return;

		s_bActive = true;
		s_bReleasing = false;
		s_fElapsed = 0;
		s_fPwmPhase = 0;

		// The system starts enabled; the onset blends it off.
		if (!s_StaminaSystem)
			s_StaminaSystem = StaminaSystem.Cast(GetGame().GetWorld().FindSystem(StaminaSystem));

		if (s_StaminaSystem)
		{
			s_StaminaSystem.Enable(true);
			s_bSystemEnabled = true;
		}

		AudioSystem.PlaySound(BREATH_INHALE);
	}

	//------------------------------------------------------------------------------------------------
	//! The stamina system is world-wide. A pure client may switch its own local copy;
	//! a listen server must not, or every character simulated by the host loses stamina.
	protected static bool CanSwitchStaminaSystem()
	{
		if (!Replication.IsServer())
			return true;

		PlayerManager players = GetGame().GetPlayerManager();
		return !players || players.GetPlayerCount() <= 1;
	}

	//------------------------------------------------------------------------------------------------
	protected static void SetStaminaEnabled(bool enabled)
	{
		if (!s_StaminaSystem)
			s_StaminaSystem = StaminaSystem.Cast(GetGame().GetWorld().FindSystem(StaminaSystem));

		if (!s_StaminaSystem)
			return;

		if (!CanSwitchStaminaSystem())
		{
			if (!s_bSystemEnabled)
			{
				s_StaminaSystem.Enable(true);
				s_bSystemEnabled = true;
			}

			return;
		}

		if (s_bSystemEnabled == enabled)
			return;

		s_bSystemEnabled = enabled;
		s_StaminaSystem.Enable(enabled);
	}

	//------------------------------------------------------------------------------------------------
	//! End of the hold: exhale, put the stamina system back, run the short release tail.
	protected static void Release()
	{
		if (s_bReleasing)
			return;

		s_bReleasing = true;
		s_fReleaseEnd = s_fElapsed + RELEASE_TIME;
		SetStaminaEnabled(true);
		AudioSystem.PlaySound(BREATH_EXHALE);
	}

	//------------------------------------------------------------------------------------------------
	protected static void Finish()
	{
		if (!s_bActive)
		{
			s_bHeld = false;
			return;
		}

		SetStaminaEnabled(true);

		s_bActive = false;
		s_bReleasing = false;
		s_fElapsed = 0;
		s_fPwmPhase = 0;
		s_fReadyTime = BreathNow() + BREATH_COOLDOWN_MS;
	}

	//------------------------------------------------------------------------------------------------
	protected static float BreathNow()
	{
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}
}

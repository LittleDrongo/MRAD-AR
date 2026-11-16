// Opt-in QA only: a real owned client, scripted native ADS state entry, and the
// product PlayerController RPC. No protected product callback or projectile
// direction is replaced. This does not claim physical-key or impact coverage.
class MRAD_NetworkADSCheck
{
	protected static ref MRAD_NetworkADSCheck s_Instance;
	protected MRAD_NetworkProbe m_Probe;
	protected SCR_PlayerController m_Player;
	protected ChimeraCharacter m_Character;
	protected MRAD_ZeroingStateComponent m_OwnScope;
	protected MRAD_ZeroingStateComponent m_ForeignScope;
	protected ref array<IEntity> m_aSpawned = {};
	protected RplId m_CharacterId;
	protected RplId m_OwnScopeId;
	protected RplId m_ForeignScopeId;
	protected int m_iFineTicks = 1;
	protected int m_iStage;
	protected int m_iTicks;
	protected int m_iStageTicks;
	protected int m_iStagesPassed;
	protected int m_iFailures;
	protected int m_iActionDispatches;
	protected int m_iPulseFrames;
	protected int m_iInputFrames;
	protected bool m_bClient;
	protected bool m_bFinished;
	protected bool m_bMutationSent;
	protected bool m_bServerPassed;
	protected bool m_bPulseComplete;
	protected bool m_bListenerAdded;
	protected bool m_bMenusClosed;
	protected bool m_bADSStateInitialized;
	protected bool m_bAppliedADS;

	static MRAD_NetworkADSCheck Get() { return s_Instance; }

	static void StartClient(MRAD_NetworkProbe probe)
	{
		s_Instance = new MRAD_NetworkADSCheck();
		s_Instance.m_bClient = true;
		s_Instance.m_Probe = probe;
		s_Instance.m_iFineTicks = Math.ClampInt(MRAD_ZeroingStateComponent.ToTicks(MRAD_Settings.Get().GetFineStepMrad()), 1, 100);
		Print("MRAD_ADS_CLIENT_BEGIN deadlineSeconds=35 fixture=M21_PIP entry=scriptedNativeState");
		GetGame().GetCallqueue().CallLater(s_Instance.Tick, 100, true);
	}

	static void RequestAuthorityFixture(SCR_PlayerController player, int fineTicks)
	{
		if (!Replication.IsServer() || !player || !System.IsCLIParam("mradNetworkADS"))
			return;
		if (!s_Instance)
		{
			s_Instance = new MRAD_NetworkADSCheck();
			s_Instance.m_Player = player;
			s_Instance.m_iFineTicks = Math.ClampInt(fineTicks, 1, 100);
			s_Instance.CreateAuthorityFixture();
		}
		if (s_Instance.m_Player == player && s_Instance.m_Character && s_Instance.m_OwnScope && s_Instance.m_ForeignScope)
			player.MRAD_QA_SendADSFixture(Replication.FindItemId(s_Instance.m_Character), Replication.FindItemId(s_Instance.m_OwnScope), Replication.FindItemId(s_Instance.m_ForeignScope));
	}

	protected IEntity Spawn(ResourceName name, vector position)
	{
		Resource resource = Resource.Load(name);
		if (!resource || !resource.IsValid())
			return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = position;
		IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (entity)
			m_aSpawned.Insert(entity);
		return entity;
	}

	protected void CreateAuthorityFixture()
	{
		m_Character = ChimeraCharacter.Cast(Spawn("{37578B1666981FCE}Prefabs/Characters/Core/Character_Base.et", "20 3 20"));
		IEntity weapon = Spawn("{A7DE1002B62C4804}Prefabs/Weapons/MRAD/Rifles/MRAD_Rifle_M21_PIP.et", "20 3 21");
		IEntity foreign = Spawn("{A7DE1002B62C4802}Prefabs/Weapons/MRAD/Optics/MRAD_Optic_ARTII_PIP.et", "22 3 20");
		if (!m_Character || !weapon || !foreign)
		{
			Print("MRAD_ADS_AUTHORITY_FAIL fixture spawn failed", LogLevel.ERROR);
			return;
		}
		m_ForeignScope = MRAD_ZeroingStateComponent.Cast(foreign.FindComponent(MRAD_ZeroingStateComponent));
		CharacterWeaponManagerComponent manager = CharacterWeaponManagerComponent.Cast(m_Character.FindComponent(CharacterWeaponManagerComponent));
		array<WeaponSlotComponent> slots = {};
		if (manager)
			manager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			if (slot && slot.GetWeaponSlotIndex() == 0)
			{
				manager.SetSlotWeapon(slot, weapon);
				manager.SelectWeapon(slot);
				break;
			}
		}
		m_OwnScope = FindScopeInHierarchy(weapon);
		// Native existing-entity possession also transfers the Rpl hierarchy,
		// records the main entity, and deactivates AI. A bare SetControlledEntity
		// leaves the character server-owned and cannot test client controls.
		m_Player.SetInitialMainEntity(m_Character);
		bool possessed = m_Player.GetControlledEntity() == m_Character;
		RplComponent.EnableStreamingForConnection(m_Player.GetRplIdentity(), true);
		PrintFormat("MRAD_ADS_AUTHORITY_FIXTURE possessed=%1 ownScope=%2 foreignScope=%3 player=%4", possessed, m_OwnScope != null, m_ForeignScope != null, m_Player.GetPlayerId());
	}

	protected MRAD_ZeroingStateComponent FindScopeInHierarchy(IEntity entity)
	{
		if (!entity)
			return null;
		MRAD_ZeroingStateComponent state = MRAD_ZeroingStateComponent.Cast(entity.FindComponent(MRAD_ZeroingStateComponent));
		if (state)
			return state;
		IEntity child = entity.GetChildren();
		while (child)
		{
			state = FindScopeInHierarchy(child);
			if (state)
				return state;
			child = child.GetSibling();
		}
		return null;
	}

	void ReceiveFixture(RplId characterId, RplId ownScopeId, RplId foreignScopeId)
	{
		if (!m_bClient || m_iStage != 0)
			return;
		m_CharacterId = characterId;
		m_OwnScopeId = ownScopeId;
		m_ForeignScopeId = foreignScopeId;
		m_iStage = 1;
	}

	protected bool ResolveClientFixture()
	{
		m_Character = ChimeraCharacter.Cast(Replication.FindItem(m_CharacterId));
		m_OwnScope = MRAD_ZeroingStateComponent.Cast(Replication.FindItem(m_OwnScopeId));
		m_ForeignScope = MRAD_ZeroingStateComponent.Cast(Replication.FindItem(m_ForeignScopeId));
		RplComponent rpl;
		if (m_Character)
			rpl = RplComponent.Cast(m_Character.FindComponent(RplComponent));
		return m_Player && m_Character && m_OwnScope && m_ForeignScope && m_Player.GetControlledEntity() == m_Character && rpl && rpl.IsOwner();
	}

	protected bool WantADS()
	{
		return (m_iStage >= 3 && m_iStage <= 10) || m_iStage >= 13;
	}

	// Called after native SCR_PlayerController.OnUpdate, only for our isolated
	// owned local controller. Its actual character/controller consumes the input.
	void ClientFrame(SCR_PlayerController player, float timeSlice)
	{
		if (!m_bClient || m_bFinished || player != GetGame().GetPlayerController() || !m_Character || player.GetControlledEntity() != m_Character)
			return;
		RplComponent rpl = RplComponent.Cast(m_Character.FindComponent(RplComponent));
		if (!rpl || !rpl.IsOwner())
			return;
		CharacterControllerComponent controller = m_Character.GetCharacterController();
		if (!controller)
			return;
		CharacterInputContext input = controller.GetInputContext();
		if (!input)
			return;
		m_iInputFrames++;
		input.SetRaiseWeapon(true);
		input.SetWeaponADS(WantADS());
		input.SetWeaponPullTrigger(false);
		controller.SetWeaponADSInput(WantADS());
		controller.SetWeaponRaised(true);
		if (!m_bADSStateInitialized || m_bAppliedADS != WantADS())
		{
			// This is fixture setup through a documented native setter, not an
			// assertion about physical ADS keys. Later stages still require the
			// actual sight camera and the independently observed server ADS state.
			controller.SetWeaponADS(WantADS());
			m_bAppliedADS = WantADS();
			m_bADSStateInitialized = true;
			PrintFormat("MRAD_ADS_SCRIPTED_ENTRY requested=%1 nativeState=%2", WantADS(), controller.IsWeaponADS());
		}
		if (m_iStage == 10 && m_bListenerAdded && !m_bPulseComplete)
		{
			InputManager actions = GetGame().GetInputManager();
			actions.ActivateAction("MRAD_ElevationUpFine");
			float value;
			if (m_iPulseFrames == 1 || m_iPulseFrames == 2)
				value = 1;
			actions.SetActionValue("MRAD_ElevationUpFine", value);
			m_iPulseFrames++;
			if (m_iPulseFrames >= 5)
				m_bPulseComplete = true;
		}
	}

	protected vector ExpectedTicks(int stage)
	{
		if (stage == 5)
			return "10 0 0";
		if (stage == 6)
			return "10 -7 0";
		if (stage == 7 || stage == 8)
			return "20 -7 0";
		if (stage >= 10 && stage <= 13)
			return Vector(m_iFineTicks, 0, 0);
		return vector.Zero;
	}

	protected bool MatchesExpected(int stage)
	{
		vector expected = ExpectedTicks(stage);
		return m_OwnScope && m_ForeignScope && m_OwnScope.GetElevationTicks() == expected[0] && m_OwnScope.GetWindageTicks() == expected[1] && m_ForeignScope.GetElevationTicks() == 0 && m_ForeignScope.GetWindageTicks() == 0;
	}

	protected void Tick()
	{
		if (!m_bClient || m_bFinished)
			return;
		m_iTicks++;
		m_iStageTicks++;
		if (m_iTicks > 350)
		{
			Fail("bounded owned-client ADS check expired at stage " + m_iStage);
			return;
		}
		m_Player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (!m_Player)
			return;
		if (m_iStage == 0)
		{
			if (m_iTicks % 10 == 0)
				m_Player.MRAD_QA_RequestADSFixture(m_iFineTicks);
			return;
		}
		if (!ResolveClientFixture())
		{
			if (m_iTicks % 20 == 0)
				PrintFormat("MRAD_ADS_CLIENT_WAIT stage=%1 character=%2 ownScope=%3 foreignScope=%4 controlled=%5", m_iStage, m_Character != null, m_OwnScope != null, m_ForeignScope != null, m_Character && m_Player.GetControlledEntity() == m_Character);
			return;
		}
		if (!m_bMenusClosed)
		{
			GetGame().GetMenuManager().CloseAllMenus();
			m_Character.GetCharacterController().SetDisableWeaponControls(false);
			m_bMenusClosed = true;
		}
		CharacterControllerComponent controller = m_Character.GetCharacterController();
		BaseWeaponManagerComponent manager = controller.GetWeaponManagerComponent();
		BaseSightsComponent sights;
		if (manager)
			sights = manager.GetCurrentSights();
		bool localADS = controller.IsWeaponADS() && sights && sights.IsSightADSActive() && MRAD_ZeroingStateComponent.FindForSights(sights) == m_OwnScope;
		if (m_iTicks % 20 == 0)
		{
			PrintFormat("MRAD_ADS_CLIENT_DIAG stage=%1 ads=%2 nativeADS=%3 sights=%4 own=%5 elevation=%6 windage=%7 actionDispatches=%8", m_iStage, localADS, controller.IsWeaponADS(), sights != null, MRAD_ZeroingStateComponent.FindForSights(sights) == m_OwnScope, m_OwnScope.GetElevationTicks(), m_OwnScope.GetWindageTicks(), m_iActionDispatches);
			CharacterInputContext context = controller.GetInputContext();
			PrintFormat("MRAD_ADS_CLIENT_INPUT frames=%1 owner=1 raised=%2 wantedADS=%3 nativeADSInput=%4 contextADS=%5 menuOpen=%6 controlsDisabled=%7", m_iInputFrames, controller.IsWeaponRaised(), WantADS(), controller.GetWeaponADSInput(), context && context.IsWeaponADS(), GetGame().GetMenuManager().IsAnyMenuOpen(), controller.GetDisableWeaponControls());
		}
		if ((WantADS() && !localADS) || (!WantADS() && controller.IsWeaponADS()))
			return;
		if (m_bServerPassed && MatchesExpected(m_iStage))
		{
			if (m_iStage == 10 && m_iActionDispatches != 1)
			{
				if (m_iActionDispatches > 1)
					Fail("action pulse caused more than one DOWN dispatch");
				return;
			}
			PrintFormat("MRAD_ADS_CLIENT_PASS stage=%1 serverVerified=1 replicatedStateMatches=1 ads=%2", m_iStage, localADS);
			m_iStagesPassed++;
			m_iStage++;
			m_iStageTicks = 0;
			m_bMutationSent = false;
			m_bServerPassed = false;
			if (m_iStage > 14)
				FinishClient();
			return;
		}
		if (!m_bMutationSent)
		{
			SendStageMutation();
			m_bMutationSent = true;
		}
		if (m_iStage == 10 && !m_bPulseComplete)
			return;
		if (m_iStageTicks % 5 == 0)
			m_Player.MRAD_QA_CheckADSStage(m_iStage);
	}

	protected void SendStageMutation()
	{
		if (m_iStage == 2 || m_iStage == 12)
			m_Player.MRAD_RequestZeroing(m_OwnScope, 5, 0, 500, 500);
		else if (m_iStage == 4)
			m_Player.MRAD_RequestZeroing(m_ForeignScope, 5, 0, 500, 500);
		else if (m_iStage == 5)
			m_Player.MRAD_RequestZeroing(m_OwnScope, 10, 0, 500, 500, false, m_Player.MRAD_BeginZeroingSync());
		else if (m_iStage == 6)
			m_Player.MRAD_RequestZeroing(m_OwnScope, 0, -7, 500, 500);
		else if (m_iStage == 7)
			m_Player.MRAD_RequestZeroing(m_OwnScope, 100, 0, 20, 20);
		else if (m_iStage == 8)
		{
			m_Player.MRAD_RequestZeroing(m_OwnScope, 1, 1, 20, 20);
			m_Player.MRAD_RequestZeroing(m_OwnScope, 101, 0, 20, 20);
			m_Player.MRAD_RequestZeroing(m_OwnScope, 1, 0, 0, 20);
		}
		else if (m_iStage == 9 || m_iStage == 14)
			m_Player.MRAD_RequestZeroing(m_OwnScope, 0, 0, 500, 500, true);
		else if (m_iStage == 10)
		{
			InputManager input = GetGame().GetInputManager();
			input.ResetAction("MRAD_ElevationUpFine");
			input.AddActionListener("MRAD_ElevationUpFine", EActionTrigger.DOWN, OnActionDown);
			m_bListenerAdded = true;
		}
	}

	protected void OnActionDown(float value, EActionTrigger trigger)
	{
		m_iActionDispatches++;
		PrintFormat("MRAD_ADS_ACTION_DISPATCH count=%1 value=%2", m_iActionDispatches, value);
	}

	void InspectAuthority(SCR_PlayerController player, int stage)
	{
		if (m_bClient || player != m_Player || stage < 1 || stage > 14 || !m_Character || !m_OwnScope || !m_ForeignScope)
			return;
		CharacterControllerComponent controller = m_Character.GetCharacterController();
		BaseWeaponManagerComponent manager = controller.GetWeaponManagerComponent();
		bool wantsADS = (stage >= 3 && stage <= 10) || stage >= 13;
		bool ads = controller.IsWeaponADS();
		bool current = manager && MRAD_ZeroingStateComponent.FindForSights(manager.GetCurrentSights()) == m_OwnScope;
		bool readyStage = stage == 1 || stage == 3 || stage == 11 || stage == 13;
		if (readyStage && (ads != wantsADS || (wantsADS && !current)))
		{
			string transition = string.Format("native ADS transition ads=%1 wants=%2 current=%3 nativeADSInput=%4", ads, wantsADS, current, controller.GetWeaponADSInput());
			PrintFormat("MRAD_ADS_AUTHORITY_WAIT stage=%1 %2", stage, transition);
			player.MRAD_QA_SendADSStage(stage, false, true, transition);
			return;
		}
		bool passed = player.GetControlledEntity() == m_Character && !controller.IsDead() && !controller.IsUnconscious() && ads == wantsADS && (!wantsADS || current) && MatchesExpected(stage);
		if (stage == 7 || stage == 8)
			passed = passed && m_OwnScope.GetElevationLimitTicks() == 20 && m_OwnScope.GetWindageLimitTicks() == 20;
		// An engine action is dispatched on a later input frame; the check may
		// precede its generated production RPC. Keep polling a bounded interval.
		bool waiting = stage == 10 && !passed && m_OwnScope.GetElevationTicks() == 0 && m_OwnScope.GetWindageTicks() == 0;
		string detail = string.Format("ads=%1 current=%2 elevation=%3 windage=%4 foreignElevation=%5 foreignWindage=%6", ads, current, m_OwnScope.GetElevationTicks(), m_OwnScope.GetWindageTicks(), m_ForeignScope.GetElevationTicks(), m_ForeignScope.GetWindageTicks());
		if (passed)
			PrintFormat("MRAD_ADS_AUTHORITY_PASS stage=%1 %2", stage, detail);
		else if (!waiting)
			PrintFormat("MRAD_ADS_AUTHORITY_FAIL stage=%1 %2", stage, detail);
		if (passed && stage == 14)
			Print("MRAD_ADS_AUTHORITY_RESULT stages=14 failures=0");
		player.MRAD_QA_SendADSStage(stage, passed, waiting, detail);
	}

	void ReceiveStage(int stage, bool passed, bool waiting, string detail)
	{
		if (!m_bClient || m_bFinished || stage != m_iStage)
			return;
		if (waiting)
			return;
		if (!passed)
		{
			Fail(string.Format("authority rejected stage=%1 %2", stage, detail));
			return;
		}
		m_bServerPassed = true;
	}

	protected void Fail(string reason)
	{
		m_iFailures++;
		Print("MRAD_ADS_CLIENT_FAIL " + reason, LogLevel.ERROR);
		FinishClient();
	}

	protected void FinishClient()
	{
		if (m_bFinished)
			return;
		m_bFinished = true;
		GetGame().GetCallqueue().Remove(Tick);
		if (m_bListenerAdded)
		{
			GetGame().GetInputManager().RemoveActionListener("MRAD_ElevationUpFine", EActionTrigger.DOWN, OnActionDown);
			GetGame().GetInputManager().SetActionValue("MRAD_ElevationUpFine", 0);
		}
		if (m_Character && m_Character.GetCharacterController())
		{
			m_Character.GetCharacterController().SetWeaponADSInput(false);
			m_Character.GetCharacterController().SetWeaponADS(false);
		}
		PrintFormat("MRAD_ADS_CLIENT_RESULT stages=%1 actionDispatches=%2 failures=%3", m_iStagesPassed, m_iActionDispatches, m_iFailures);
		if (m_Probe)
			m_Probe.OnADSCheckFinished(m_iFailures);
	}

	static void CleanupAuthority()
	{
		if (!s_Instance || s_Instance.m_bClient)
			return;
		foreach (IEntity entity : s_Instance.m_aSpawned)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		s_Instance.m_aSpawned.Clear();
		s_Instance = null;
	}
}

// All extra transport and input injection below is confined to the QA addon.
modded class SCR_PlayerController : PlayerController
{
	override void OnUpdate(float timeSlice)
	{
		super.OnUpdate(timeSlice);
		MRAD_NetworkADSCheck check = MRAD_NetworkADSCheck.Get();
		if (check)
			check.ClientFrame(this, timeSlice);
	}

	void MRAD_QA_RequestADSFixture(int fineTicks) { Rpc(RpcAsk_MRAD_QA_ADSFixture, fineTicks); }
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MRAD_QA_ADSFixture(int fineTicks) { MRAD_NetworkADSCheck.RequestAuthorityFixture(this, fineTicks); }

	void MRAD_QA_SendADSFixture(RplId character, RplId ownScope, RplId foreignScope) { Rpc(RpcDo_MRAD_QA_ADSFixture, character, ownScope, foreignScope); }
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_MRAD_QA_ADSFixture(RplId character, RplId ownScope, RplId foreignScope)
	{
		if (MRAD_NetworkADSCheck.Get())
			MRAD_NetworkADSCheck.Get().ReceiveFixture(character, ownScope, foreignScope);
	}

	void MRAD_QA_CheckADSStage(int stage) { Rpc(RpcAsk_MRAD_QA_ADSStage, stage); }
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MRAD_QA_ADSStage(int stage)
	{
		if (MRAD_NetworkADSCheck.Get())
			MRAD_NetworkADSCheck.Get().InspectAuthority(this, stage);
	}

	void MRAD_QA_SendADSStage(int stage, bool passed, bool waiting, string detail) { Rpc(RpcDo_MRAD_QA_ADSStage, stage, passed, waiting, detail); }
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_MRAD_QA_ADSStage(int stage, bool passed, bool waiting, string detail)
	{
		if (MRAD_NetworkADSCheck.Get())
			MRAD_NetworkADSCheck.Get().ReceiveStage(stage, passed, waiting, detail);
	}
}

// QA-only, real network replication test. No local mirror stands in for a peer.
class MRAD_NetworkProbeClass : GenericEntityClass
{
}

class MRAD_NetworkProbe : GenericEntity
{
	protected static MRAD_NetworkProbe s_Instance;

	[RplProp()]
	protected RplId m_NotebookId;
	[RplProp()]
	protected RplId m_ScopeId;
	[RplProp()]
	protected int m_iPhase;
	[RplProp()]
	protected int m_iExpectedPage;
	[RplProp()]
	protected int m_iExpectedRows;
	[RplProp()]
	protected int m_iExpectedElevation;
	[RplProp()]
	protected int m_iExpectedWindage;
	[RplProp()]
	protected string m_sExpectedRows;

	protected ref array<IEntity> m_aSpawned = {};
	protected ref array<RplIdentity> m_aObservers = {};
	protected ChimeraCharacter m_Character;
	protected Drg_AutoNotepadRtComponent m_Notebook;
	protected MRAD_NotepadGadgetComponent m_Gadget;
	protected MRAD_ZeroingStateComponent m_Scope;
	protected int m_iFailures;
	protected int m_iFirstPhase;
	protected int m_iLastPassedPhase;
	protected int m_iPassedSnapshots;
	protected int m_iFinalAcknowledgements;
	protected bool m_bSecondPhaseScheduled;
	protected bool m_bClosing;
	protected string m_sLastMismatch;
	protected int m_iReplicaPolls;
	protected int m_iStreamingRequests;
	protected bool m_bADSCheckStarted;

	static MRAD_NetworkProbe GetInstance() { return s_Instance; }

	void MRAD_NetworkProbe(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	override void EOnInit(IEntity owner)
	{
		if (SCR_Global.IsEditMode())
			return;
		s_Instance = this;
		if (Replication.IsServer())
		{
			Print("MRAD_NETWORK_BEGIN role=server deadlineSeconds=90");
			GetGame().GetCallqueue().CallLater(PrepareAuthority, 1000, false);
		}
		else
		{
			Print("MRAD_NETWORK_BEGIN role=client deadlineSeconds=55");
			GetGame().GetCallqueue().CallLater(PollReplica, 250, true);
			GetGame().GetCallqueue().CallLater(Timeout, 55000, false);
			return;
		}
		GetGame().GetCallqueue().CallLater(Timeout, 90000, false);
	}

	protected void Fail(string reason)
	{
		m_iFailures++;
		Print("MRAD_NETWORK_FAIL " + reason, LogLevel.ERROR);
	}

	protected IEntity SpawnFixture(ResourceName name, int offset)
	{
		Resource resource = Resource.Load(name);
		if (!resource || !resource.IsValid())
			return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = Vector(offset * 2, 4, 0);
		IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		if (entity)
		{
			m_aSpawned.Insert(entity);
			RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
			if (rpl)
			{
				rpl.EnableSpatialRelevancy(true);
				rpl.EnableStreaming(true);
			}
		}
		return entity;
	}

	protected void PrepareAuthority()
	{
		RplComponent ownRpl = RplComponent.Cast(FindComponent(RplComponent));
		if (ownRpl)
		{
			ownRpl.EnableSpatialRelevancy(true);
			ownRpl.EnableStreaming(true);
		}
		m_Character = ChimeraCharacter.Cast(SpawnFixture("{37578B1666981FCE}Prefabs/Characters/Core/Character_Base.et", 0));
		IEntity weapon = SpawnFixture("{B31929F65F0D0279}Prefabs/Weapons/Rifles/M14/Rifle_M21.et", 1);
		IEntity notebook = SpawnFixture("{2A7C01B93CC92ACE}Prefabs/Items/Equipment/RangeCard/RangeCard_Notepad_AUTO.et", 2);
		IEntity scope = SpawnFixture("{A7DE1002B62C4802}Prefabs/Weapons/MRAD/Optics/MRAD_Optic_ARTII_PIP.et", 3);
		if (!m_Character || !weapon || !notebook || !scope)
		{
			Fail("authority fixture spawn failed");
			FinishServer();
			return;
		}
		m_Notebook = Drg_AutoNotepadRtComponent.Cast(notebook.FindComponent(Drg_AutoNotepadRtComponent));
		m_Gadget = MRAD_NotepadGadgetComponent.Cast(notebook.FindComponent(MRAD_NotepadGadgetComponent));
		m_Scope = MRAD_ZeroingStateComponent.Cast(scope.FindComponent(MRAD_ZeroingStateComponent));
		CharacterWeaponManagerComponent manager = CharacterWeaponManagerComponent.Cast(m_Character.FindComponent(CharacterWeaponManagerComponent));
		if (!m_Notebook || !m_Gadget || !m_Scope || !manager)
		{
			Fail("authority fixture components unavailable");
			FinishServer();
			return;
		}
		array<WeaponSlotComponent> slots = {};
		manager.GetWeaponsSlots(slots);
		WeaponSlotComponent primary;
		foreach (WeaponSlotComponent slot : slots)
		{
			if (slot && slot.GetWeaponSlotIndex() == 0)
				primary = slot;
		}
		if (!primary)
		{
			Fail("authority fixture primary slot unavailable");
			FinishServer();
			return;
		}
		manager.SetSlotWeapon(primary, weapon);
		manager.SelectWeapon(primary);
		GetGame().GetCallqueue().CallLater(PublishFirst, 3000, false);
	}

	static string DataSignature(MRAD_RangeCardData data)
	{
		if (!data)
			return string.Empty;
		string signature = string.Format("%1|%2|%3|%4|%5|%6", data.m_sBulletName, data.m_sBulletDetail, data.m_sError, data.m_bSuppressed, data.m_bMissingWind, data.m_bEstimatedWind);
		foreach (MRAD_RangeCardRow row : data.m_aRows)
			signature += string.Format(";%1:%2:%3:%4", row.m_iRange, row.m_fDropMrad.ToString(-1, 5), row.m_fWindMrad.ToString(-1, 5), row.m_bHasWind);
		return signature;
	}

	protected bool CaptureAuthority(int phase, int expectedRows)
	{
		MRAD_RangeCardData data = m_Notebook.GetData();
		if (!data || !data.m_sError.IsEmpty() || data.m_aRows.Count() != expectedRows)
		{
			Fail("authority notebook did not generate the required snapshot");
			FinishServer();
			return false;
		}
		m_NotebookId = Replication.FindItemId(m_Notebook);
		m_ScopeId = Replication.FindItemId(m_Scope);
		if (!m_NotebookId.IsValid() || !m_ScopeId.IsValid() || !Replication.FindItemId(this).IsValid())
		{
			Fail("authority fixture or probe lacks a replication ID");
			FinishServer();
			return false;
		}
		m_iExpectedPage = m_Notebook.GetPageIndex();
		m_iExpectedRows = data.m_aRows.Count();
		m_iExpectedElevation = m_Scope.GetElevationTicks();
		m_iExpectedWindage = m_Scope.GetWindageTicks();
		m_sExpectedRows = DataSignature(data);
		m_iPhase = phase;
		Replication.BumpMe();
		PrintFormat("MRAD_NETWORK_AUTHORITY phase=%1 page=%2 rows=%3 elevationTicks=%4 windageTicks=%5 notebook=%6 scope=%7", phase, m_iExpectedPage, m_iExpectedRows, m_iExpectedElevation, m_iExpectedWindage, m_NotebookId, m_ScopeId);
		return true;
	}

	protected void PublishFirst()
	{
		m_Gadget.OnModeChanged(EGadgetMode.IN_HAND, m_Character);
		m_Notebook.ApplySettingsAuthority(1000, 5, 4, false);
		m_Notebook.ChangePageAuthority(1);
		m_Scope.ApplyDeltaAuthority(10, 0, 500, 500, false);
		m_Scope.ApplyDeltaAuthority(0, 30, 500, 500, false);
		if (CaptureAuthority(1, 10))
			Print("MRAD_NETWORK_READY phase=1 awaitingFirstClient=1");
	}

	protected void PublishSecond()
	{
		if (m_bClosing)
			return;
		m_Notebook.ApplySettingsAuthority(1200, 6, 4, false);
		m_Notebook.ChangePageAuthority(1);
		m_Scope.ApplyDeltaAuthority(7, 0, 500, 500, false);
		m_Scope.ApplyDeltaAuthority(0, -11, 500, 500, false);
		CaptureAuthority(2, 12);
	}

	void Acknowledge(SCR_PlayerController sender, int phase)
	{
		if (!Replication.IsServer() || !sender || m_bClosing || phase != m_iPhase)
			return;
		PrintFormat("MRAD_NETWORK_ACK player=%1 phase=%2", sender.GetPlayerId(), phase);
		if (phase == 1 && !m_bSecondPhaseScheduled)
		{
			m_bSecondPhaseScheduled = true;
			GetGame().GetCallqueue().CallLater(PublishSecond, 1500, false);
		}
		else if (phase == 2)
		{
			m_iFinalAcknowledgements++;
			// Client closes itself first; keep the session alive until its log
			// and normal process exit have completed. The 90 s deadline remains.
			if (m_iFinalAcknowledgements >= 2)
				GetGame().GetCallqueue().CallLater(FinishServer, 2000, false);
		}
	}

	void BootstrapStreaming(SCR_PlayerController player)
	{
		if (!Replication.IsServer() || !player || m_bClosing)
			return;
		// The native MP test lobby can hold connection streaming until respawn.
		// This isolated data test deliberately has no respawn/possessed player.
		// Enable only the requesting QA peer; do not change product replication.
		RplIdentity identity = player.GetRplIdentity();
		RplComponent.InsertMPObserver(identity, 0, 0);
		if (m_aObservers.Find(identity) < 0)
			m_aObservers.Insert(identity);
		RplComponent.EnableStreamingForConnection(identity, true);
		PrintFormat("MRAD_NETWORK_STREAM_ENABLED player=%1 identity=%2", player.GetPlayerId(), player.GetRplIdentity());
		PrintNodeDiagnostics();
		Replication.BumpMe();
	}

	protected void PollReplica()
	{
		if (m_bClosing || m_bADSCheckStarted)
			return;
		m_iReplicaPolls++;
		if (m_iReplicaPolls == 1 || m_iReplicaPolls % 20 == 0)
			PrintReplicaDiagnostics();
		SCR_PlayerController player = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (player && m_iStreamingRequests < 8 && m_iReplicaPolls % 8 == 0 && (m_iPhase <= 0 || !Replication.FindItem(m_NotebookId) || !Replication.FindItem(m_ScopeId)))
		{
			// A native owned controller exists before optional runtime entities are
			// streamed. Bootstrap must not depend on those entities or their IDs.
			player.MRAD_QA_RequestNetworkStream();
			m_iStreamingRequests++;
		}
		if (m_iPhase <= 0)
		{
			m_sLastMismatch = "probe metadata has not streamed";
			return;
		}
		Drg_AutoNotepadRtComponent notebook = Drg_AutoNotepadRtComponent.Cast(Replication.FindItem(m_NotebookId));
		MRAD_ZeroingStateComponent scope = MRAD_ZeroingStateComponent.Cast(Replication.FindItem(m_ScopeId));
		if (!notebook || !scope || !player)
		{
			m_sLastMismatch = "fixture stream or local player controller not ready";
			return;
		}
		MRAD_RangeCardData data = notebook.GetData();
		if (!data || data.m_aRows.Count() != m_iExpectedRows || notebook.GetPageIndex() != m_iExpectedPage || DataSignature(data) != m_sExpectedRows)
		{
			m_sLastMismatch = "notebook rows/page have not converged to authority snapshot";
			return;
		}
		if (scope.GetElevationTicks() != m_iExpectedElevation || scope.GetWindageTicks() != m_iExpectedWindage)
		{
			m_sLastMismatch = "scope ticks have not converged to authority state";
			return;
		}
		if (m_iLastPassedPhase != m_iPhase)
		{
			if (m_iFirstPhase == 0)
				m_iFirstPhase = m_iPhase;
			m_iLastPassedPhase = m_iPhase;
			m_iPassedSnapshots++;
			PrintFormat("MRAD_NETWORK_CLIENT_PASS phase=%1 page=%2 rows=%3 elevationTicks=%4 windageTicks=%5", m_iPhase, notebook.GetPageIndex(), data.m_aRows.Count(), scope.GetElevationTicks(), scope.GetWindageTicks());
		}
		player.MRAD_QA_AcknowledgeNetwork(Replication.FindItemId(this), m_iPhase);
		if (m_iPhase == 2)
		{
			if (m_iFirstPhase == 1 && System.IsCLIParam("mradNetworkADS"))
			{
				m_bADSCheckStarted = true;
				MRAD_NetworkADSCheck.StartClient(this);
			}
			else
				FinishClient();
		}
	}

	void OnADSCheckFinished(int failures)
	{
		m_iFailures += failures;
		FinishClient();
	}

	protected void PrintReplicaDiagnostics()
	{
		PrintNodeDiagnostics();
		Drg_AutoNotepadRtComponent notebook;
		MRAD_ZeroingStateComponent scope;
		if (m_NotebookId.IsValid())
			notebook = Drg_AutoNotepadRtComponent.Cast(Replication.FindItem(m_NotebookId));
		if (m_ScopeId.IsValid())
			scope = MRAD_ZeroingStateComponent.Cast(Replication.FindItem(m_ScopeId));
		PlayerController player = GetGame().GetPlayerController();
		PrintFormat("MRAD_NETWORK_DIAG phase=%1 probeId=%2 notebookId=%3 scopeId=%4 notebook=%5 scope=%6 player=%7 mismatch=%8", m_iPhase, Replication.FindItemId(this), m_NotebookId, m_ScopeId, notebook != null, scope != null, player != null, m_sLastMismatch);
		if (notebook)
		{
			MRAD_RangeCardData data = notebook.GetData();
			int count = -1;
			string signature;
			if (data)
			{
				count = data.m_aRows.Count();
				signature = DataSignature(data);
			}
			PrintFormat("MRAD_NETWORK_DIAG_NOTEBOOK page=%1 expectedPage=%2 rows=%3 expectedRows=%4 signatureEqual=%5", notebook.GetPageIndex(), m_iExpectedPage, count, m_iExpectedRows, signature == m_sExpectedRows);
			if (data && count == m_iExpectedRows && signature != m_sExpectedRows)
			{
				Print("MRAD_NETWORK_DIAG_EXPECTED " + m_sExpectedRows);
				Print("MRAD_NETWORK_DIAG_OBSERVED " + signature);
			}
		}
		if (scope)
			PrintFormat("MRAD_NETWORK_DIAG_SCOPE elevation=%1 expectedElevation=%2 windage=%3 expectedWindage=%4", scope.GetElevationTicks(), m_iExpectedElevation, scope.GetWindageTicks(), m_iExpectedWindage);
	}

	protected void PrintNodeDiagnostics()
	{
		RplComponent rpl = RplComponent.Cast(FindComponent(RplComponent));
		if (!rpl)
		{
			Print("MRAD_NETWORK_DIAG_NODE missingRpl=1");
			return;
		}
		RplNode node = rpl.GetNode();
		int count = -1;
		if (node)
			count = node.GetItemCount();
		PrintFormat("MRAD_NETWORK_DIAG_NODE phase=%1 probeId=%2 rplId=%3 proxy=%4 owner=%5 selfInserted=%6 released=%7 nodeItems=%8", m_iPhase, Replication.FindItemId(this), rpl.Id(), rpl.IsProxy(), rpl.IsOwner(), rpl.IsSelfInserted(), rpl.IsReleasedFromRpl(), count);
	}

	protected void StopCallbacks()
	{
		GetGame().GetCallqueue().Remove(PollReplica);
		GetGame().GetCallqueue().Remove(PrepareAuthority);
		GetGame().GetCallqueue().Remove(PublishFirst);
		GetGame().GetCallqueue().Remove(PublishSecond);
		GetGame().GetCallqueue().Remove(Timeout);
		GetGame().GetCallqueue().Remove(FinishServer);
	}

	protected void Timeout()
	{
		Fail(string.Format("network deadline expired phase=%1 finalAcks=%2 lastMismatch=%3", m_iPhase, m_iFinalAcknowledgements, m_sLastMismatch));
		if (Replication.IsServer())
			FinishServer();
		else
			FinishClient();
	}

	protected void FinishServer()
	{
		if (m_bClosing)
			return;
		m_bClosing = true;
		StopCallbacks();
		if (m_iPhase != 2 || m_iFinalAcknowledgements < 2)
			Fail("both live update and JIP acknowledgements are required");
		PrintFormat("MRAD_NETWORK_SERVER_RESULT phase=%1 finalAcks=%2 failures=%3", m_iPhase, m_iFinalAcknowledgements, m_iFailures);
		foreach (RplIdentity identity : m_aObservers)
			RplComponent.RemoveMPObserver(identity);
		m_aObservers.Clear();
		MRAD_NetworkADSCheck.CleanupAuthority();
		foreach (IEntity entity : m_aSpawned)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		m_aSpawned.Clear();
		GetGame().GetCallqueue().CallLater(CloseProcess, 100, false);
	}

	protected void FinishClient()
	{
		if (m_bClosing)
			return;
		m_bClosing = true;
		StopCallbacks();
		if (m_iLastPassedPhase != 2)
			Fail("client never observed the final authoritative snapshot");
		PrintFormat("MRAD_NETWORK_CLIENT_RESULT snapshots=%1 firstPhase=%2 lastPhase=%3 failures=%4", m_iPassedSnapshots, m_iFirstPhase, m_iLastPassedPhase, m_iFailures);
		GetGame().GetCallqueue().CallLater(CloseProcess, 1000, false);
	}

	protected void CloseProcess()
	{
		GetGame().RequestClose();
	}
}

// QA addon only: an acknowledgement uses an owned native controller so that
// a client never sends Server RPC from the unowned server probe hierarchy.
modded class SCR_PlayerController : PlayerController
{
	void MRAD_QA_RequestNetworkStream()
	{
		Rpc(RpcAsk_MRAD_QA_NetworkStream);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MRAD_QA_NetworkStream()
	{
		MRAD_NetworkProbe probe = MRAD_NetworkProbe.GetInstance();
		if (probe)
			probe.BootstrapStreaming(this);
	}

	void MRAD_QA_AcknowledgeNetwork(RplId probeId, int phase)
	{
		if (probeId.IsValid())
			Rpc(RpcAsk_MRAD_QA_NetworkAck, probeId, phase);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MRAD_QA_NetworkAck(RplId probeId, int phase)
	{
		MRAD_NetworkProbe probe = MRAD_NetworkProbe.Cast(Replication.FindItem(probeId));
		if (probe && (phase == 1 || phase == 2))
			probe.Acknowledge(this, phase);
	}
}

// Один узкий modded нужен для RPC через принадлежащий клиенту объект.
// Прицел может быть отдельной серверной replication hierarchy.
modded class SCR_PlayerController : PlayerController
{
	protected int m_iMRADZeroingSyncSequence;
	protected int m_iMRADZeroingSyncAck;

	int MRAD_BeginZeroingSync()
	{
		m_iMRADZeroingSyncSequence++;
		return m_iMRADZeroingSyncSequence;
	}

	bool MRAD_HasZeroingSyncAck(int requestId)
	{
		return requestId > 0 && m_iMRADZeroingSyncAck >= requestId;
	}

	bool MRAD_RequestZeroing(MRAD_ZeroingStateComponent state, int elevationDelta, int windageDelta, int elevationLimit, int windageLimit, bool reset = false, int syncRequestId = 0)
	{
		if (!state)
			return false;

		if (Replication.IsServer())
		{
			bool accepted = MRAD_ApplyZeroingRequest(state, elevationDelta, windageDelta, elevationLimit, windageLimit, reset);
			if (accepted && syncRequestId > 0)
				m_iMRADZeroingSyncAck = Math.Max(m_iMRADZeroingSyncAck, syncRequestId);
			return accepted;
		}

		RplId stateId = Replication.FindItemId(state);
		if (!stateId.IsValid())
			return false;

		Rpc(RpcAsk_MRAD_Zeroing, stateId, elevationDelta, windageDelta, elevationLimit, windageLimit, reset, syncRequestId);
		return true;
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MRAD_Zeroing(RplId stateId, int elevationDelta, int windageDelta, int elevationLimit, int windageLimit, bool reset, int syncRequestId)
	{
		if (!stateId.IsValid())
			return;

		MRAD_ZeroingStateComponent state = MRAD_ZeroingStateComponent.Cast(Replication.FindItem(stateId));
		if (MRAD_ApplyZeroingRequest(state, elevationDelta, windageDelta, elevationLimit, windageLimit, reset) && syncRequestId > 0)
			Rpc(RpcDo_MRAD_ZeroingSyncAck, syncRequestId);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	protected void RpcDo_MRAD_ZeroingSyncAck(int requestId)
	{
		m_iMRADZeroingSyncAck = Math.Max(m_iMRADZeroingSyncAck, requestId);
	}

	protected bool MRAD_ApplyZeroingRequest(MRAD_ZeroingStateComponent state, int elevationDelta, int windageDelta, int elevationLimit, int windageLimit, bool reset)
	{
		if (!Replication.IsServer() || !state)
			return false;

		ChimeraCharacter character = ChimeraCharacter.Cast(GetControlledEntity());
		if (!character)
			return false;

		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (!controller || !controller.IsWeaponADS() || controller.IsDead() || controller.IsUnconscious())
			return false;

		BaseWeaponManagerComponent manager = controller.GetWeaponManagerComponent();
		if (!manager || !manager.GetCurrentWeapon())
			return false;

		// Проверяем сам компонент текущего прицела, не присланный клиентом playerId.
		BaseSightsComponent currentSights = manager.GetCurrentSights();
		if (MRAD_ZeroingStateComponent.FindForSights(currentSights) != state)
			return false;

		return state.ApplyDeltaAuthority(elevationDelta, windageDelta, elevationLimit, windageLimit, reset);
	}
}

// Requests travel through the requesting player's owned replication hierarchy.
modded class SCR_PlayerController : PlayerController
{
	void MRAD_RequestNotebookPage(Drg_AutoNotepadRtComponent notepad, int direction)
	{
		if (!notepad)
			return;
		if (Replication.IsServer())
		{
			if (MRAD_CanControlNotebook(notepad))
				notepad.ChangePageAuthority(direction);
			return;
		}
		RplId id = Replication.FindItemId(notepad);
		if (id.IsValid())
			Rpc(RpcAsk_MRAD_NotebookPage, id, direction);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MRAD_NotebookPage(RplId id, int direction)
	{
		Drg_AutoNotepadRtComponent notepad = Drg_AutoNotepadRtComponent.Cast(Replication.FindItem(id));
		if (MRAD_CanControlNotebook(notepad))
			notepad.ChangePageAuthority(direction);
	}

	void MRAD_RequestNotebookSettings(Drg_AutoNotepadRtComponent notepad, int maxRange, int rows, float wind, bool actualWind)
	{
		if (!notepad)
			return;
		if (Replication.IsServer())
		{
			if (MRAD_CanControlNotebook(notepad))
				notepad.ApplySettingsAuthority(maxRange, rows, wind, actualWind);
			return;
		}
		RplId id = Replication.FindItemId(notepad);
		if (id.IsValid())
			Rpc(RpcAsk_MRAD_NotebookSettings, id, maxRange, rows, wind, actualWind);
	}

	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_MRAD_NotebookSettings(RplId id, int maxRange, int rows, float wind, bool actualWind)
	{
		Drg_AutoNotepadRtComponent notepad = Drg_AutoNotepadRtComponent.Cast(Replication.FindItem(id));
		if (MRAD_CanControlNotebook(notepad))
			notepad.ApplySettingsAuthority(maxRange, rows, wind, actualWind);
	}

	protected bool MRAD_CanControlNotebook(Drg_AutoNotepadRtComponent notepad)
	{
		if (!Replication.IsServer() || !notepad)
			return false;
		ChimeraCharacter character = ChimeraCharacter.Cast(GetControlledEntity());
		if (!character)
			return false;
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (!controller || controller.IsDead() || controller.IsUnconscious())
			return false;
		MRAD_NotepadGadgetComponent gadget = MRAD_NotepadGadgetComponent.Cast(notepad.GetOwner().FindComponent(MRAD_NotepadGadgetComponent));
		return gadget && gadget.GetCharacterOwner() == character && gadget.GetMode() == EGadgetMode.IN_HAND;
	}
}

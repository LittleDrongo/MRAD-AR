[ComponentEditorProps(category: "GameScripted/MRAD", description: "Specialist gadget for MRAD notepads")]
class MRAD_NotepadGadgetComponentClass : SCR_DataDisplayGadgetComponentClass
{
}

// Vanilla specialist-item behaviour (in-hand animation, page actions).
// The page itself is drawn by Drg_NotepadRtComponent on the same entity;
// its render target lives for the whole life of the item, as in 1.x.
class MRAD_NotepadGadgetComponent : SCR_DataDisplayGadgetComponent
{
	//------------------------------------------------------------------------------------------------
	protected Drg_NotepadRtComponent GetNotepad()
	{
		return Drg_NotepadRtComponent.Cast(GetOwner().FindComponent(Drg_NotepadRtComponent));
	}

	//------------------------------------------------------------------------------------------------
	protected override void EOnInit(IEntity owner)
	{
		m_iNumberOfPages = 1;
	}

	//------------------------------------------------------------------------------------------------
	protected override bool InitializeRenderTarget()
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (!notepad)
			return false;

		notepad.OnTakenInHand(ChimeraCharacter.Cast(GetCharacterOwner()));
		return true;
	}

	//------------------------------------------------------------------------------------------------
	protected override void ClearRenderTarget()
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (notepad)
			notepad.OnPutAway();
	}

	//------------------------------------------------------------------------------------------------
	override int GetNumberOfPages()
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (notepad)
			return notepad.GetNumberOfPages();

		return 1;
	}

	//------------------------------------------------------------------------------------------------
	override void ChangePage(int direction = 0)
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (notepad)
			notepad.ChangePage(direction);
	}

	//------------------------------------------------------------------------------------------------
	protected override void UpdateData(bool playSound = true)
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (notepad)
			notepad.Draw();
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnDelete(IEntity owner)
	{
		DisconnectFromGadgetsSystem();
	}
}

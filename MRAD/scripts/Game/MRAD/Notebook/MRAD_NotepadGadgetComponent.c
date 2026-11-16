[ComponentEditorProps(category: "GameScripted/MRAD", description: "Native specialist gadget with dynamic MRAD pages")]
class MRAD_NotepadGadgetComponentClass : SCR_DataDisplayGadgetComponentClass
{
}

// Native inventory/animation/action behavior, with a separate data lifecycle.
// No global override of SCR_GadgetComponent is necessary.
class MRAD_NotepadGadgetComponent : SCR_DataDisplayGadgetComponent
{
	protected Drg_NotepadRtComponent GetNotepad()
	{
		return Drg_NotepadRtComponent.Cast(GetOwner().FindComponent(Drg_NotepadRtComponent));
	}

	protected override void EOnInit(IEntity owner)
	{
		m_iNumberOfPages = 1;
	}

	protected override bool InitializeRenderTarget()
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		return notepad && notepad.OpenFor(GetCharacterOwner());
	}

	protected override void ClearRenderTarget()
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (notepad)
			notepad.Close();
	}

	override int GetNumberOfPages()
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (notepad)
			return notepad.GetNumberOfPages();
		return 1;
	}

	override void ChangePage(int direction = 0)
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (!notepad)
			return;
		if (direction == 0)
			notepad.Draw();
		else
			notepad.RequestPage(direction);
	}

	protected override void OnDelete(IEntity owner)
	{
		ClearRenderTarget();
		DisconnectFromGadgetsSystem();
	}

	protected override void UpdateData(bool playSound = true)
	{
		Drg_NotepadRtComponent notepad = GetNotepad();
		if (notepad)
			notepad.Draw();
	}
}

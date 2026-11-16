// Class/attribute names stay as in 1.x so inherited notepads keep working.
// Rendering follows the original 1.x scheme: one RichTextWidget "drg_Text"
// in NotepadSheet.layout, drawn into the item's render target.
[ComponentEditorProps(category: "GameScripted/MRAD", description: "Notepad page drawn into the item render target")]
class Drg_NotepadRtComponentClass : ScriptComponentClass
{
}

class Drg_NotepadRtComponent : ScriptComponent
{
	[Attribute("")]
	ResourceName m_sPathLayout;

	[Attribute("", UIWidgets.EditBoxMultiline)]
	string m_sText;

	protected Widget m_wRTRoot;
	protected RichTextWidget m_wRich;
	protected RTTextureWidget m_wRtTexture;

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}

	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		CreateRenderTarget();
	}

	//------------------------------------------------------------------------------------------------
	//! (Re)creates the page widgets and binds them to this item's render target.
	protected void CreateRenderTarget()
	{
		ClearRenderTarget();

		if (SCR_Global.IsEditMode() || System.IsConsoleApp())
			return;

		WorkspaceWidget workspace = GetGame().GetWorkspace();
		if (!workspace || m_sPathLayout.IsEmpty())
			return;

		m_wRTRoot = workspace.CreateWidgets(m_sPathLayout);
		if (!m_wRTRoot)
			return;

		m_wRtTexture = RTTextureWidget.Cast(m_wRTRoot.FindAnyWidget("drg_Main_RTTexture"));
		m_wRich = RichTextWidget.Cast(m_wRTRoot.FindAnyWidget("drg_Text"));
		if (!m_wRtTexture || !m_wRich)
		{
			ClearRenderTarget();
			return;
		}

		// Vanilla data-display pattern: the texture renders when enabled for a draw, then stays.
		m_wRtTexture.SetRenderTarget(GetOwner());
		m_wRtTexture.SetEnabled(false);
		Draw();
	}

	//------------------------------------------------------------------------------------------------
	protected void ClearRenderTarget()
	{
		IEntity owner = GetOwner();
		if (m_wRtTexture && owner && !owner.IsDeleted())
			m_wRtTexture.RemoveRenderTarget(owner);

		m_wRtTexture = null;
		m_wRich = null;

		if (m_wRTRoot)
			m_wRTRoot.RemoveFromHierarchy();

		m_wRTRoot = null;
	}

	//------------------------------------------------------------------------------------------------
	//! Text of the current page. Static notepads show m_sText.
	protected string BuildText()
	{
		return m_sText;
	}

	//------------------------------------------------------------------------------------------------
	string GetPageText()
	{
		return BuildText();
	}

	//------------------------------------------------------------------------------------------------
	void Draw()
	{
		if (!m_wRich || !m_wRtTexture)
			return;

		m_wRtTexture.SetEnabled(true);
		m_wRich.SetText(BuildText());
		m_wRtTexture.SetEnabled(false);
	}

	//------------------------------------------------------------------------------------------------
	//! Called by MRAD_NotepadGadgetComponent when the notepad is taken into hands.
	void OnTakenInHand(ChimeraCharacter character)
	{
		// Only LOD 0 of Notepad_LP.xob has the "sheet" part with the render-target material;
		// in hands the item is often small enough on screen for LOD 1 (plain paper texture).
		GetOwner().SetFixedLOD(0);
		// The item mesh in hands needs a fresh binding (same as vanilla data-display gadgets).
		CreateRenderTarget();
	}

	//------------------------------------------------------------------------------------------------
	//! Called by MRAD_NotepadGadgetComponent when the notepad leaves the hands.
	void OnPutAway()
	{
		IEntity owner = GetOwner();
		if (owner && !owner.IsDeleted())
			owner.SetFixedLOD(-1);
	}

	//------------------------------------------------------------------------------------------------
	int GetNumberOfPages()
	{
		return 1;
	}

	//------------------------------------------------------------------------------------------------
	void ChangePage(int direction)
	{
		Draw();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		ClearRenderTarget();
	}
}

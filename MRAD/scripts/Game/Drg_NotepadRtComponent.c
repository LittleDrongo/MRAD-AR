// Existing class/attribute names stay stable for inherited 1.x notepads.
[ComponentEditorProps(category: "GameScripted/MRAD", description: "In-hand notepad render target")]
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
	protected RTTextureWidget m_wRtTexture;
	protected ChimeraCharacter m_Character;
	protected bool m_bHeld;

	bool OpenFor(ChimeraCharacter character)
	{
		if (SCR_Global.IsEditMode() || !character)
			return false;
		m_Character = character;
		m_bHeld = true;
		// Dedicated server keeps data/page state but never creates UI.
		if (System.IsConsoleApp() || !GetGame().GetWorkspace())
			return true;
		if (m_wRTRoot)
			return true;
		if (m_sPathLayout.IsEmpty())
			return false;
		m_wRTRoot = GetGame().GetWorkspace().CreateWidgets(m_sPathLayout);
		if (!m_wRTRoot)
			return false;
		m_wRtTexture = RTTextureWidget.Cast(m_wRTRoot.FindAnyWidget("drg_Main_RTTexture"));
		if (!m_wRtTexture)
		{
			m_wRTRoot.RemoveFromHierarchy();
			m_wRTRoot = null;
			return false;
		}
		m_wRtTexture.SetRenderTarget(GetOwner());
		Draw();
		return true;
	}

	void Close()
	{
		m_bHeld = false;
		m_Character = null;
		// Detach while the widget is alive, before removing its hierarchy.
		if (m_wRtTexture && GetOwner() && !GetOwner().IsDeleted())
			m_wRtTexture.RemoveRenderTarget(GetOwner());
		m_wRtTexture = null;
		if (m_wRTRoot)
			m_wRTRoot.RemoveFromHierarchy();
		m_wRTRoot = null;
	}

	void Draw()
	{
		if (!m_wRTRoot || !m_wRtTexture)
			return;
		m_wRtTexture.SetEnabled(true);
		RichTextWidget text = RichTextWidget.Cast(m_wRTRoot.FindAnyWidget("drg_Text"));
		if (text)
			text.SetText(m_sText);
		m_wRtTexture.SetEnabled(false);
	}

	int GetNumberOfPages() { return 1; }
	void RequestPage(int direction) {}
	bool IsHeld() { return m_bHeld; }
	bool HasRenderTarget() { return m_wRTRoot != null && m_wRtTexture != null; }
	ChimeraCharacter GetHolder() { return m_Character; }

	override void OnDelete(IEntity owner)
	{
		Close();
	}
}

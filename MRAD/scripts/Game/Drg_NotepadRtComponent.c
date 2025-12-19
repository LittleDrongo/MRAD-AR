[ComponentEditorProps(category: "GameScripted/Misc", description: "")]
class Drg_NotepadRtComponentClass : ScriptGameComponentClass
{
}

class Drg_NotepadRtComponent : ScriptGameComponent
{
	[Attribute("")]
	ResourceName m_sPathLayout;
	
	[Attribute("", UIWidgets.EditBoxMultiline)]
	string m_sText;
	
	Widget m_wRTRoot;
	RichTextWidget m_wRich;
	RTTextureWidget m_wRtTexture;
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		if (SCR_Global.IsEditMode())
			return;
		
		m_wRTRoot = GetGame().GetWorkspace().CreateWidgets(m_sPathLayout);
		m_wRtTexture = RTTextureWidget.Cast(m_wRTRoot.FindAnyWidget("drg_Main_RTTexture"));
		m_wRich = RichTextWidget.Cast(m_wRTRoot.FindAnyWidget("drg_Text"));
		m_wRtTexture.SetRenderTarget(owner);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_wRTRoot)
			m_wRTRoot.RemoveFromHierarchy();
		
		if (m_wRtTexture)
		{
			m_wRtTexture.SetRenderTarget(owner);
			m_wRtTexture = null;
		}
	}	
}

[ComponentEditorProps(category: "GameScripted/Misc", description: "")]
class Drg_AutoNotepadRtComponentClass : Drg_NotepadRtComponentClass
{
}


class Drg_AutoNotepadRtComponent : Drg_NotepadRtComponent
{
	void RecalculateWidget()
	{
		m_wRich.SetText(CalculateRadians());
	}
	
	BaseWeaponComponent GetMuzzleFromWeapon()
	{
		IEntity controlledEntity = GetOwner().GetRootParent();
		if (!controlledEntity)
			return null;
		
		ChimeraCharacter character = ChimeraCharacter.Cast(controlledEntity);
		if (!character)
			return null;
		
		CharacterWeaponManagerComponent manager = CharacterWeaponManagerComponent.Cast(character.FindComponent(CharacterWeaponManagerComponent));
		BaseWeaponComponent current = manager.GetCurrentWeapon();
		if (!current)
			return null;
		
		return current;
	}
	
	string CalculateRadians()
	{
		BaseWeaponComponent weapon = GetMuzzleFromWeapon();
		if (!weapon)
			return WidgetManager.Translate("#MRAD_Error_NoWeapon");
		
		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle)
			return WidgetManager.Translate("#MRAD_Error_NoMuzzle");
			
		BaseMagazineComponent magazine = weapon.GetCurrentMagazine();
		if (!magazine)
			return WidgetManager.Translate("#MRAD_Error_NoMagazine");
			
		string returnValue;
		
		MagazineUIInfo magazineUI = MagazineUIInfo.Cast(magazine.GetUIInfo());

		returnValue =  string.Format("%1 \n\n", WidgetManager.Translate(magazineUI.GetAmmoCaliber()));
		
		
		float coef = muzzle.GetBulletInitSpeedCoef();
		
		for (int i = 1; i <= 15; i++)
		{
			float distance = 100 * i;
			float travelTime, aimHeight, angleRad;
	
			aimHeight = BallisticTable.GetAimHeightOfNextProjectile(distance, travelTime, muzzle, false);
			angleRad = Math.Atan2(aimHeight, distance);
			angleRad = SCR_Math.ConvertFromRadians(angleRad, SCR_EOpticsAngleUnits.MILLIRADIANS) * coef;
			string roundValue = angleRad.ToString(2, 1); 
			returnValue += string.Format("%1 m --- %2 Mrad\n", distance, roundValue);
		}
			
		return returnValue;
	}
}

modded class SCR_GadgetComponent : ScriptGameComponent
{
	override void UpdateVisibility(EGadgetMode mode)
	{	
		super.UpdateVisibility(mode);
		Drg_AutoNotepadRtComponent notepad = Drg_AutoNotepadRtComponent.Cast(GetOwner().FindComponent(Drg_AutoNotepadRtComponent));
		if (!notepad)
			return;
		
		notepad.RecalculateWidget();
	}
}

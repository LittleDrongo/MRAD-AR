// Page index is local to the holder (the card is calculated locally too).
class MRAD_ChangeNotepadPageAction : SCR_ChangeDataPageAction
{
	override bool CanBeShownScript(IEntity user)
	{
		return super.CanBeShownScript(user) && m_DataDisplayGadgetComp.GetCharacterOwner() == user;
	}

	override bool HasLocalEffectOnlyScript()
	{
		return true;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (CanBeShownScript(pUserEntity))
			super.PerformAction(pOwnerEntity, pUserEntity);
	}
}

class MRAD_ChangeNotepadPageAction : SCR_ChangeDataPageAction
{
	override bool CanBeShownScript(IEntity user)
	{
		return super.CanBeShownScript(user) && m_DataDisplayGadgetComp.GetCharacterOwner() == user;
	}

	override void PerformAction(IEntity pOwnerEntity, IEntity pUserEntity)
	{
		if (CanBeShownScript(pUserEntity))
			super.PerformAction(pOwnerEntity, pUserEntity);
	}
}

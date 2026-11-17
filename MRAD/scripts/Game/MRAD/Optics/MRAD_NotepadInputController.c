// Zeroing input outside ADS: while the weapon is in hands, or while the range-card
// notepad is held. The normal MRAD_OpticsContext only exists in ADS, so this controller
// activates the passive zeroing context and routes the actions to the slot-0 weapon optic.
class MRAD_NotepadInputController
{
	protected static bool s_bHeld;

	//------------------------------------------------------------------------------------------------
	static void SetHeld(bool held)
	{
		s_bHeld = held;
	}

	//------------------------------------------------------------------------------------------------
	static void Update()
	{
		if (SCR_Global.IsEditMode() || System.IsConsoleApp())
			return;

		ChimeraCharacter character = ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;

		SCR_2DOpticsComponent optic;
		if (s_bHeld)
		{
			// The notepad is in hands: the weapon itself may not be the current one.
			optic = FindSlot0Optic(character);
		}
		else
		{
			// Weapon in hands: use its current sights, ADS or not.
			BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(character.FindComponent(BaseWeaponManagerComponent));
			if (manager)
				optic = SCR_2DOpticsComponent.Cast(manager.GetCurrentSights());
		}

		if (!optic || !optic.MRAD_IsSniperOptic() || optic.MRAD_IsActive())
			return;

		MRAD_OpticsInputController controller = optic.MRAD_GetInput();
		if (!controller)
			return;

		InputManager inputManager = GetGame().GetInputManager();
		float dt = 0.016;
		if (inputManager)
			dt = inputManager.GetLastTimeDelta();

		controller.UpdatePassive(dt);
	}

	//------------------------------------------------------------------------------------------------
	//! Sights of the weapon in slot 0 (the card's weapon).
	protected static SCR_2DOpticsComponent FindSlot0Optic(ChimeraCharacter character)
	{
		CharacterWeaponManagerComponent manager = CharacterWeaponManagerComponent.Cast(character.FindComponent(CharacterWeaponManagerComponent));
		if (!manager)
			return null;

		array<WeaponSlotComponent> slots = {};
		manager.GetWeaponsSlots(slots);

		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot || slot.GetWeaponSlotIndex() != 0)
				continue;

			return SCR_2DOpticsComponent.Cast(slot.GetSights());
		}

		return null;
	}
}

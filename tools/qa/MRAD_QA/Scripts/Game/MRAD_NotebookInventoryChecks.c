// Bounded native entity/slot regression. No fabricated range-card inputs or cache edits.
// Inventory actions are performed on a separate character owned by this helper.
class MRAD_NotebookInventoryChecks
{
	protected int m_iChecks;
	protected int m_iFailures;
	protected int m_iWeaponChanges;
	protected int m_iMagazineChanges;

	protected void Check(bool passed, string description)
	{
		m_iChecks++;
		if (passed)
			Print("MRAD_INVENTORY_PASS " + description);
		else
		{
			m_iFailures++;
			Print("MRAD_INVENTORY_FAIL " + description, LogLevel.ERROR);
		}
	}

	protected IEntity Spawn(ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = Vector(8, 5, 8);
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	protected ResourceName ReadProjectile(IEntity weapon)
	{
		if (!weapon)
			return string.Empty;
		BaseWeaponComponent component = BaseWeaponComponent.Cast(weapon.FindComponent(BaseWeaponComponent));
		if (!component || !component.GetCurrentMuzzle() || !component.GetCurrentMuzzle().GetMagazine())
			return string.Empty;
		return component.GetCurrentMuzzle().GetMagazine().GetAmmoType(0);
	}

	protected void DeleteOwned(IEntity entity)
	{
		if (entity && !entity.IsDeleted())
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	protected void CheckMagazine(ChimeraCharacter character, WeaponSlotComponent primary, Drg_AutoNotepadRtComponent notepad)
	{
		BaseMuzzleComponent muzzle = primary.GetCurrentMuzzle();
		BaseMagazineComponent original;
		if (muzzle)
			original = muzzle.GetMagazine();
		InventoryItemComponent item;
		if (original)
			item = InventoryItemComponent.Cast(original.GetOwner().FindComponent(InventoryItemComponent));
		InventoryStorageSlot parentSlot;
		if (item)
			parentSlot = item.GetParentSlot();
		InventoryStorageManagerComponent storageManager = InventoryStorageManagerComponent.Cast(character.FindComponent(InventoryStorageManagerComponent));
		IEntity replacement = Spawn("{BCB5E5C608ECFF50}Prefabs/Weapons/Magazines/Magazine_762x51_M14_20rnd_M80.et");
		BaseMagazineComponent replacementMagazine;
		if (replacement)
			replacementMagazine = BaseMagazineComponent.Cast(replacement.FindComponent(BaseMagazineComponent));
		Check(original && parentSlot && parentSlot.GetStorage() && storageManager && replacementMagazine, "actual M14 magazine, storage slot and replacement are available");
		if (!original || !parentSlot || !parentSlot.GetStorage() || !storageManager || !replacementMagazine)
		{
			DeleteOwned(replacement);
			return;
		}
		ResourceName beforeProjectile = original.GetAmmoType(0);
		ResourceName afterProjectile = replacementMagazine.GetAmmoType(0);
		Check(!beforeProjectile.IsEmpty() && !afterProjectile.IsEmpty() && beforeProjectile != afterProjectile, "native M118 and M80 magazines expose different actual projectile resources");
		BaseInventoryStorageComponent storage = parentSlot.GetStorage();
		int slotID = parentSlot.GetID();
		bool permitted = storageManager.CanReplaceItem(replacement, storage, slotID);
		int beforeBuild = notepad.GetBuildCount();
		// RefreshData replaces its owning ref. Retain the previous snapshot here;
		// a plain weak local can become null before the comparison is evaluated.
		ref MRAD_RangeCardData oldData = notepad.GetData();
		ref MRAD_RangeCardInput oldInput = MRAD_RangeCardInputResolver.Resolve(character);
		float oldFirstDrop = -1;
		if (oldData && !oldData.m_aRows.IsEmpty())
			oldFirstDrop = oldData.m_aRows[0].m_fDropMrad;
		bool accepted;
		if (permitted)
			accepted = storageManager.TryReplaceItem(replacement, storage, slotID);
		bool observed = muzzle.GetMagazine() == replacementMagazine;
		PrintFormat("MRAD_INVENTORY_MAGAZINE permitted=%1 accepted=%2 observed=%3 before=%4 after=%5", permitted, accepted, observed, beforeProjectile, afterProjectile);
		Check(accepted && observed, "native inventory replacement changes the actual muzzle magazine");
		if (accepted && observed)
		{
			m_iMagazineChanges++;
			// A public page request refreshes real inputs; no private cache field is touched.
			notepad.ChangePageAuthority(1);
			ref MRAD_RangeCardData newData = notepad.GetData();
			ref MRAD_RangeCardInput newInput = MRAD_RangeCardInputResolver.Resolve(character);
			Check(newData && newData.m_sError.IsEmpty() && newData.m_sProjectile == afterProjectile, "magazine replacement updates the notebook to its actual projectile");
			Check(notepad.GetBuildCount() == beforeBuild + 1, "one magazine replacement causes exactly one table rebuild");
			bool nativeMatch = oldData && newData && oldInput && newInput && oldInput.m_Source && newInput.m_Source;
			int nativeRows;
			if (nativeMatch)
			{
				foreach (int index, MRAD_RangeCardRow row : newData.m_aRows)
				{
					float oldTime, newTime;
					float oldHeight = BallisticTable.GetHeightFromProjectileSource(row.m_iRange, oldTime, oldInput.m_Source, oldInput.m_fSpeedCoef, true);
					float newHeight = BallisticTable.GetHeightFromProjectileSource(row.m_iRange, newTime, newInput.m_Source, newInput.m_fSpeedCoef, true);
					float oldNative = SCR_Math.ConvertFromRadians(Math.Atan2(oldHeight, row.m_iRange), SCR_EOpticsAngleUnits.MILLIRADIANS);
					float newNative = SCR_Math.ConvertFromRadians(Math.Atan2(newHeight, row.m_iRange), SCR_EOpticsAngleUnits.MILLIRADIANS);
					float oldCached = -1;
					if (index < oldData.m_aRows.Count() && oldData.m_aRows[index].m_iRange == row.m_iRange)
						oldCached = oldData.m_aRows[index].m_fDropMrad;
					PrintFormat("MRAD_INVENTORY_NATIVE_ROW range=%1 oldNativeMrad=%2 oldCachedMrad=%3 newNativeMrad=%4 newCachedMrad=%5", row.m_iRange, oldNative, oldCached, newNative, row.m_fDropMrad);
					if (oldTime <= 0 || newTime <= 0 || oldHeight <= 0 || newHeight <= 0 || Math.AbsFloat(oldNative - oldCached) > 0.0001 || Math.AbsFloat(newNative - row.m_fDropMrad) > 0.0001)
						nativeMatch = false;
					nativeRows++;
				}
			}
			Check(nativeMatch && nativeRows == 10, "all old and new ammo rows match their distinct native projectile source queries");
			bool changedDrop = oldFirstDrop > 0 && newData && !newData.m_aRows.IsEmpty() && Math.AbsFloat(oldFirstDrop - newData.m_aRows[0].m_fDropMrad) > 0.0001;
			Check(changedDrop, "the M118-to-M80 fixture changes drop as confirmed by the independent native source probe");
			notepad.ChangePageAuthority(-1);
			Check(notepad.GetBuildCount() == beforeBuild + 1, "unchanged replacement magazine reuses the new table");
		}
		// This helper owns the fixture and replacement, whether attached or on the ground.
		DeleteOwned(replacement);
		if (accepted && observed)
		{
			bool magazineRemoved = muzzle.GetMagazine() == null;
			Check(magazineRemoved, "deleting the owned replacement clears the actual muzzle magazine");
			if (magazineRemoved)
			{
				notepad.ChangePageAuthority(1);
				MRAD_RangeCardData emptyData = notepad.GetData();
				Check(emptyData && emptyData.m_sError == "#MRAD_Error_NoMagazine" && emptyData.m_aRows.IsEmpty(), "actual missing magazine yields the localized error and no stale rows");
			}
		}
	}

	protected int RunChecks(Drg_AutoNotepadRtComponent notepad, MRAD_NotepadGadgetComponent gadget, ChimeraCharacter originalHolder)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(Spawn("{37578B1666981FCE}Prefabs/Characters/Core/Character_Base.et"));
		IEntity rifleA = Spawn("{3E413771E1834D2F}Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et");
		IEntity rifleB = Spawn("{B31929F65F0D0279}Prefabs/Weapons/Rifles/M14/Rifle_M21.et");
		CharacterWeaponManagerComponent manager;
		WeaponSlotComponent primary;
		if (character)
			manager = CharacterWeaponManagerComponent.Cast(character.FindComponent(CharacterWeaponManagerComponent));
		if (manager)
		{
			array<WeaponSlotComponent> slots = {};
			manager.GetWeaponsSlots(slots);
			foreach (WeaponSlotComponent slot : slots)
			{
				if (slot && slot.GetWeaponSlotIndex() == 0)
				{
					primary = slot;
					break;
				}
			}
		}
		ResourceName projectileA = ReadProjectile(rifleA);
		ResourceName projectileB = ReadProjectile(rifleB);
		bool ready = character && primary && rifleA && rifleB && !projectileA.IsEmpty() && !projectileB.IsEmpty() && projectileA != projectileB;
		Check(ready, "independent inventory fixture has a real primary slot and two distinct loaded rifles");
		if (ready)
		{
			manager.SetSlotWeapon(primary, rifleA);
			gadget.OnModeChanged(EGadgetMode.IN_HAND, character);
			notepad.ApplySettingsAuthority(1000, 5, 4, false);
			notepad.ChangePageAuthority(1);
			int initialBuild = notepad.GetBuildCount();
			bool dataMatches = true;
			bool oneBuild = true;
			bool stableReads = true;
			for (int cycle = 0; cycle < 100; cycle++)
			{
				IEntity target = rifleA;
				ResourceName expectedProjectile = projectileA;
				if (Math.Mod(cycle, 2) == 0)
				{
					target = rifleB;
					expectedProjectile = projectileB;
				}
				int beforeBuild = notepad.GetBuildCount();
				manager.SetSlotWeapon(primary, target);
				if (primary.GetWeaponEntity() != target)
					break;
				m_iWeaponChanges++;
				notepad.ChangePageAuthority(1);
				MRAD_RangeCardData data = notepad.GetData();
				if (!data || !data.m_sError.IsEmpty() || data.m_sProjectile != expectedProjectile)
					dataMatches = false;
				if (notepad.GetBuildCount() != beforeBuild + 1)
					oneBuild = false;
				notepad.ChangePageAuthority(-1);
				if (notepad.GetBuildCount() != beforeBuild + 1)
					stableReads = false;
			}
			Check(m_iWeaponChanges == 100, "100 native primary-slot weapon replacements complete");
			Check(dataMatches && m_iWeaponChanges == 100, "every actual weapon replacement selects its own projectile data");
			Check(oneBuild && notepad.GetBuildCount() == initialBuild + 100, "100 actual weapon replacements cause exactly 100 table rebuilds");
			Check(stableReads && m_iWeaponChanges == 100, "page requests between replacements preserve the cached table");
			manager.SetSlotWeapon(primary, rifleB);
			notepad.ChangePageAuthority(1);
			CheckMagazine(character, primary, notepad);
			gadget.OnModeChanged(EGadgetMode.IN_HAND, originalHolder);
			Check(notepad.GetHolder() == originalHolder && notepad.GetData() && notepad.GetData().m_sError.IsEmpty(), "the original holder and its real table are restored");
		}
		DeleteOwned(character);
		DeleteOwned(rifleA);
		DeleteOwned(rifleB);
		PrintFormat("MRAD_INVENTORY_RESULT checks=%1 failures=%2 weaponChanges=%3 magazineChanges=%4", m_iChecks, m_iFailures, m_iWeaponChanges, m_iMagazineChanges);
		return m_iFailures;
	}

	static int Run(Drg_AutoNotepadRtComponent notepad, MRAD_NotepadGadgetComponent gadget, ChimeraCharacter originalHolder)
	{
		MRAD_NotebookInventoryChecks probe = new MRAD_NotebookInventoryChecks();
		return probe.RunChecks(notepad, gadget, originalHolder);
	}
}

// Exercises the real prefab and component lifecycle in the QA world.
// These checks do not claim that a second network client or a GUI was observed.
class MRAD_NotebookProbe
{
	protected int m_iChecks;
	protected int m_iFailures;

	protected void Check(bool passed, string label)
	{
		m_iChecks++;
		if (passed)
			Print("MRAD_NOTEBOOK_PASS " + label);
		else
		{
			m_iFailures++;
			Print("MRAD_NOTEBOOK_FAIL " + label, LogLevel.ERROR);
		}
	}

	protected void LogGeometry(IEntity item)
	{
		Physics physics = item.GetPhysics();
		if (!physics)
		{
			Print("MRAD_NOTEPAD_PHYSICS present=0");
			return;
		}
		PrintFormat("MRAD_NOTEPAD_PHYSICS present=1 geoms=%1 layers=%2 mass=%3", physics.GetNumGeoms(), physics.GetInteractionLayer(), physics.GetMass());
		for (int i = 0; i < physics.GetNumGeoms(); i++)
		{
			vector geomMin, geomMax;
			physics.GetGeomWorldBounds(i, geomMin, geomMax);
			PrintFormat("MRAD_NOTEPAD_GEOMETRY index=%1 name=%2 layers=%3 worldMin=%4 worldMax=%5 size=%6", i, physics.GetGeomName(i), physics.GetGeomInteractionLayer(i), geomMin, geomMax, geomMax - geomMin);
		}
	}

	protected int CheckNotebook(ChimeraCharacter character)
	{
		Resource resource = Resource.Load("{2A7C01B93CC92ACE}Prefabs/Items/Equipment/RangeCard/RangeCard_Notepad_AUTO.et");
		Check(resource && resource.IsValid(), "original AUTO resource still loads");
		if (!resource || !resource.IsValid())
			return m_iFailures;
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = Vector(4, 5, 4);
		IEntity item = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		Check(item != null, "AUTO instance spawns");
		if (!item)
			return m_iFailures;
		vector modelMin, modelMax;
		item.GetBounds(modelMin, modelMax);
		PrintFormat("MRAD_NOTEPAD_MODEL_BOUNDS min=%1 max=%2 size=%3 center=%4", modelMin, modelMax, modelMax - modelMin, (modelMin + modelMax) * 0.5);
		LogGeometry(item);
		Drg_AutoNotepadRtComponent notepad = Drg_AutoNotepadRtComponent.Cast(item.FindComponent(Drg_AutoNotepadRtComponent));
		MRAD_NotepadGadgetComponent gadget = MRAD_NotepadGadgetComponent.Cast(item.FindComponent(MRAD_NotepadGadgetComponent));
		Check(notepad && gadget, "AUTO inherits dynamic renderer and native gadget adapter");
		if (!notepad || !gadget)
		{
			SCR_EntityHelper.DeleteEntityAndChildren(item);
			return m_iFailures;
		}
		Check(!notepad.HasRenderTarget() && !notepad.IsHeld(), "spawn on ground allocates no render target");
		Check(Replication.FindItemId(notepad).IsValid(), "notebook snapshot has a replication item ID");
		gadget.OnModeChanged(EGadgetMode.IN_HAND, character);
		Check(notepad.IsHeld() && notepad.GetHolder() == character, "in-hand event resolves the explicit character owner");
		if (System.IsConsoleApp())
			Check(!notepad.HasRenderTarget(), "headless in-hand state allocates no render target");
		else
			Check(notepad.HasRenderTarget(), "client in-hand state creates its render target");
		notepad.ApplySettingsAuthority(1000, 5, 4, false);
		notepad.ChangePageAuthority(1);
		MRAD_RangeCardData data = notepad.GetData();
		Check(data && data.m_sError.IsEmpty() && data.m_aRows.Count() == 10, "actual slot-zero weapon yields ten supported rows");
		Check(notepad.GetNumberOfPages() == 2 && notepad.GetPageIndex() == 1, "authority uses configured page size and advances to page two");
		int buildCount = notepad.GetBuildCount();
		for (int i = 0; i < 100; i++)
			notepad.ChangePageAuthority(1);
		Check(notepad.GetPageIndex() == 1, "100 page changes preserve cyclic page state");
		Check(notepad.GetBuildCount() == buildCount, "page-only changes reuse the calculated table");
		notepad.ChangePageAuthority(0);
		notepad.ChangePageAuthority(2);
		Check(notepad.GetPageIndex() == 1, "invalid authority page directions are rejected");

		// Use a genuinely empty primary slot. SetSlotWeapon(primary, null) is
		// not a supported inventory removal operation in the installed engine.
		Resource characterResource = Resource.Load("{37578B1666981FCE}Prefabs/Characters/Core/Character_Base.et");
		ChimeraCharacter other = ChimeraCharacter.Cast(GetGame().SpawnEntityPrefab(characterResource, GetGame().GetWorld(), params));
		Check(other != null, "independent empty-slot character spawns");
		IEntity secondWeapon;
		if (other)
		{
			CharacterWeaponManagerComponent manager = CharacterWeaponManagerComponent.Cast(other.FindComponent(CharacterWeaponManagerComponent));
			array<WeaponSlotComponent> slots = {};
			manager.GetWeaponsSlots(slots);
			WeaponSlotComponent primary;
			WeaponSlotComponent secondary;
			foreach (WeaponSlotComponent slot : slots)
			{
				if (slot.GetWeaponSlotIndex() == 0)
					primary = slot;
				if (slot.GetWeaponSlotIndex() == 1)
					secondary = slot;
			}
			Resource secondResource = Resource.Load("{B31929F65F0D0279}Prefabs/Weapons/Rifles/M14/Rifle_M21.et");
			secondWeapon = GetGame().SpawnEntityPrefab(secondResource, GetGame().GetWorld(), params);
			if (secondary && secondWeapon)
			{
				manager.SetSlotWeapon(secondary, secondWeapon);
				manager.SelectWeapon(secondary);
			}
			Check(primary && !primary.GetWeaponEntity() && secondary && secondary.GetWeaponEntity(), "fixture has empty primary and a populated secondary slot");
			gadget.OnModeChanged(EGadgetMode.IN_HAND, other);
			Check(notepad.GetData().m_sError == "#MRAD_Error_NoWeapon", "empty first slot produces a localized error despite secondary weapon");
			Check(notepad.GetNumberOfPages() == 1 && notepad.GetPageIndex() == 0, "empty data clamps the authority page to zero");
			gadget.OnModeChanged(EGadgetMode.IN_HAND, character);
			Check(notepad.GetData().m_sError.IsEmpty(), "returning to the first-slot weapon restores the table");
			SCR_EntityHelper.DeleteEntityAndChildren(other);
		}
		if (secondWeapon)
			SCR_EntityHelper.DeleteEntityAndChildren(secondWeapon);

		m_iFailures += MRAD_NotebookInventoryChecks.Run(notepad, gadget, character);

		for (int cycle = 0; cycle < 100; cycle++)
		{
			gadget.OnModeChanged(EGadgetMode.IN_STORAGE, character);
			gadget.OnModeChanged(EGadgetMode.IN_HAND, character);
		}
		Check(notepad.IsHeld(), "100 close/open cycles complete");
		gadget.OnModeChanged(EGadgetMode.ON_GROUND, null);
		Check(!notepad.IsHeld() && !notepad.HasRenderTarget(), "drop clears owner, polling and render target");
		SCR_EntityHelper.DeleteEntityAndChildren(item);
		PrintFormat("MRAD_NOTEBOOK_RESULT checks=%1 failures=%2", m_iChecks, m_iFailures);
		return m_iFailures;
	}

	static int Run(ChimeraCharacter character)
	{
		if (!character || !Replication.IsServer())
		{
			Print("MRAD_NOTEBOOK_FAIL authority character unavailable", LogLevel.ERROR);
			return 1;
		}
		MRAD_NotebookProbe probe = new MRAD_NotebookProbe();
		return probe.CheckNotebook(character);
	}
}

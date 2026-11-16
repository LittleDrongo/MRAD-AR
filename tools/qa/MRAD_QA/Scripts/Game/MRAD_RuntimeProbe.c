// Development diagnostics for the installed engine. Real native shots require -mradNativeFire.
class MRAD_RuntimeProbeClass : GenericEntityClass
{
}

class MRAD_RuntimeProbe : GenericEntity
{
	[Attribute("{B31929F65F0D0279}Prefabs/Weapons/Rifles/M14/Rifle_M21.et", UIWidgets.ResourceNamePicker, "Isolated QA weapon prefab", "et")]
	protected ResourceName m_sTestWeapon;
	[Attribute("0", UIWidgets.CheckBox, "Require a real registered suppressor for the coefficient test")]
	protected bool m_bRequireSuppressor;
	protected int m_iValidRows;
	protected int m_iFailures;
	protected IEntity m_Weapon;
	protected IEntity m_Projectile;
	protected ChimeraCharacter m_Character;
	protected WeaponSlotComponent m_PrimarySlot;
	protected int m_iChamberAttempts;
	protected ref MRAD_NativeFireProbe m_NativeFireProbe;

	void MRAD_RuntimeProbe(IEntitySource src, IEntity parent)
	{
		SetEventMask(EntityEvent.INIT);
	}

	override void EOnInit(IEntity owner)
	{
		if (SCR_Global.IsEditMode())
			return;
		GetGame().GetCallqueue().CallLater(RunProbe, 1000, false);
	}

	protected void Failure(string reason)
	{
		m_iFailures++;
		Print("MRAD_RUNTIME_FAILURE " + reason, LogLevel.ERROR);
	}

	protected IEntity SpawnForProbe(ResourceName prefab)
	{
		Resource resource = Resource.Load(prefab);
		if (!resource || !resource.IsValid())
			return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[3] = GetOrigin() + Vector(0, 2, 0);
		return GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
	}

	protected IEntityComponentSource FindProjectileMoveSource(IEntitySource source)
	{
		if (!source)
			return null;
		for (int i = 0; i < source.GetComponentCount(); i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component && component.GetClassName().ToType().IsInherited(ProjectileMoveComponent))
				return component;
		}
		return null;
	}

	protected void CompareRows(string label, BaseMuzzleComponent muzzle, IEntitySource projectileSource, BaseWeaponStatsManagerComponent stats, int muzzleIndex)
	{
		float muzzleCoefficient = muzzle.GetBulletInitSpeedCoef();
		float attachmentCoefficient = 1.0;
		bool coefficientAvailable;
		if (stats)
			coefficientAvailable = stats.GetMuzzleVelocityCoefficient(muzzleIndex, attachmentCoefficient);
		if (!coefficientAvailable)
			attachmentCoefficient = 1.0;
		float nativeInitialSpeed;
		if (m_PrimarySlot)
			nativeInitialSpeed = m_PrimarySlot.GetInitialProjectileSpeed();
		PrintFormat("MRAD_RUNTIME_COEFFICIENT label=%1 muzzle=%2 attachment=%3 present=%4 suppressed=%5 chambered=%6 initialSpeed=%7", label, muzzleCoefficient, attachmentCoefficient, coefficientAvailable, muzzle.IsMuzzleSuppressed(), muzzle.IsCurrentBarrelChambered(), nativeInitialSpeed);
		array<int> distances = {100, 300, 700, 1000};
		foreach (int distance : distances)
		{
			float nativeTime, sourceTime, productTime;
			float nativeHeight = BallisticTable.GetAimHeightOfNextProjectile(distance, nativeTime, muzzle, true);
			float sourceHeight = BallisticTable.GetHeightFromProjectileSource(distance, sourceTime, projectileSource, muzzleCoefficient, true);
			float productHeight = BallisticTable.GetHeightFromProjectileSource(distance, productTime, projectileSource, muzzleCoefficient * attachmentCoefficient, true);
			PrintFormat("MRAD_RUNTIME_DROP label=%1 range=%2 nativeHeight=%3 nativeTime=%4 sourceHeight=%5 sourceTime=%6 productHeight=%7 productTime=%8", label, distance, nativeHeight, nativeTime, sourceHeight, sourceTime, productHeight, productTime);
			if (nativeTime > 0 && sourceTime > 0 && productTime > 0)
				m_iValidRows++;
		}
	}

	protected void ProbeWind(ResourceName projectileName, IEntitySource projectileSource, float speedCoefficient)
	{
		IEntityComponentSource component = FindProjectileMoveSource(projectileSource);
		float initialSpeed;
		if (!component || !component.Get("InitSpeed", initialSpeed) || initialSpeed <= 0)
		{
			Failure("projectile InitSpeed unavailable");
			return;
		}
		m_Projectile = SpawnForProbe(projectileName);
		if (!m_Projectile)
		{
			Failure("projectile probe spawn failed");
			return;
		}
		ProjectileMoveComponent movement = ProjectileMoveComponent.Cast(m_Projectile.FindComponent(ProjectileMoveComponent));
		if (!movement)
		{
			Failure("projectile movement unavailable");
			return;
		}
		ResourceName windConfig;
		component.Get("ProjectileWindTableConfig", windConfig);
		PrintFormat("MRAD_RUNTIME_PROJECTILE prefab=%1 initSpeed=%2 muzzleCoefficient=%3 bulletCoefficient=%4 windConfig=%5", projectileName, initialSpeed, speedCoefficient, movement.GetBulletSpeedCoef(), windConfig);
		float velocity = initialSpeed * speedCoefficient * movement.GetBulletSpeedCoef();
		// API angles are degrees. Native authoring tool uses +X as crosswind, +Z as travel.
		vector noWind = movement.GetProjectileSimulationResult(vector.Zero, velocity, 1.0, maxSimulationTime: 10.0, maxHorizontalDistance: 1000.0);
		vector crossWind = movement.GetProjectileSimulationResult(vector.Zero, velocity, 1.0, windSpeed: Vector(4, 0, 0), maxSimulationTime: 10.0, maxHorizontalDistance: 1000.0);
		PrintFormat("MRAD_RUNTIME_SIMULATION nowind=%1 crosswind4=%2", noWind, crossWind);
		if (noWind[2] <= 1.0 || crossWind[2] <= 1.0)
			Print("MRAD_RUNTIME_SIMULATION_UNAVAILABLE native debug prerequisite or invalid result", LogLevel.WARNING);
		else
			PrintFormat("MRAD_RUNTIME_SIMULATION_AVAILABLE driftX=%1 traveledZ=%2", crossWind[0] - noWind[0], crossWind[2]);
	}

	protected IEntity FindSuppressor(IEntity entity, out SCR_WeaponAttachmentSuppressorAttributes suppressorAttributes)
	{
		suppressorAttributes = null;
		if (!entity)
			return null;
		InventoryItemComponent item = InventoryItemComponent.Cast(entity.FindComponent(InventoryItemComponent));
		if (item)
		{
			ItemAttributeCollection attributes = item.GetAttributes();
			if (attributes)
				suppressorAttributes = SCR_WeaponAttachmentSuppressorAttributes.Cast(attributes.FindAttribute(SCR_WeaponAttachmentSuppressorAttributes));
			if (suppressorAttributes)
				return entity;
		}
		IEntity child = entity.GetChildren();
		while (child)
		{
			IEntity found = FindSuppressor(child, suppressorAttributes);
			if (found)
				return found;
			child = child.GetSibling();
		}
		return null;
	}

	protected void RunProbe()
	{
#ifdef ENABLE_DIAG
		PrintFormat("MRAD_RUNTIME_BEGIN enableDiag=1 consoleApp=%1", System.IsConsoleApp());
#else
		PrintFormat("MRAD_RUNTIME_BEGIN enableDiag=0 consoleApp=%1", System.IsConsoleApp());
#endif
		m_iFailures += MRAD_OpticsProbe.Run();
		PrintFormat("MRAD_RUNTIME_PREFAB %1", m_sTestWeapon);
		m_Weapon = SpawnForProbe(m_sTestWeapon);
		if (!m_Weapon)
		{
			Failure("QA weapon spawn failed");
			Finish();
			return;
		}
		m_Character = ChimeraCharacter.Cast(SpawnForProbe("{37578B1666981FCE}Prefabs/Characters/Core/Character_Base.et"));
		CharacterWeaponManagerComponent manager;
		if (m_Character)
			manager = CharacterWeaponManagerComponent.Cast(m_Character.FindComponent(CharacterWeaponManagerComponent));
		if (manager)
		{
			array<WeaponSlotComponent> slots = {};
			manager.GetWeaponsSlots(slots);
			foreach (WeaponSlotComponent slot : slots)
			{
				if (slot && slot.GetWeaponSlotIndex() == 0)
				{
					m_PrimarySlot = slot;
					break;
				}
			}
			if (m_PrimarySlot)
			{
				manager.SetSlotWeapon(m_PrimarySlot, m_Weapon);
				bool selected = manager.SelectWeapon(m_PrimarySlot);
				PrintFormat("MRAD_RUNTIME_EQUIP selected=%1 weapon=%2", selected, m_PrimarySlot.GetWeaponEntity());
				GetGame().GetCallqueue().CallLater(WaitForChamber, 3000, false);
				return;
			}
		}
		Failure("QA character primary slot unavailable");
		ContinueProbe();
	}

	protected void WaitForChamber()
	{
		BaseMuzzleComponent muzzle;
		if (m_PrimarySlot)
			muzzle = m_PrimarySlot.GetCurrentMuzzle();
		if (muzzle && !muzzle.IsCurrentBarrelChambered() && m_iChamberAttempts < 2)
		{
			CharacterControllerComponent controller = CharacterControllerComponent.Cast(m_Character.FindComponent(CharacterControllerComponent));
			if (controller)
			{
				m_iChamberAttempts++;
				bool requested = controller.ReloadWeapon();
				PrintFormat("MRAD_RUNTIME_CHAMBER requested=%1 attempt=%2", requested, m_iChamberAttempts);
				GetGame().GetCallqueue().CallLater(WaitForChamber, 3500, false);
				return;
			}
		}
		ContinueProbe();
	}

	protected void ContinueProbe()
	{
		BaseWeaponComponent weapon = BaseWeaponComponent.Cast(m_Weapon.FindComponent(BaseWeaponComponent));
		if (m_PrimarySlot)
			weapon = m_PrimarySlot;
		BaseMuzzleComponent muzzle;
		if (weapon)
			muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle)
		{
			Failure("QA weapon current muzzle unavailable");
			Finish();
			return;
		}
		BaseMagazineComponent magazine = muzzle.GetMagazine();
		if (!magazine)
		{
			Failure("QA weapon loaded magazine unavailable");
			Finish();
			return;
		}
		ResourceName projectileName = magazine.GetAmmoType(0);
		Resource projectileResource = Resource.Load(projectileName);
		if (!projectileResource || !projectileResource.IsValid())
		{
			Failure("loaded magazine projectile resource unavailable");
			Finish();
			return;
		}
		IEntitySource projectileSource = projectileResource.GetResource().ToEntitySource();
		if (!projectileSource)
		{
			Failure("loaded magazine projectile source unavailable");
			Finish();
			return;
		}
		array<BaseMuzzleComponent> muzzles = {};
		weapon.GetMuzzlesList(muzzles);
		int muzzleIndex = muzzles.Find(muzzle);
		BaseWeaponStatsManagerComponent stats = BaseWeaponStatsManagerComponent.Cast(m_Weapon.FindComponent(BaseWeaponStatsManagerComponent));
		GenericComponent weaponTree = GenericComponent.Cast(m_Weapon.FindComponent(BaseWeaponComponent));
		BaseWeaponStatsManagerComponent nestedStats;
		if (weaponTree)
			nestedStats = BaseWeaponStatsManagerComponent.Cast(weaponTree.FindComponent(BaseWeaponStatsManagerComponent));
		PrintFormat("MRAD_RUNTIME_STATS_LOOKUP entity=%1 weaponComponent=%2 nested=%3", stats, weaponTree, nestedStats);
		if (!stats)
			stats = nestedStats;
		PrintFormat("MRAD_RUNTIME_WEAPON muzzles=%1 index=%2 stats=%3 initialSpeed=%4", muzzles.Count(), muzzleIndex, stats, weapon.GetInitialProjectileSpeed());
		if (stats)
			PrintFormat("MRAD_RUNTIME_STATS muzzleCount=%1", stats.GetNumberOfMuzzles());
		CompareRows("baseline", muzzle, projectileSource, stats, muzzleIndex);
		SCR_WeaponAttachmentSuppressorAttributes suppressorAttributes;
		IEntity suppressor = FindSuppressor(m_Weapon, suppressorAttributes);
		if (stats && muzzleIndex >= 0 && suppressor && suppressorAttributes)
		{
			// Native manager accepts only a registered attachment as the modifier key.
			// Restore from its real attributes, not from a guessed aggregate coefficient.
			bool applied = stats.SetMuzzleVelocityCoefficient(suppressor, muzzleIndex, 0.8);
			PrintFormat("MRAD_RUNTIME_MODIFIER registeredSuppressor=%1 applied=%2", suppressor, applied);
			if (applied)
			{
				CompareRows("registered_suppressor_0.8", muzzle, projectileSource, stats, muzzleIndex);
				bool restored = suppressorAttributes.ApplyModifiers(stats, muzzleIndex, suppressor);
				PrintFormat("MRAD_RUNTIME_MODIFIER_RESTORE restored=%1", restored);
				if (!restored)
					Failure("registered suppressor attributes could not be restored");
				CompareRows("after_attribute_restore", muzzle, projectileSource, stats, muzzleIndex);
			}
			else
				Failure("registered suppressor coefficient was rejected");
		}
		else if (m_bRequireSuppressor)
			Failure("required registered suppressor or stats manager unavailable");
		else
			Print("MRAD_RUNTIME_MODIFIER_SKIP baseline weapon has no registered suppressor");
		ProbeWind(projectileName, projectileSource, muzzle.GetBulletInitSpeedCoef());
		CheckCalculator();
		m_iFailures += MRAD_NotebookProbe.Run(m_Character);
		m_iFailures += MRAD_SettingsProbe.Run(true);
		if (System.IsCLIParam("mradNativeFire"))
		{
			m_NativeFireProbe = new MRAD_NativeFireProbe();
			m_NativeFireProbe.Start(this, m_Character, stats, muzzleIndex, suppressor, suppressorAttributes);
			return;
		}
		Finish();
	}

	void OnNativeFireFinished(int failures)
	{
		m_iFailures += failures;
		Finish();
	}

	protected void CheckCalculator()
	{
		MRAD_RangeCardInput input = MRAD_RangeCardInputResolver.Resolve(m_Character);
		MRAD_RangeCardData calm = MRAD_RangeCardCalculator.Build(input, 1000, 0);
		MRAD_RangeCardData right = MRAD_RangeCardCalculator.Build(input, 1000, 4);
		MRAD_RangeCardData left = MRAD_RangeCardCalculator.Build(input, 1000, -4);
		PrintFormat("MRAD_RUNTIME_CALCULATOR calmRows=%1 rightRows=%2 leftRows=%3 error=%4 estimated=%5 missing=%6", calm.m_aRows.Count(), right.m_aRows.Count(), left.m_aRows.Count(), right.m_sError, right.m_bEstimatedWind, right.m_bMissingWind);
		if (calm.m_aRows.Count() != 10 || right.m_aRows.Count() != 10 || left.m_aRows.Count() != 10)
		{
			Failure("calculator should produce ten valid QA weapon rows through 1000m");
			return;
		}
		foreach (int i, MRAD_RangeCardRow row : right.m_aRows)
		{
			float time;
			float height = BallisticTable.GetHeightFromProjectileSource(row.m_iRange, time, input.m_Source, input.m_fSpeedCoef, true);
			float expected = SCR_Math.ConvertFromRadians(Math.Atan2(height, row.m_iRange), SCR_EOpticsAngleUnits.MILLIRADIANS);
			if (Math.AbsFloat(row.m_fDropMrad - expected) > 0.0001)
				Failure("calculator drop differs from same-input native source query");
			if (Math.AbsFloat(calm.m_aRows[i].m_fWindMrad) > 0.0001 || !row.m_bHasWind || !left.m_aRows[i].m_bHasWind || Math.AbsFloat(row.m_fWindMrad + left.m_aRows[i].m_fWindMrad) > 0.0001)
				Failure("calculator wind must handle calm and symmetric correction signs");
			PrintFormat("MRAD_RUNTIME_CARD range=%1 drop=%2 right4=%3 left4=%4", row.m_iRange, row.m_fDropMrad, row.m_fWindMrad, left.m_aRows[i].m_fWindMrad);
		}
	}

	protected void Finish()
	{
		if (m_iValidRows == 0 && m_iFailures == 0)
			Failure("no native/source comparison produced valid flight times");
		if (m_Projectile)
			delete m_Projectile;
		if (m_Weapon)
			delete m_Weapon;
		if (m_Character)
			delete m_Character;
		PrintFormat("MRAD_RUNTIME_DONE validRows=%1 failures=%2", m_iValidRows, m_iFailures);
		if (System.IsCLIParam("mradUIVisual") && MRAD_SettingsVisualProbe.Open())
		{
			GetGame().GetCallqueue().CallLater(CloseProbe, 10000, false);
			return;
		}
		GetGame().GetCallqueue().CallLater(CloseProbe, 100, false);
	}

	protected void CloseProbe()
	{
		MRAD_SettingsVisualProbe.Close();
		GetGame().RequestClose();
	}
}

// Development-only checks on actual spawned prefab instances. This does not
// replace the separate two-client ADS/impact acceptance test.
class MRAD_OpticsProbe
{
	protected int m_iChecks;
	protected int m_iFailures;
	protected ref array<IEntity> m_aEntities = {};

	protected void Check(bool passed, string name)
	{
		m_iChecks++;
		if (passed)
			Print("MRAD_OPTICS_PASS " + name);
		else
		{
			m_iFailures++;
			Print("MRAD_OPTICS_FAIL " + name, LogLevel.ERROR);
		}
	}

	protected IEntity LoadAndSpawn(ResourceName name, int index, string label)
	{
		Resource resource = Resource.Load(name);
		Check(resource && resource.IsValid() && resource.GetResource(), label + " resource loads");
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return null;
		IEntitySource source = resource.GetResource().ToEntitySource();
		Check(source != null, label + " is an entity resource");
		if (!source)
			return null;
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = Vector(index * 2, 10, 0);
		IEntity entity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), params);
		Check(entity != null, label + " instance spawns");
		if (entity)
			m_aEntities.Insert(entity);
		return entity;
	}

	protected MRAD_ZeroingStateComponent CheckOptic(IEntity entity, bool pip, string label)
	{
		if (!entity)
			return null;
		array<Managed> sights = {};
		array<Managed> states = {};
		entity.FindComponents(BaseSightsComponent, sights);
		entity.FindComponents(MRAD_ZeroingStateComponent, states);
		Check(sights.Count() == 1, label + " has exactly one sights component");
		Check(states.Count() == 1, label + " has exactly one per-instance state");
		BaseSightsComponent nativeSights = BaseSightsComponent.Cast(entity.FindComponent(BaseSightsComponent));
		if (nativeSights)
		{
			vector baseZero = nativeSights.GetCurrentSightsRange();
			PrintFormat("MRAD_OPTICS_NATIVE_RANGE label=%1 animation=%2 metres=%3", label, baseZero[0], baseZero[1]);
			Check(Math.AbsFloat(baseZero[1]) < 0.001, label + " has neutral vanilla metre zero");
		}
		MRAD_2DSightsComponent twoD = MRAD_2DSightsComponent.Cast(entity.FindComponent(MRAD_2DSightsComponent));
		MRAD_PIPSightsComponent picture = MRAD_PIPSightsComponent.Cast(entity.FindComponent(MRAD_PIPSightsComponent));
		if (pip)
			Check(picture && !twoD, label + " uses MRAD PIP class");
		else
		{
			Check(twoD && !picture, label + " uses MRAD 2D class");
			Check(!entity.FindComponent(SCR_2DPIPSightsComponent), label + " has no residual vanilla PIP component");
		}
		MRAD_ZeroingStateComponent state = MRAD_ZeroingStateComponent.Cast(entity.FindComponent(MRAD_ZeroingStateComponent));
		if (state)
		{
			RplId id = Replication.FindItemId(state);
			Check(id.IsValid(), label + " state has a replication item ID");
			Check(state.GetElevationTicks() == 0 && state.GetWindageTicks() == 0, label + " starts with zero extra correction");
		}
		return state;
	}

	protected void CollectStates(IEntity entity, array<MRAD_ZeroingStateComponent> states)
	{
		if (!entity)
			return;
		MRAD_ZeroingStateComponent state = MRAD_ZeroingStateComponent.Cast(entity.FindComponent(MRAD_ZeroingStateComponent));
		if (state)
			states.Insert(state);
		IEntity child = entity.GetChildren();
		while (child)
		{
			CollectStates(child, states);
			child = child.GetSibling();
		}
	}

	protected void CheckWeapon(IEntity entity, bool pip, string label)
	{
		if (!entity)
			return;
		Check(entity.FindComponent(BaseWeaponComponent) != null, label + " retains native weapon component");
		array<MRAD_ZeroingStateComponent> states = {};
		CollectStates(entity, states);
		Check(states.Count() == 1, label + " spawns exactly one MRAD scope attachment");
		if (states.Count() == 1)
			CheckOptic(states[0].GetOwner(), pip, label + " attached scope");
	}

	protected void CheckState(MRAD_ZeroingStateComponent first, MRAD_ZeroingStateComponent second)
	{
		if (!first || !second)
			return;
		Check(first != second, "two spawned optics have distinct state instances");
		if (!Replication.IsServer())
		{
			Print("MRAD_OPTICS_SKIP authority mutation checks require a server runtime", LogLevel.WARNING);
			return;
		}
		for (int i = 0; i < 10; i++)
			first.ApplyDeltaAuthority(1, 0, 500, 500, false);
		Check(first.GetElevationTicks() == 10 && first.GetWindageTicks() == 0, "ten fine changes accumulate as exact integer ticks");
		Check(second.GetElevationTicks() == 0 && second.GetWindageTicks() == 0, "mutating first optic leaves second optic unchanged");
		for (int j = 0; j < 3; j++)
			first.ApplyDeltaAuthority(0, 10, 500, 500, false);
		Check(first.GetWindageTicks() == Math.Min(30, first.GetWindageLimitTicks()), "three coarse changes obey the effective server limit");
		int beforeElevation = first.GetElevationTicks();
		int beforeWindage = first.GetWindageTicks();
		first.ApplyDeltaAuthority(1, 1, 500, 500, false);
		first.ApplyDeltaAuthority(101, 0, 500, 500, false);
		first.ApplyDeltaAuthority(0, 1, 501, 500, false);
		first.ApplyDeltaAuthority(1, 0, 500, 500, true);
		Check(first.GetElevationTicks() == beforeElevation && first.GetWindageTicks() == beforeWindage, "malformed dual-axis, oversized, invalid-limit and malformed-reset requests are rejected");
		for (int k = 0; k < 6; k++)
			first.ApplyDeltaAuthority(100, 0, 500, 500, false);
		Check(first.GetElevationTicks() == first.GetElevationLimitTicks() && first.GetElevationTicks() <= 500, "positive accumulation clamps at effective authority limit");
		for (int n = 0; n < 11; n++)
			first.ApplyDeltaAuthority(-100, 0, 500, 500, false);
		Check(first.GetElevationTicks() == -first.GetElevationLimitTicks(), "negative accumulation clamps symmetrically");
		first.ApplyDeltaAuthority(0, 0, 10, 10, false);
		Check(first.GetElevationTicks() == -10 && first.GetElevationLimitTicks() == 10 && first.GetWindageTicks() <= 10, "live lower limits reconcile without resetting the adjustment");
		vector transform[4];
		Math3D.AnglesToMatrix(vector.Zero, transform);
		transform[3] = Vector(12, 34, 56);
		first.ApplyCameraRotation(transform);
		Check(vector.Distance(transform[3], Vector(12, 34, 56)) < 0.00001, "instance camera correction preserves translation");
		first.ApplyDeltaAuthority(0, 0, 500, 500, true);
		Check(first.GetElevationTicks() == 0 && first.GetWindageTicks() == 0, "explicit reset clears both extra axes");
		Check(second.GetElevationTicks() == 0 && second.GetWindageTicks() == 0, "independent second optic remains unchanged after all requests");
	}

	protected int RunChecks()
	{
		if (!GetGame() || !GetGame().GetWorld())
		{
			Check(false, "runtime world is available for prefab checks");
			return m_iFailures;
		}
		IEntity optic2D = LoadAndSpawn("{A7DE1002B62C4801}Prefabs/Weapons/MRAD/Optics/MRAD_Optic_ARTII_2D.et", 0, "ARTII 2D");
		IEntity opticPIP = LoadAndSpawn("{A7DE1002B62C4802}Prefabs/Weapons/MRAD/Optics/MRAD_Optic_ARTII_PIP.et", 1, "ARTII PIP");
		// The mod owner deliberately removed the old sample rifle prefabs.
		// Exercise standalone scope prefabs only; do not restore/modify rifles.
		MRAD_ZeroingStateComponent first = CheckOptic(optic2D, false, "ARTII 2D");
		MRAD_ZeroingStateComponent second = CheckOptic(opticPIP, true, "ARTII PIP");
		CheckState(first, second);
		foreach (IEntity entity : m_aEntities)
		{
			if (entity)
				SCR_EntityHelper.DeleteEntityAndChildren(entity);
		}
		m_aEntities.Clear();
		PrintFormat("MRAD_OPTICS_RESULT checks=%1 failures=%2", m_iChecks, m_iFailures);
		return m_iFailures;
	}

	static int Run()
	{
		MRAD_OpticsProbe probe = new MRAD_OpticsProbe();
		return probe.RunChecks();
	}
}

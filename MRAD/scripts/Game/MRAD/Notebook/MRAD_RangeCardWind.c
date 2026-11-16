// Чтение native wind-конфига без опасного midId-1 из GetDataByAngle.
// Ни один массив здесь не обязан содержать 90 записей.
class MRAD_RangeCardWindEntry
{
	float m_fSpeedCoef;
	float m_fWindSpeed;
	ref array<vector> m_aSolutions = {};
	ref array<vector> m_aValues = {};

	bool TryAngle(float angleRad, out float correctionMrad)
	{
		correctionMrad = 0;
		if (m_aSolutions.IsEmpty() || m_aSolutions.Count() != m_aValues.Count())
			return false;
		int below = -1;
		int above = -1;
		float belowAngle = -1.0;
		float aboveAngle = 100.0;
		for (int i = 0; i < m_aSolutions.Count(); i++)
		{
			float sampleAngle = m_aSolutions[i][0];
			if (sampleAngle < 0 || m_aSolutions[i][1] <= 0)
				continue;
			if (Math.AbsFloat(sampleAngle - angleRad) <= 0.000001)
			{
				correctionMrad = m_aValues[i][0];
				return true;
			}
			if (sampleAngle < angleRad && sampleAngle > belowAngle)
			{
				below = i;
				belowAngle = sampleAngle;
			}
			if (sampleAngle > angleRad && sampleAngle < aboveAngle)
			{
				above = i;
				aboveAngle = sampleAngle;
			}
		}
		// Не экстраполируем ниже первого или выше последнего образца.
		if (below < 0 || above < 0 || aboveAngle <= belowAngle)
			return false;
		float fraction = (angleRad - belowAngle) / (aboveAngle - belowAngle);
		correctionMrad = m_aValues[below][0] + (m_aValues[above][0] - m_aValues[below][0]) * fraction;
		return true;
	}
}

class MRAD_RangeCardWind
{
	protected ref array<ref MRAD_RangeCardWindEntry> m_aEntries = {};
	protected IEntity m_Probe;
	protected ProjectileMoveComponent m_Movement;
	protected bool m_bSimulationUnavailable;
	protected ref MRAD_GameShellWindModel m_GameModel;

	void Load(IEntitySource source)
	{
		m_aEntries.Clear();
		m_GameModel = new MRAD_GameShellWindModel();
		m_GameModel.Load(source);
		if (!source)
			return;
		ResourceName windResource;
		for (int i = 0; i < source.GetComponentCount(); i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (!component || !component.GetClassName().ToType().IsInherited(ProjectileMoveComponent))
				continue;
			component.Get("ProjectileWindTableConfig", windResource);
			break;
		}
		if (windResource.IsEmpty())
			return;
		Resource resource = BaseContainerTools.LoadContainer(windResource);
		if (!resource || !resource.IsValid() || !resource.GetResource())
			return;
		BaseContainer container = resource.GetResource().ToBaseContainer();
		if (!container)
			return;
		BaseContainerList entries = container.GetObjectArray("m_aData");
		if (!entries)
			return;
		for (int entryIndex = 0; entryIndex < entries.Count(); entryIndex++)
		{
			BaseContainer sourceEntry = entries.Get(entryIndex);
			if (!sourceEntry)
				continue;
			MRAD_RangeCardWindEntry entry = new MRAD_RangeCardWindEntry();
			sourceEntry.Get("m_fInitSpeedCoef", entry.m_fSpeedCoef);
			sourceEntry.Get("m_fWindSpeed", entry.m_fWindSpeed);
			sourceEntry.Get("m_aFiringSolution", entry.m_aSolutions);
			sourceEntry.Get("m_aValues", entry.m_aValues);
			if (entry.m_fSpeedCoef > 0 && entry.m_fWindSpeed > 0 && !entry.m_aSolutions.IsEmpty() && entry.m_aSolutions.Count() == entry.m_aValues.Count())
				m_aEntries.Insert(entry);
		}
	}

	// signedCrosswind означает направление компенсации: + вправо, - влево.
	bool TryTable(float angleRad, float speedCoef, float signedCrosswind, out float correctionMrad)
	{
		correctionMrad = 0;
		float windMagnitude = Math.AbsFloat(signedCrosswind);
		if (windMagnitude <= 0.0001)
			return true;
		bool exactFound;
		float exactValue;
		float exactWindDifference = 100000.0;
		float belowCoef = -1;
		float aboveCoef = 100000.0;
		float belowValue, aboveValue;
		float belowWindDifference = 100000.0;
		float aboveWindDifference = 100000.0;
		foreach (MRAD_RangeCardWindEntry entry : m_aEntries)
		{
			float value;
			if (!entry.TryAngle(angleRad, value))
				continue;
			value *= signedCrosswind / entry.m_fWindSpeed;
			float windDifference = Math.AbsFloat(entry.m_fWindSpeed - windMagnitude);
			float coefficient = entry.m_fSpeedCoef;
			if (Math.AbsFloat(coefficient - speedCoef) <= 0.0001)
			{
				if (windDifference < exactWindDifference)
				{
					exactFound = true;
					exactValue = value;
					exactWindDifference = windDifference;
				}
			}
			else if (coefficient < speedCoef && (coefficient > belowCoef || (coefficient == belowCoef && windDifference < belowWindDifference)))
			{
				belowCoef = coefficient;
				belowValue = value;
				belowWindDifference = windDifference;
			}
			else if (coefficient > speedCoef && (coefficient < aboveCoef || (coefficient == aboveCoef && windDifference < aboveWindDifference)))
			{
				aboveCoef = coefficient;
				aboveValue = value;
				aboveWindDifference = windDifference;
			}
		}
		if (exactFound)
		{
			correctionMrad = exactValue;
			return true;
		}
		if (belowCoef <= 0 || aboveCoef >= 100000.0 || aboveCoef <= belowCoef)
			return false;
		correctionMrad = belowValue + (aboveValue - belowValue) * ((speedCoef - belowCoef) / (aboveCoef - belowCoef));
		return true;
	}

	protected bool EnsureSimulation(MRAD_RangeCardInput input)
	{
		if (m_bSimulationUnavailable)
			return false;
		if (m_Movement)
			return true;
		if (!GetGame() || !GetGame().GetWorld() || !input.m_Resource || !input.m_Resource.IsValid())
			return false;
		EntitySpawnParams params = new EntitySpawnParams();
		params.Transform[0] = vector.Right;
		params.Transform[1] = vector.Up;
		params.Transform[2] = vector.Forward;
		params.Transform[3] = Vector(0, -10000, 0);
		// Один временный, незапущенный снаряд на пересчёт; ни Launch, ни EnableSimulation.
		m_Probe = GetGame().SpawnEntityPrefab(input.m_Resource, GetGame().GetWorld(), params);
		if (m_Probe)
			m_Movement = ProjectileMoveComponent.Cast(m_Probe.FindComponent(ProjectileMoveComponent));
		if (!m_Movement)
		{
			m_bSimulationUnavailable = true;
			Release();
			return false;
		}
		return true;
	}

	bool TrySimulation(MRAD_RangeCardInput input, float distance, float angleRad, float flightTime, float signedCrosswind, out float correctionMrad)
	{
		correctionMrad = 0;
		if (!EnsureSimulation(input))
			return false;
		float speed = input.m_fInitSpeed * input.m_fSpeedCoef * m_Movement.GetBulletSpeedCoef();
		if (speed <= 0)
			return false;
		float angleDeg = SCR_Math.ConvertFromRadians(angleRad, SCR_EOpticsAngleUnits.DEGREES);
		float timeLimit = Math.Clamp(flightTime * 2.0 + 1.0, 2.0, 60.0);
		// В native API углы в градусах. Очень низкая targetHeight оставляет
		// горизонтальную дальность единственным нормальным условием остановки.
		vector result = m_Movement.GetProjectileSimulationResult(vector.Zero, speed, angleDeg, windSpeed: Vector(-signedCrosswind, 0, 0), targetHeight: -100000.0, maxSimulationTime: timeLimit, maxHorizontalDistance: distance);
		if (result[2] < distance * 0.995 || result[2] > distance + 5.0)
		{
			// В обычной сборке prediction может быть отключён. Нулевой результат
			// не превращаем в правдоподобную нулевую поправку.
			m_bSimulationUnavailable = true;
			return false;
		}
		correctionMrad = -SCR_Math.ConvertFromRadians(Math.Atan2(result[0], result[2]), SCR_EOpticsAngleUnits.MILLIRADIANS);
		return true;
	}

	// Runtime fallback uses the installed ShellMove prefab's movement parameters.
	// Coarse authored AI-table times are not accurate enough for this column.
	bool TryGameModel(MRAD_RangeCardInput input, float distance, float angleRad, float signedCrosswind, out float correctionMrad)
	{
		correctionMrad = 0;
		if (!input || !m_GameModel)
			return false;
		float speed = input.m_fInitSpeed * input.m_fSpeedCoef;
		if (m_Movement)
			speed *= m_Movement.GetBulletSpeedCoef();
		return m_GameModel.TryCorrection(distance, speed, angleRad, signedCrosswind, correctionMrad);
	}

	void Release()
	{
		m_Movement = null;
		if (m_Probe)
			delete m_Probe;
		m_Probe = null;
	}
}

class MRAD_RangeCardCalculator
{
	// +signedCrosswind = компенсация вправо. Опорный режим возвращает +W;
	// текущий ветер проектируется на горизонтальную ось вправо от взгляда.
	static float GetSignedCrosswind(ChimeraCharacter character, float referenceWind, bool useActual)
	{
		if (!useActual)
			return Math.Max(0.0, referenceWind);
		if (!character || !GetGame())
			return 0;
		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
			return 0;
		TimeAndWeatherManagerEntity weather = world.GetTimeAndWeatherManager();
		CharacterControllerComponent controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
		if (!weather || !controller)
			return 0;
		CharacterAimingComponent aiming = controller.GetAimingComponent();
		if (!aiming)
			return 0;
		vector forward = aiming.GetAimingDirectionWorld();
		return ProjectWindCorrection(forward, weather.GetWindDirection(), weather.GetWindSpeed());
	}

	// The native weather editor describes direction as where the wind comes FROM
	// (AR-Editor_Attribute_WindDirection_Description). Positive means aim right.
	static float ProjectWindCorrection(vector forward, float windFromDegrees, float windSpeed)
	{
		float horizontalLength = Math.Sqrt(forward[0] * forward[0] + forward[2] * forward[2]);
		if (horizontalLength <= 0.0001)
			return 0;
		vector right = Vector(forward[2] / horizontalLength, 0, -forward[0] / horizontalLength);
		float azimuth = SCR_Math.ConvertToRadians(windFromDegrees, SCR_EOpticsAngleUnits.DEGREES);
		vector windFrom = Vector(Math.Sin(azimuth), 0, Math.Cos(azimuth)) * Math.Max(0.0, windSpeed);
		return windFrom[0] * right[0] + windFrom[2] * right[2];
	}

	static MRAD_RangeCardData Build(MRAD_RangeCardInput input, int maxRange, float signedCrosswind)
	{
		MRAD_RangeCardData data = new MRAD_RangeCardData();
		data.m_fWindSpeed = Math.AbsFloat(signedCrosswind);
		data.m_sError = "#MRAD_Error_NoBallistics";
		if (!input)
			return data;
		data.m_sBulletName = input.m_sBulletName;
		data.m_sBulletDetail = input.m_sBulletDetail;
		data.m_sProjectile = input.m_sProjectile;
		data.m_bSuppressed = input.m_bSuppressed;
		if (!input.m_sError.IsEmpty())
		{
			data.m_sError = input.m_sError;
			return data;
		}
		if (!input.m_Source || input.m_fSpeedCoef <= 0)
			return data;

		maxRange = Math.ClampInt(maxRange, 100, 5000);
		MRAD_RangeCardWind wind = new MRAD_RangeCardWind();
		wind.Load(input.m_Source);
		for (int distance = 100; distance <= maxRange; distance += 100)
		{
			float flightTime;
			float aimHeight = BallisticTable.GetHeightFromProjectileSource(distance, flightTime, input.m_Source, input.m_fSpeedCoef, true);
			if (flightTime <= 0 || aimHeight <= 0)
				continue;
			float angleRad = Math.Atan2(aimHeight, distance);
			MRAD_RangeCardRow row = new MRAD_RangeCardRow();
			row.m_iRange = distance;
			row.m_fDropMrad = SCR_Math.ConvertFromRadians(angleRad, SCR_EOpticsAngleUnits.MILLIRADIANS);
			row.m_bHasWind = wind.TryTable(angleRad, input.m_fSpeedCoef, signedCrosswind, row.m_fWindMrad);
			if (!row.m_bHasWind)
				row.m_bHasWind = wind.TrySimulation(input, distance, angleRad, flightTime, signedCrosswind, row.m_fWindMrad);
			if (!row.m_bHasWind)
			{
				row.m_bHasWind = wind.TryGameModel(input, distance, angleRad, signedCrosswind, row.m_fWindMrad);
				if (row.m_bHasWind)
					data.m_bEstimatedWind = true;
			}
			if (!row.m_bHasWind)
				data.m_bMissingWind = true;
			data.m_aRows.Insert(row);
		}
		wind.Release();
		if (!data.m_aRows.IsEmpty())
			data.m_sError = string.Empty;
		return data;
	}
}

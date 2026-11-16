[ComponentEditorProps(category: "GameScripted/MRAD", description: "Replicated per-instance MRAD sight adjustment")]
class MRAD_ZeroingStateComponentClass : ScriptComponentClass
{
}

// Состояние принадлежит предмету прицела, а не общему ComponentClass.
class MRAD_ZeroingStateComponent : ScriptComponent
{
	static const int TICKS_PER_MRAD = 10;
	static const int MAX_LIMIT_TICKS = 500;
	static const int MAX_STEP_TICKS = 100;

	[Attribute("50", UIWidgets.EditBox, "Maximum extra elevation in MRAD", "0.1 50 0.1", category: "MRAD")]
	protected float m_fElevationLimitMrad;

	[Attribute("50", UIWidgets.EditBox, "Maximum extra windage in MRAD", "0.1 50 0.1", category: "MRAD")]
	protected float m_fWindageLimitMrad;

	[RplProp()]
	protected int m_iElevationTicks;

	[RplProp()]
	protected int m_iWindageTicks;

	[RplProp()]
	protected int m_iElevationLimitTicks = MAX_LIMIT_TICKS;

	[RplProp()]
	protected int m_iWindageLimitTicks = MAX_LIMIT_TICKS;

	int GetElevationTicks() { return m_iElevationTicks; }
	int GetWindageTicks() { return m_iWindageTicks; }
	int GetElevationLimitTicks() { return m_iElevationLimitTicks; }
	int GetWindageLimitTicks() { return m_iWindageLimitTicks; }
	float GetElevationMrad() { return m_iElevationTicks * 0.1; }
	float GetWindageMrad() { return m_iWindageTicks * 0.1; }

	static int ToTicks(float mrad)
	{
		return Math.Round(mrad * TICKS_PER_MRAD);
	}

	static bool IsSupportedSight(BaseSightsComponent sights)
	{
		return MRAD_2DSightsComponent.Cast(sights) || MRAD_PIPSightsComponent.Cast(sights);
	}

	static MRAD_ZeroingStateComponent FindForSights(BaseSightsComponent sights)
	{
		if (!sights || !IsSupportedSight(sights))
			return null;

		IEntity owner = sights.GetOwner();
		if (!owner)
			return null;

		return MRAD_ZeroingStateComponent.Cast(owner.FindComponent(MRAD_ZeroingStateComponent));
	}

	// Вызывается только проверенным серверным обработчиком PlayerController.
	bool ApplyDeltaAuthority(int elevationDelta, int windageDelta, int elevationLimit, int windageLimit, bool reset)
	{
		if (!Replication.IsServer())
			return false;

		if (elevationLimit < 1 || elevationLimit > MAX_LIMIT_TICKS || windageLimit < 1 || windageLimit > MAX_LIMIT_TICKS)
			return false;

		if (reset)
		{
			if (elevationDelta != 0 || windageDelta != 0)
				return false;
		}
		else
		{
			// Одна ось либо нулевая дельта для применения новых пределов.
			// Никаких абсолютных client углов.
			if (elevationDelta != 0 && windageDelta != 0)
				return false;
			if (elevationDelta < -MAX_STEP_TICKS || elevationDelta > MAX_STEP_TICKS || windageDelta < -MAX_STEP_TICKS || windageDelta > MAX_STEP_TICKS)
				return false;
		}

		MRAD_Settings settings = MRAD_Settings.Get();
		int serverElevationLimit = Math.ClampInt(ToTicks(settings.GetElevationLimitMrad()), 1, MAX_LIMIT_TICKS);
		int serverWindageLimit = Math.ClampInt(ToTicks(settings.GetWindageLimitMrad()), 1, MAX_LIMIT_TICKS);
		int prefabElevationLimit = Math.ClampInt(ToTicks(m_fElevationLimitMrad), 1, MAX_LIMIT_TICKS);
		int prefabWindageLimit = Math.ClampInt(ToTicks(m_fWindageLimitMrad), 1, MAX_LIMIT_TICKS);
		elevationLimit = Math.Min(elevationLimit, Math.Min(serverElevationLimit, prefabElevationLimit));
		windageLimit = Math.Min(windageLimit, Math.Min(serverWindageLimit, prefabWindageLimit));

		int elevation = 0;
		int windage = 0;
		if (!reset)
		{
			elevation = Math.ClampInt(m_iElevationTicks + elevationDelta, -elevationLimit, elevationLimit);
			windage = Math.ClampInt(m_iWindageTicks + windageDelta, -windageLimit, windageLimit);
		}

		if (elevation == m_iElevationTicks && windage == m_iWindageTicks && elevationLimit == m_iElevationLimitTicks && windageLimit == m_iWindageLimitTicks)
			return true;

		m_iElevationTicks = elevation;
		m_iWindageTicks = windage;
		m_iElevationLimitTicks = elevationLimit;
		m_iWindageLimitTicks = windageLimit;
		Replication.BumpMe();
		return true;
	}

	// Добавочная ориентация в локальных осях камеры; позиция и vanilla zeroing сохраняются.
	void ApplyCameraRotation(inout vector cameraTransform[4])
	{
		ApplyRotationTicks(m_iElevationTicks, m_iWindageTicks, cameraTransform);
	}

	// Чистая функция доступна для проверки матрицы штатным Workbench Math3D.
	static void ApplyRotationTicks(int elevationTicks, int windageTicks, inout vector cameraTransform[4])
	{
		if (elevationTicks == 0 && windageTicks == 0)
			return;

		vector position = cameraTransform[3];
		vector correction[4];
		float degreesPerTick = 0.0001 * Math.RAD2DEG;
		// Math3D использует yaw/pitch/roll. Положительная поправка смещает LOS обратно.
		Math3D.AnglesToMatrix(Vector(-windageTicks * degreesPerTick, -elevationTicks * degreesPerTick, 0), correction);
		Math3D.MatrixMultiply3(cameraTransform, correction, cameraTransform);
		cameraTransform[3] = position;
	}

	// UI также должен считать эту добавочную ориентацию частью самой оптики,
	// иначе штатная компенсация misalignment способна вернуть сетку к старой LOS.
	vector ApplyReticleReferenceRotation(vector misalignmentAngles)
	{
		if (m_iElevationTicks == 0 && m_iWindageTicks == 0)
			return misalignmentAngles;

		vector relative[4];
		Math3D.AnglesToMatrix(misalignmentAngles, relative);
		ApplyCameraRotation(relative);
		return SCR_Math3D.FixEulerVector180(Math3D.MatrixToAngles(relative));
	}
}

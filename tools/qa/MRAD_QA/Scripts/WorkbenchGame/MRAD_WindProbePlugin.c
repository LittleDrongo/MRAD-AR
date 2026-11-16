#ifdef WORKBENCH
// Diagnostic alternatives identify a possible native fixed-step convention.
// These candidates never affect production output or the pass/fail tolerance.
class MRAD_WindStepProbeModel : MRAD_GameShellWindModel
{
	bool TryStep(float distance, float speed, float angle, float signedWind, float dt, int scheme, out float correction)
	{
		correction = 0;
		if (!m_bSupported)
			return false;
		vector wind = Vector(-signedWind, 0, 0);
		vector velocity = Vector(0, Math.Sin(angle) * speed, Math.Cos(angle) * speed);
		vector position;
		for (int step = 0; step < 12000 && step * dt < 60.0; step++)
		{
			vector nextVelocity = velocity + Acceleration(velocity, wind) * dt;
			vector nextPosition = position + velocity * dt;
			if (scheme == 1)
				nextPosition = position + nextVelocity * dt;
			else if (scheme == 2)
				nextPosition = position + (velocity + nextVelocity) * (0.5 * dt);
			if (nextPosition[2] >= distance)
			{
				vector atDistance = vector.Lerp(position, nextPosition, (distance - position[2]) / (nextPosition[2] - position[2]));
				correction = -SCR_Math.ConvertFromRadians(Math.Atan2(atDistance[0], distance), SCR_EOpticsAngleUnits.MILLIRADIANS);
				return true;
			}
			if (nextVelocity[2] <= 0)
				return false;
			velocity = nextVelocity;
			position = nextPosition;
		}
		return false;
	}
}

// Development comparison against the installed game's own diagnostic predictor.
// Uses the same WorldEditor path as SCR_ProjectileWindageDataGeneratorPlugin.
// Never launches a projectile, edits a prefab, or writes a wind configuration.
[WorkbenchPluginAttribute(name: "MRAD native wind comparison", wbModules: {"WorldEditor"})]
class MRAD_WindProbePlugin : WorldEditorPlugin
{
	protected int m_iSamples;
	protected int m_iValid;
	protected int m_iFailures;
	protected int m_iUnavailable;
	protected float m_fMaxError;
	protected IEntity m_Projectile;
	protected ref Resource m_ProjectileResource;
	protected ref MRAD_GameShellWindModel m_GameModel;
	protected ref MRAD_WindStepProbeModel m_StepProbe;
	protected ResourceName m_sFixture;
	protected const int EXPECTED_SAMPLES = 360;

	protected void Fail(string reason)
	{
		m_iFailures++;
		Print("MRAD_WIND_PROBE_FAIL " + reason, LogLevel.ERROR);
	}

	protected bool ReachedSampleDistance(vector point, float distance)
	{
		return point[2] >= distance * 0.995 && point[2] <= distance + 5.0;
	}

	protected void CompareSample(ProjectileMoveComponent movement, IEntitySource source, float initialSpeed, float coefficient, int distance, float signedWind)
	{
		m_iSamples++;
		float flightTime;
		float height = BallisticTable.GetHeightFromProjectileSource(distance, flightTime, source, coefficient, true);
		if (flightTime <= 0 || height <= 0)
		{
			Fail(string.Format("native row unavailable coefficient=%1 sample=%2", coefficient, distance));
			return;
		}
		float angle = Math.Atan2(height, distance);
		float angleDegrees = SCR_Math.ConvertFromRadians(angle, SCR_EOpticsAngleUnits.DEGREES);
		float speed = initialSpeed * coefficient * movement.GetBulletSpeedCoef();
		float timeLimit = Math.Clamp(flightTime * 2.0 + 1.0, 2.0, 60.0);
		vector calm = movement.GetProjectileSimulationResult(vector.Zero, speed, angleDegrees, targetHeight: -100000.0, maxSimulationTime: timeLimit, maxHorizontalDistance: distance);
		vector windy = movement.GetProjectileSimulationResult(vector.Zero, speed, angleDegrees, windSpeed: Vector(-signedWind, 0, 0), targetHeight: -100000.0, maxSimulationTime: timeLimit, maxHorizontalDistance: distance);
		if (!ReachedSampleDistance(calm, distance) || !ReachedSampleDistance(windy, distance))
		{
			m_iUnavailable++;
			Fail(string.Format("diagnostic predictor unavailable or incomplete coefficient=%1 sample=%2 wind=%3 calm=%4 windy=%5", coefficient, distance, signedWind, calm, windy));
			return;
		}
		float nativeCorrection = -SCR_Math.ConvertFromRadians(Math.Atan2(windy[0] - calm[0], windy[2]), SCR_EOpticsAngleUnits.MILLIRADIANS);
		float estimate;
		if (!m_GameModel.TryCorrection(distance, speed, angle, signedWind, estimate))
		{
			Fail("current fallback rejected a valid native sample");
			return;
		}
		m_iValid++;
		float error = Math.AbsFloat(estimate - nativeCorrection);
		m_fMaxError = Math.Max(m_fMaxError, error);
		bool passed = error <= 0.2;
		PrintFormat("MRAD_WIND_PROBE_SAMPLE coefficient=%1 sample=%2 wind=%3 native=%4 fallback=%5 error=%6 endpoint=%7 passed=%8", coefficient, distance, signedWind, nativeCorrection, estimate, error, windy, passed);
		if (!passed)
		{
			Fail(string.Format("fallback exceeds 0.2 MRAD game-reference tolerance coefficient=%1 sample=%2 wind=%3 error=%4", coefficient, distance, signedWind, error));
			if (coefficient == 1.0 && distance == 1500 && signedWind == 30.0)
				ProbeTimeSteps(distance, speed, angle, signedWind, nativeCorrection);
		}
	}

	protected void ProbeTimeSteps(float distance, float speed, float angle, float signedWind, float nativeCorrection)
	{
		array<float> steps = {0.005, 0.01, 0.02, 0.05, 0.016666667, 0.033333333};
		foreach (float dt : steps)
		{
			for (int scheme = 0; scheme < 3; scheme++)
			{
				float correction;
				if (m_StepProbe.TryStep(distance, speed, angle, signedWind, dt, scheme, correction))
					PrintFormat("MRAD_WIND_STEP_DIAGNOSTIC fixture=%1 scheme=%2 dt=%3 correction=%4 nativeError=%5", m_sFixture, scheme, dt, correction, Math.AbsFloat(correction - nativeCorrection));
			}
		}
	}

	protected void CompareFixture(WorldEditorAPI api, ResourceName projectileName)
	{
		if (m_Projectile)
			delete m_Projectile;
		m_Projectile = null;
		m_sFixture = projectileName;
		m_ProjectileResource = Resource.Load(projectileName);
		if (!m_ProjectileResource || !m_ProjectileResource.IsValid() || !m_ProjectileResource.GetResource())
		{
			Fail("installed game fixture resource cannot be loaded");
			return;
		}
		IEntitySource source = m_ProjectileResource.GetResource().ToEntitySource();
		float initialSpeed;
		if (!source || !MRAD_RangeCardInputResolver.ReadProjectileSpeed(source, initialSpeed))
		{
			Fail("installed fixture has no valid ProjectileMoveComponent source");
			return;
		}
		m_Projectile = GetGame().SpawnEntityPrefab(m_ProjectileResource, api.GetWorld());
		if (!m_Projectile)
		{
			Fail("fixture spawn in WorldEditor world failed");
			return;
		}
		ProjectileMoveComponent movement = ProjectileMoveComponent.Cast(m_Projectile.FindComponent(ProjectileMoveComponent));
		if (!movement)
		{
			Fail("spawned fixture has no ProjectileMoveComponent");
			return;
		}
		PrintFormat("MRAD_WIND_PROBE_FIXTURE resource=%1 bulletCoefficient=%2", projectileName, movement.GetBulletSpeedCoef());
		m_GameModel = new MRAD_GameShellWindModel();
		m_GameModel.Load(source);
		m_StepProbe = new MRAD_WindStepProbeModel();
		m_StepProbe.Load(source);
		array<float> coefficients = {1.0, 0.8, 1.01};
		array<float> signedWinds = {4.0, -4.0, 30.0, -30.0};
		foreach (float coefficient : coefficients)
		{
			foreach (float signedWind : signedWinds)
			{
				for (int distance = 100; distance <= 1500; distance += 100)
					CompareSample(movement, source, initialSpeed, coefficient, distance, signedWind);
			}
		}
	}

	protected void CheckEditorNotepad(WorldEditorAPI api)
	{
		Resource resource = Resource.Load("{2A7C01B93CC92ACE}Prefabs/Items/Equipment/RangeCard/RangeCard_Notepad_AUTO.et");
		if (!resource || !resource.IsValid())
		{
			Fail("editor AUTO notebook resource cannot be loaded");
			return;
		}
		IEntity entity = GetGame().SpawnEntityPrefab(resource, api.GetWorld());
		Drg_AutoNotepadRtComponent notepad;
		if (entity)
			notepad = Drg_AutoNotepadRtComponent.Cast(entity.FindComponent(Drg_AutoNotepadRtComponent));
		if (!notepad)
			Fail("editor AUTO notebook does not spawn its component");
		else
		{
			PrintFormat("MRAD_EDITOR_NOTEPAD_RESULT editMode=%1 hasRT=%2 held=%3", SCR_Global.IsEditMode(), notepad.HasRenderTarget(), notepad.IsHeld());
			if (!SCR_Global.IsEditMode() || notepad.HasRenderTarget() || notepad.IsHeld())
				Fail("editor notebook must stay unheld and allocate no render target");
		}
		if (entity)
			SCR_EntityHelper.DeleteEntityAndChildren(entity);
	}

	protected void Finish()
	{
		if (m_Projectile)
			delete m_Projectile;
		m_Projectile = null;
		m_ProjectileResource = null;
		if (m_iSamples != EXPECTED_SAMPLES || m_iValid != EXPECTED_SAMPLES)
			Fail("all 360 diagnostic comparisons are required; unavailable prediction is not a pass");
		PrintFormat("MRAD_WIND_PROBE_RESULT samples=%1 valid=%2 failures=%3 unavailable=%4 maxError=%5", m_iSamples, m_iValid, m_iFailures, m_iUnavailable, m_fMaxError);
		int result;
		if (m_iFailures > 0)
			result = 1;
		Workbench.Exit(result);
	}

	override void Run()
	{
		// The command-line entry opens a specific QA world and exits Workbench.
		// Prevent invoking that behavior in a user's existing editor session.
		Print("MRAD_WIND_PROBE_INFO use -wbModule=WorldEditor -plugin=MRAD_WindProbePlugin in an isolated QA profile");
	}

	override void RunCommandline()
	{
#ifdef ENABLE_DIAG
		Print("MRAD_WIND_PROBE_BEGIN worldEditor=1 ENABLE_DIAG=1");
#else
		Print("MRAD_WIND_PROBE_BEGIN worldEditor=1 ENABLE_DIAG=0");
#endif
		WorldEditor editor = Workbench.GetModule(WorldEditor);
		if (!editor)
		{
			Fail("WorldEditor module unavailable");
			Finish();
			return;
		}
		string worldPath;
		if (!Workbench.GetAbsolutePath("$MRAD_QA:Worlds/MRAD_QA/MRAD_QA.ent", worldPath) || !editor.SetOpenedResource(worldPath))
		{
			Fail("isolated QA world could not be opened");
			Finish();
			return;
		}
		WorldEditorAPI api = editor.GetApi();
		if (!api || !api.GetWorld())
		{
			Fail("WorldEditor API/world unavailable after opening QA scene");
			Finish();
			return;
		}
		CheckEditorNotepad(api);
		CompareFixture(api, "{B7DC0C3C3D1784C2}Prefabs/Weapons/Ammo/Ammo_762x51_Ball_M118.et");
		CompareFixture(api, "{AC26AB660097633D}Prefabs/Weapons/Ammo/Ammo_556x45_Ball_M855.et");
		Finish();
	}
}
#endif

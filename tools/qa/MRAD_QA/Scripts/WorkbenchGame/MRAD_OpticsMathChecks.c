// Actual engine Math3D contract checks. These are camera geometry tests,
// not multiplayer or projectile-impact evidence.
class MRAD_OpticsMathChecks
{
	protected int m_iChecks;
	protected int m_iFailures;

	protected void Check(bool passed, string name)
	{
		m_iChecks++;
		if (passed)
			Print("MRAD_OPTICS_MATH_PASS " + name);
		else
		{
			m_iFailures++;
			Print("MRAD_OPTICS_MATH_FAIL " + name, LogLevel.ERROR);
		}
	}

	protected bool NearVector(vector first, vector second, float tolerance = 0.00001)
	{
		return vector.Distance(first, second) <= tolerance;
	}

	protected int RunChecks()
	{
		vector transform[4];
		vector original[4];
		Math3D.AnglesToMatrix(Vector(23, -12, 7), original);
		original[3] = Vector(12, 34, 56);
		Math3D.MatrixCopy(original, transform);
		MRAD_ZeroingStateComponent.ApplyRotationTicks(0, 0, transform);
		Check(NearVector(transform[0], original[0]) && NearVector(transform[1], original[1]) && NearVector(transform[2], original[2]) && NearVector(transform[3], original[3]), "zero correction preserves the full native camera transform");
		Math3D.AnglesToMatrix(vector.Zero, transform);
		transform[3] = original[3];
		MRAD_ZeroingStateComponent.ApplyRotationTicks(10, 0, transform);
		vector angles = SCR_Math3D.FixEulerVector180(Math3D.MatrixToAngles(transform));
		Check(NearVector(angles, Vector(0, -0.001 * Math.RAD2DEG, 0)), "ten fine elevation ticks equal the expected local camera pitch");
		Check(NearVector(transform[3], original[3]), "pitch correction leaves camera translation unchanged");
		MRAD_ZeroingStateComponent.ApplyRotationTicks(-10, 0, transform);
		Check(NearVector(transform[0], vector.Right) && NearVector(transform[1], vector.Up) && NearVector(transform[2], vector.Forward), "inverse pitch corrections compose to identity");
		Math3D.AnglesToMatrix(vector.Zero, transform);
		MRAD_ZeroingStateComponent.ApplyRotationTicks(0, 30, transform);
		angles = SCR_Math3D.FixEulerVector180(Math3D.MatrixToAngles(transform));
		Check(NearVector(angles, Vector(-0.003 * Math.RAD2DEG, 0, 0)), "three coarse windage changes equal the expected local yaw");
		MRAD_ZeroingStateComponent.ApplyRotationTicks(0, -30, transform);
		Check(NearVector(transform[0], vector.Right) && NearVector(transform[1], vector.Up) && NearVector(transform[2], vector.Forward), "inverse yaw corrections compose to identity");
		Math3D.MatrixCopy(original, transform);
		MRAD_ZeroingStateComponent.ApplyRotationTicks(500, -500, transform);
		Check(NearVector(transform[3], original[3]), "combined maximum correction preserves the native camera position");
		Check(Math.AbsFloat(transform[0].Length() - 1.0) < 0.00001 && Math.AbsFloat(transform[1].Length() - 1.0) < 0.00001 && Math.AbsFloat(transform[2].Length() - 1.0) < 0.00001, "combined correction preserves unit basis lengths");
		PrintFormat("MRAD_OPTICS_MATH_RESULT checks=%1 failures=%2", m_iChecks, m_iFailures);
		return m_iFailures;
	}

	static int Run()
	{
		MRAD_OpticsMathChecks checks = new MRAD_OpticsMathChecks();
		return checks.RunChecks();
	}
}

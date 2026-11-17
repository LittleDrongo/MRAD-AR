// Fallback for the game's ordinary ShellMoveComponent only. Parameters come
// from the loaded prefab, never from external ammunition data or AI-table time.
// Other movement classes and invalid/incomplete sources remain unsupported.
class MRAD_GameShellWindModel
{
	protected bool m_bSupported;
	protected float m_fDragPerMass;
	// Trapezoidal integration. Lab 2026-11-16, M21/M118, real rounds launched by the engine and
	// the native predictor (Diag build only; the retail build returns zero):
	// - elevation: 60 Hz gives bore angles within 0.08 MRAD at 300/800/1500 m (30 Hz: 0.2-0.5 high);
	// - wind: 30 Hz matches the real drift within 0.01 MRAD (60 Hz: 0.06 low).
	static const float ELEVATION_STEP = 1.0 / 60.0;
	static const float WIND_STEP = 1.0 / 30.0;
	protected const int MAX_STEPS = 3601;
	protected const float MAX_TIME = 60.0;

	void Load(IEntitySource source)
	{
		m_bSupported = false;
		if (!source)
			return;
		for (int i = 0; i < source.GetComponentCount(); i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (!component || component.GetClassName() != "ShellMoveComponent")
				continue;
			float mass, drag;
			if (!component.Get("Mass", mass) || !component.Get("AirDrag", drag) || mass <= 0 || drag < 0)
				return;
			m_fDragPerMass = drag / mass;
			m_bSupported = true;
			return;
		}
	}

	protected vector Acceleration(vector velocity, vector wind)
	{
		vector relative = velocity - wind;
		return Physics.VGravity - relative * (relative.Length() * m_fDragPerMass);
	}

	bool IsSupported()
	{
		return m_bSupported;
	}

	//------------------------------------------------------------------------------------------------
	//! Bore angle (rad) that brings the round back onto a line of sight parallel to the bore
	//! and sightHeight metres above it, at this distance. Same physics as the flight itself.
	bool TryElevation(float distance, float initialSpeed, float guessRad, float sightHeight, out float angleRad)
	{
		angleRad = guessRad;
		for (int iteration = 0; iteration < 6; iteration++)
		{
			vector at;
			if (!TrySimulate(distance, initialSpeed, angleRad, 0, 0, at, ELEVATION_STEP))
				return false;

			// Height error against the line of sight; d(height)/d(angle) is about the distance.
			float error = at[1] - sightHeight;
			angleRad -= error / distance;
			if (Math.AbsFloat(error) < 0.001)
				return true;
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Position of the round when it has flown this horizontal distance: x lateral, y height.
	//! signedCrosswind: + blows the round to the left of the line of sight (turn right to correct);
	//! signedHeadwind: + blows towards the shooter and slows the round down.
	bool TrySimulate(float distance, float initialSpeed, float angleRad, float signedCrosswind, float signedHeadwind, out vector atDistance, float stepSeconds = WIND_STEP)
	{
		atDistance = vector.Zero;
		if (!m_bSupported || distance <= 0 || distance > 5000 || initialSpeed <= 0 || Math.AbsFloat(angleRad) >= Math.PI * 0.5)
			return false;
		vector wind = Vector(-signedCrosswind, 0, -signedHeadwind);
		vector velocity = Vector(0, Math.Sin(angleRad) * initialSpeed, Math.Cos(angleRad) * initialSpeed);
		vector position = vector.Zero;
		float elapsed;
		for (int step = 0; step < MAX_STEPS && elapsed < MAX_TIME; step++)
		{
			if (velocity[2] <= 0)
				return false;
			float dt = stepSeconds;
			vector relative = velocity - wind;
			if (m_fDragPerMass * relative.Length() * dt > 0.25)
				return false;
			dt = Math.Min(dt, MAX_TIME - elapsed);
			if (dt <= 0)
				return false;
			vector nextVelocity = velocity + Acceleration(velocity, wind) * dt;
			vector nextPosition = position + (velocity + nextVelocity) * (dt * 0.5);
			if (nextPosition[2] >= distance)
			{
				float fraction = (distance - position[2]) / (nextPosition[2] - position[2]);
				atDistance = vector.Lerp(position, nextPosition, fraction);
				return true;
			}
			position = nextPosition;
			velocity = nextVelocity;
			elapsed += dt;
		}
		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Lateral correction for a crosswind at the no-wind bore angle, in MRAD: + turn right.
	//! The lateral drift is linear in the wind, so the lateral step of the wind run is enough
	//! (the lab matched a launched round within 0.01 MRAD at 30 Hz).
	bool TryCross(float distance, float initialSpeed, float angleRad, float signedCrosswind, out float crossMrad)
	{
		crossMrad = 0;
		if (Math.AbsFloat(signedCrosswind) <= 0.0001)
			return true;

		vector at;
		if (!TrySimulate(distance, initialSpeed, angleRad, signedCrosswind, 0, at, WIND_STEP))
			return false;

		crossMrad = -SCR_Math.ConvertFromRadians(Math.Atan2(at[0], distance), SCR_EOpticsAngleUnits.MILLIRADIANS);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Headwind/tailwind correction alone. The vertical is integrated at the finer elevation
	//! step because a 1 m/s wind moves the impact by only centimetres, and one Newton step
	//! removes the quadratic part of the wind's effect (it matters for strong wind).
	bool TryHead(float distance, float initialSpeed, float angleRad, float signedHeadwind, out float headMrad)
	{
		headMrad = 0;
		if (Math.AbsFloat(signedHeadwind) <= 0.0001)
			return true;

		vector calm;
		if (!TrySimulate(distance, initialSpeed, angleRad, 0, 0, calm, ELEVATION_STEP))
			return false;

		vector at;
		if (!TrySimulate(distance, initialSpeed, angleRad, 0, signedHeadwind, at, ELEVATION_STEP))
			return false;

		// Reference height is the calm flight at the same distance: it is the sight line height.
		float correction = (calm[1] - at[1]) / distance;
		if (!TrySimulate(distance, initialSpeed, angleRad + correction, 0, signedHeadwind, at, ELEVATION_STEP))
			return false;

		correction += (calm[1] - at[1]) / distance;
		headMrad = SCR_Math.ConvertFromRadians(correction, SCR_EOpticsAngleUnits.MILLIRADIANS);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	bool TryCorrection(float distance, float initialSpeed, float angleRad, float signedWind, out float correctionMrad)
	{
		correctionMrad = 0;
		if (!m_bSupported || distance <= 0 || distance > 5000 || initialSpeed <= 0 || Math.AbsFloat(angleRad) >= Math.PI * 0.5)
			return false;
		if (Math.AbsFloat(signedWind) <= 0.0001 || m_fDragPerMass == 0)
			return true;

		float crossMrad;
		if (!TryCross(distance, initialSpeed, angleRad, signedWind, crossMrad))
			return false;
		correctionMrad = crossMrad;
		return true;
	}
}

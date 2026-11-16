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
			if (!TrySimulate(distance, initialSpeed, angleRad, 0, at, ELEVATION_STEP))
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
	bool TrySimulate(float distance, float initialSpeed, float angleRad, float signedWind, out vector atDistance, float stepSeconds = WIND_STEP)
	{
		atDistance = vector.Zero;
		if (!m_bSupported || distance <= 0 || distance > 5000 || initialSpeed <= 0 || Math.AbsFloat(angleRad) >= Math.PI * 0.5)
			return false;
		vector wind = Vector(-signedWind, 0, 0);
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
	bool TryCorrection(float distance, float initialSpeed, float angleRad, float signedWind, out float correctionMrad)
	{
		correctionMrad = 0;
		if (!m_bSupported || distance <= 0 || distance > 5000 || initialSpeed <= 0 || Math.AbsFloat(angleRad) >= Math.PI * 0.5)
			return false;
		if (Math.AbsFloat(signedWind) <= 0.0001 || m_fDragPerMass == 0)
			return true;
		vector wind = Vector(-signedWind, 0, 0);
		vector velocity = Vector(0, Math.Sin(angleRad) * initialSpeed, Math.Cos(angleRad) * initialSpeed);
		vector position = vector.Zero;
		float elapsed;
		for (int step = 0; step < MAX_STEPS && elapsed < MAX_TIME; step++)
		{
			if (velocity[2] <= 0)
				return false;
			float dt = WIND_STEP;
			vector relative = velocity - wind;
			float response = m_fDragPerMass * relative.Length();
			// Reject numerically unstable exotic sources instead of returning
			// a plausible value from an unsupported movement configuration.
			if (response * dt > 0.25)
				return false;
			dt = Math.Min(dt, MAX_TIME - elapsed);
			if (dt <= 0)
				return false;
			// Nothing is applied to a live projectile or weapon.
			vector nextVelocity = velocity + Acceleration(velocity, wind) * dt;
			vector nextPosition = position + (velocity + nextVelocity) * (dt * 0.5);
			if (nextPosition[2] >= distance)
			{
				float fraction = (distance - position[2]) / (nextPosition[2] - position[2]);
				vector atDistance = vector.Lerp(position, nextPosition, fraction);
				correctionMrad = -SCR_Math.ConvertFromRadians(Math.Atan2(atDistance[0], distance), SCR_EOpticsAngleUnits.MILLIRADIANS);
				return true;
			}
			position = nextPosition;
			velocity = nextVelocity;
			elapsed += dt;
		}
		return false;
	}
}

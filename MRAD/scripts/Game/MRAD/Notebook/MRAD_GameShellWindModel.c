// Fallback for the game's ordinary ShellMoveComponent only. Parameters come
// from the loaded prefab, never from external ammunition data or AI-table time.
// Other movement classes and invalid/incomplete sources remain unsupported.
class MRAD_GameShellWindModel
{
	protected bool m_bSupported;
	protected float m_fDragPerMass;
	// Native diagnostic comparisons identify a fixed 30 Hz velocity update
	// with trapezoidal position integration. This is an engine convention,
	// not an ammunition-specific adjustment (see the WorldEditor QA probe).
	protected const float MODEL_STEP = 1.0 / 30.0;
	protected const int MAX_STEPS = 1801;
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
			float dt = MODEL_STEP;
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

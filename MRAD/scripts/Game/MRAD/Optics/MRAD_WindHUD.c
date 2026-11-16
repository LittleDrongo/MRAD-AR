// Small wind indicator in the top-left corner, in the spirit of the Arma 3 wind HUD:
// an arrow that shows where the wind blows (12 o'clock = away from the viewer) and
// one dot per Beaufort step. It is drawn only while an MRAD scope is in ADS and can
// be hidden with the settings checkbox or the MRAD_ToggleWindHUD action.
class MRAD_WindHUD
{
	protected const ResourceName LAYOUT = "{A7DE1002B62C4780}UI/layouts/MRAD/MRAD_WindHUD.layout";

	// TrianglePointer points up at zero rotation, which is zero bearing.
	protected const float ARROW_TEXTURE_OFFSET_DEG = 0;
	protected const string DOT = "•";

	protected Widget m_wRoot;
	protected ImageWidget m_wArrow;
	protected TextWidget m_wDots;
	protected TextWidget m_wSpeed;
	protected string m_sDots;
	protected float m_fBearing;
	protected bool m_bToggleHeld;

	//------------------------------------------------------------------------------------------------
	void Delete()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
		m_wArrow = null;
		m_wDots = null;
		m_wSpeed = null;
		m_sDots = string.Empty;
	}

	//------------------------------------------------------------------------------------------------
	protected bool Create()
	{
		if (m_wRoot)
			return true;

		if (!GetGame().GetWorkspace())
			return false;

		m_wRoot = GetGame().GetWorkspace().CreateWidgets(LAYOUT);
		if (!m_wRoot)
			return false;

		m_wArrow = ImageWidget.Cast(m_wRoot.FindAnyWidget("MRAD_WindArrow"));
		m_wDots = TextWidget.Cast(m_wRoot.FindAnyWidget("MRAD_WindDots"));
		m_wSpeed = TextWidget.Cast(m_wRoot.FindAnyWidget("MRAD_WindSpeed"));
		if (!m_wArrow || !m_wDots || !m_wSpeed)
		{
			Delete();
			return false;
		}

		m_wArrow.SetPivot(0.5, 0.5);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Beaufort scale from the wind speed in m/s (upper bounds of steps 1..12).
	static int Beaufort(float speed)
	{
		if (speed < 0.3)
			return 0;
		if (speed < 1.6)
			return 1;
		if (speed < 3.4)
			return 2;
		if (speed < 5.5)
			return 3;
		if (speed < 8.0)
			return 4;
		if (speed < 10.8)
			return 5;
		if (speed < 13.9)
			return 6;
		if (speed < 17.2)
			return 7;
		if (speed < 20.8)
			return 8;
		if (speed < 24.5)
			return 9;
		if (speed < 28.5)
			return 10;
		if (speed < 32.7)
			return 11;

		return 12;
	}

	//------------------------------------------------------------------------------------------------
	//! Bearing of the wind relative to the view, degrees clockwise, 0 = wind blows away from the viewer.
	static float RelativeBearing(vector forward, float windDirectionDegrees)
	{
		float horizontal = Math.Sqrt(forward[0] * forward[0] + forward[2] * forward[2]);
		if (horizontal <= 0.0001)
			return 0;

		vector view = Vector(forward[0] / horizontal, 0, forward[2] / horizontal);
		vector right = Vector(view[2], 0, -view[0]);
		float azimuth = windDirectionDegrees * Math.DEG2RAD;
		// TimeAndWeatherManager direction is where the wind blows to.
		vector windTo = Vector(Math.Sin(azimuth), 0, Math.Cos(azimuth));
		float toRight = windTo[0] * right[0] + windTo[2] * right[2];
		float toForward = windTo[0] * view[0] + windTo[2] * view[2];
		return Math.Atan2(toRight, toForward) * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	protected static string Dots(int beaufort)
	{
		string dots = string.Empty;
		for (int i = 0; i < beaufort; i++)
		{
			if (!dots.IsEmpty())
				dots += " ";

			dots += DOT;
		}

		return dots;
	}

	//------------------------------------------------------------------------------------------------
	//! Called every frame while an MRAD scope is in ADS.
	//! \param character the local shooter
	//! \param toggleHeld the MRAD_ToggleWindHUD action is held this frame
	void Update(ChimeraCharacter character, bool toggleHeld)
	{
		MRAD_Settings settings = MRAD_Settings.Get();

		// Edge on the toggle action, so one press flips the visibility once.
		if (toggleHeld && !m_bToggleHeld)
			settings.SetShowWindHUD(!settings.GetShowWindHUD());

		m_bToggleHeld = toggleHeld;

		if (!settings.GetShowWindHUD() || !character)
		{
			Delete();
			return;
		}

		ChimeraWorld world = GetGame().GetWorld();
		if (!world)
		{
			Delete();
			return;
		}

		TimeAndWeatherManagerEntity weather = world.GetTimeAndWeatherManager();
		if (!weather)
		{
			Delete();
			return;
		}

		if (!Create())
			return;

		float speed = Math.Max(0.0, weather.GetWindSpeed());
		int beaufort = Beaufort(speed);
		float bearing = RelativeBearing(character.GetWorldTransformAxis(2), weather.GetWindDirection());

		if (beaufort != 0)
		{
			string dots = Dots(beaufort);
			if (dots != m_sDots)
			{
				m_wDots.SetText(dots);
				m_sDots = dots;
			}
		}
		else if (m_sDots != "0")
		{
			m_wDots.SetText("0");
			m_sDots = "0";
		}

		m_wDots.SetVisible(true);
		m_wArrow.SetVisible(beaufort != 0);
		if (beaufort != 0)
			m_wArrow.SetRotation(bearing + ARROW_TEXTURE_OFFSET_DEG);

		m_wSpeed.SetText(speed.ToString(1, 1) + " " + WidgetManager.Translate("#MRAD_WindUnit"));
	}
}

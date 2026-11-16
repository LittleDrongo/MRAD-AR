// Small wind indicator in the top-left corner (Arma 3 style): an arrow that shows
// where the wind blows relative to the view (12 o'clock = away from the viewer) and
// one dot per Beaufort step. No panel, no exact speed in m/s.
// Shown or hidden with MRAD_ToggleWindHUD (Shift+F by default) or the settings checkbox.
class MRAD_WindHUD
{
	protected const ResourceName LAYOUT = "{A7DE1002B62C4780}UI/layouts/MRAD/MRAD_WindHUD.layout";
	protected const string DOT = "•";

	protected const int BEAUFORT_MAX = 12;

	protected Widget m_wRoot;
	protected ref array<ImageWidget> m_aArrows;
	protected TextWidget m_wDots;
	protected string m_sDots;
	protected int m_iBeaufort = -1;
	protected float m_fBearing;

	//------------------------------------------------------------------------------------------------
	void Delete()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();

		m_wRoot = null;
		m_aArrows = null;
		m_wDots = null;
		m_sDots = string.Empty;
		m_iBeaufort = -1;
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

		m_wDots = TextWidget.Cast(m_wRoot.FindAnyWidget("MRAD_WindDots"));
		m_aArrows = {};
		for (int i = 0; i <= BEAUFORT_MAX; i++)
		{
			ImageWidget arrow = ImageWidget.Cast(m_wRoot.FindAnyWidget("MRAD_WindArrow" + i));
			if (!arrow)
			{
				Delete();
				return false;
			}

			// Цвет клетки «Баллы Бофорта» задан в layout; ноль поворота
			// соответствует нулевому пеленгу (текстура смотрит вверх).
			arrow.SetPivot(0.5, 0.5);
			arrow.SetVisible(false);
			m_aArrows.Insert(arrow);
		}

		if (!m_wDots)
		{
			Delete();
			return false;
		}

		// Виджет индикатора не должен забирать фокус UI: иначе игра считает,
		// что открыт интерфейс, и не отдаёт ввод персонажу.
		GetGame().GetWorkspace().SetFocusedWidget(null);

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
	//! Direction the player looks at: the camera when available, the body otherwise.
	protected static vector ViewForward(ChimeraCharacter character)
	{
		CameraManager cameraManager = GetGame().GetCameraManager();
		if (cameraManager)
		{
			CameraBase camera = cameraManager.CurrentCamera();
			if (camera)
			{
				vector cameraMatrix[4];
				camera.GetTransform(cameraMatrix);
				return cameraMatrix[2];
			}
		}

		return character.GetWorldTransformAxis(2);
	}

	//------------------------------------------------------------------------------------------------
	//! Show/hide the indicator; remembered in the player profile.
	static void Toggle()
	{
		MRAD_Settings settings = MRAD_Settings.Get();
		settings.SetShowWindHUD(!settings.GetShowWindHUD());
	}

	//------------------------------------------------------------------------------------------------
	//! Called every frame for the local player.
	//! \param character the local shooter
	void Update(ChimeraCharacter character)
	{
		MRAD_Settings settings = MRAD_Settings.Get();

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

		int beaufort = Beaufort(Math.Max(0.0, weather.GetWindSpeed()));
		string dots = Dots(beaufort);
		if (dots != m_sDots)
		{
			m_wDots.SetText(dots);
			m_sDots = dots;
		}

		if (!m_aArrows || m_aArrows.Count() <= beaufort)
			return;

		// Показана ровно одна стрелка: цвет клетки шкалы Бофорта для текущих баллов.
		if (beaufort != m_iBeaufort)
		{
			if (m_iBeaufort >= 0 && m_iBeaufort < m_aArrows.Count())
				m_aArrows[m_iBeaufort].SetVisible(false);

			m_iBeaufort = beaufort;
			m_aArrows[beaufort].SetVisible(true);
		}

		// Calm weather: no direction to show.
		m_aArrows[beaufort].SetVisible(beaufort != 0);
		if (beaufort == 0)
			return;

		// Стрелка следует за взглядом каждый кадр: без порогов и без ограничений.
		float bearing = RelativeBearing(ViewForward(character), weather.GetWindDirection());
		m_aArrows[beaufort].SetRotation(bearing);
		m_fBearing = bearing;
	}
}

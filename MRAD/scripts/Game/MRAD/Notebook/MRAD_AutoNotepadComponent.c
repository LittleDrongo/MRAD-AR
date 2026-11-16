// Range card filled from the weapon of the player who holds the notepad.
// Same data and format as the 1.x card (author's example in NotepadSheet.layout):
//	M40A5
//	Ball: M61
//
//	Meters	|	up mrad
//	100	-	1,3
// plus a wind column (2.0 spec): correction for the reference crosswind (Settings, 4 m/s)
// or, with "actual wind", for the current crosswind with an arrow to the side to turn.
// Calculated locally on the holder's machine, nothing is replicated.
[ComponentEditorProps(category: "GameScripted/MRAD", description: "Range card of the holder's weapon")]
class Drg_AutoNotepadRtComponentClass : Drg_NotepadRtComponentClass
{
}

class Drg_AutoNotepadRtComponent : Drg_NotepadRtComponent
{
	protected static const int RANGE_STEP = 100;
	protected static const int MAX_ROWS_ON_SHEET = 15;
	protected static const string ARROW_RIGHT = "→";
	protected static const string ARROW_LEFT = "←";
	// Text box over the whole sheet; 15 rows (100-1500 m) + header + page number at font 50.
	protected static const int FONT_SIZE = 50;
	// The font's own line height is ~1.25x its size: tighter lines so 19 lines fit the sheet.
	protected static const float LINE_SPACING = -12;
	protected static const float TEXT_X = 95;
	protected static const float TEXT_Y = 30;
	protected static const float TEXT_W = 570;
	protected static const float TEXT_H = 960;

	// Page number in the bottom right corner (RichText collapses a run of tabs).
	protected static const float PAGE_X = 470;
	protected static const float PAGE_Y = 920;
	protected static const float PAGE_W = 180;
	protected static const float PAGE_H = 65;
	protected static const ResourceName PAGE_FONT = "{F2C89AF8C4C1FE43}UI/Fonts/Eskal_Font4You.fnt";

	protected TextWidget m_wPage;

	//------------------------------------------------------------------------------------------------
	protected override void OnWidgetsCreated()
	{
		FrameSlot.SetPos(m_wRich, TEXT_X, TEXT_Y);
		FrameSlot.SetSize(m_wRich, TEXT_W, TEXT_H);
		m_wRich.SetExactFontSize(FONT_SIZE);
		m_wRich.SetLineSpacing(LINE_SPACING);

		m_wPage = TextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.TextWidgetTypeID, WidgetFlags.VISIBLE, m_wRich.GetColor(), 0, m_wRich.GetParent()));
		if (!m_wPage)
			return;

		FrameSlot.SetPos(m_wPage, PAGE_X, PAGE_Y);
		FrameSlot.SetSize(m_wPage, PAGE_W, PAGE_H);
		m_wPage.SetFont(PAGE_FONT);
		m_wPage.SetBold(true);
		m_wPage.SetExactFontSize(FONT_SIZE);
	}

	//------------------------------------------------------------------------------------------------
	override void Draw()
	{
		if (!m_wRich || !m_wRtTexture)
			return;

		m_wRtTexture.SetEnabled(true);
		m_wRich.SetText(BuildText());
		if (m_wPage)
		{
			string page;
			int pages = GetNumberOfPages();
			if (m_Holder && m_sError.IsEmpty() && pages > 1)
				page = string.Format("%1/%2", m_iPage + 1, pages);

			m_wPage.SetText(page);
		}

		m_wRtTexture.SetEnabled(false);
	}

	protected ChimeraCharacter m_Holder;
	protected string m_sWeaponName;
	protected string m_sAmmoName;
	protected string m_sError = "#MRAD_Error_NoWeapon";
	protected ref array<int> m_aRanges = {};
	protected ref array<float> m_aMrad = {};
	protected ref array<float> m_aWind = {};
	protected ref array<bool> m_aWindValid = {};
	protected float m_fWindSpeed;
	protected bool m_bActualWind;
	protected int m_iPage;

	//------------------------------------------------------------------------------------------------
	override void OnTakenInHand(ChimeraCharacter character)
	{
		m_Holder = character;
		Recalculate();
		super.OnTakenInHand(character);
	}

	//------------------------------------------------------------------------------------------------
	override int GetNumberOfPages()
	{
		int rows = GetRowsPerPage();
		return Math.Max(1, (m_aRanges.Count() + rows - 1) / rows);
	}

	//------------------------------------------------------------------------------------------------
	override void ChangePage(int direction)
	{
		int count = GetNumberOfPages();
		m_iPage = ((m_iPage + direction) % count + count) % count;
		Draw();
	}

	//------------------------------------------------------------------------------------------------
	int GetPageIndex()
	{
		return m_iPage;
	}

	//------------------------------------------------------------------------------------------------
	protected int GetRowsPerPage()
	{
		return Math.ClampInt(MRAD_Settings.Get().GetRowsPerPage(), 1, MAX_ROWS_ON_SHEET);
	}

	//------------------------------------------------------------------------------------------------
	protected BaseWeaponComponent FindWeapon()
	{
		if (!m_Holder)
			return null;

		CharacterWeaponManagerComponent manager = CharacterWeaponManagerComponent.Cast(m_Holder.FindComponent(CharacterWeaponManagerComponent));
		if (!manager)
			return null;

		BaseWeaponComponent weapon = manager.GetCurrentWeapon();
		if (weapon)
			return weapon;

		// Weapon may be slung while the notepad is in hands: use the primary slot.
		array<WeaponSlotComponent> slots = {};
		manager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			if (slot && slot.GetWeaponSlotIndex() == 0 && slot.GetWeaponEntity())
				return slot;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	void Recalculate()
	{
		m_aRanges.Clear();
		m_aMrad.Clear();
		m_aWind.Clear();
		m_aWindValid.Clear();
		m_sWeaponName = string.Empty;
		m_sAmmoName = string.Empty;
		m_sError = string.Empty;

		BaseWeaponComponent weapon = FindWeapon();
		if (!weapon)
		{
			m_sError = "#MRAD_Error_NoWeapon";
			return;
		}

		UIInfo weaponInfo = weapon.GetUIInfo();
		if (weaponInfo)
			m_sWeaponName = WidgetManager.Translate(weaponInfo.GetName());

		BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
		if (!muzzle)
		{
			m_sError = "#MRAD_Error_NoMuzzle";
			return;
		}

		BaseMagazineComponent magazine = weapon.GetCurrentMagazine();
		if (!magazine)
		{
			m_sError = "#MRAD_Error_NoMagazine";
			return;
		}

		MagazineUIInfo magazineInfo = MagazineUIInfo.Cast(magazine.GetUIInfo());
		if (magazineInfo)
		{
			m_sAmmoName = WidgetManager.Translate(magazineInfo.GetAmmoCaliber());
			string ammoType = WidgetManager.Translate(magazineInfo.GetAmmoType());
			if (!ammoType.IsEmpty())
				m_sAmmoName = m_sAmmoName + " " + ammoType;

			// The handwritten font has no "×": "7.62×51mm" would fall back to a printed font.
			m_sAmmoName = ReplaceMultiplySign(m_sAmmoName);
		}

		MRAD_Settings settings = MRAD_Settings.Get();
		m_bActualWind = settings.GetUseActualWind();
		float signedWind = GetSignedCrosswind(settings);
		m_fWindSpeed = Math.AbsFloat(signedWind);
		if (!m_bActualWind)
			m_fWindSpeed = settings.GetReferenceWindSpeed();

		MRAD_RangeCardInput input = MRAD_RangeCardInputResolver.ResolveWeapon(weapon);
		bool hasInput = input.m_sError.IsEmpty();
		MRAD_RangeCardWind wind = new MRAD_RangeCardWind();
		if (hasInput)
			wind.Load(input.m_Source);

		// The engine's aim table (GetAimHeightOfNextProjectile) is off by up to ~0.7 MRAD at 800 m
		// against a real M21 round (lab 2026-11-16): the flight itself is simulated instead
		// (ShellMove physics, the same as the launched round), from the sight line above the bore.
		MRAD_GameShellWindModel model;
		float launchSpeed;
		if (hasInput)
		{
			model = wind.GetGameModel();
			launchSpeed = wind.GetLaunchSpeed(input);
		}

		bool useModel = model && model.IsSupported() && launchSpeed > 0;
		float sightHeight = GetSightHeight(weapon);

		int maxRange = Math.ClampInt(settings.GetMaxRange(), RANGE_STEP, 5000);
		for (int distance = RANGE_STEP; distance <= maxRange; distance += RANGE_STEP)
		{
			float travelTime;
			float aimHeight = BallisticTable.GetAimHeightOfNextProjectile(distance, travelTime, muzzle, false);
			// Negative time: the round does not reach this distance.
			if (travelTime < 0)
				break;

			float angle = Math.Atan2(aimHeight, distance);
			float windMrad;
			bool windValid;
			vector at;
			float simulatedAngle;
			if (useModel && model.TryElevation(distance, launchSpeed, angle, sightHeight, simulatedAngle) && model.TrySimulate(distance, launchSpeed, simulatedAngle, signedWind, at))
			{
				angle = simulatedAngle;
				windValid = true;
				windMrad = -SCR_Math.ConvertFromRadians(Math.Atan2(at[0], distance), SCR_EOpticsAngleUnits.MILLIRADIANS);
			}
			else if (hasInput)
			{
				// Other projectile movement types: game wind table -> native prediction.
				windValid = wind.TryTable(angle, input.m_fSpeedCoef, signedWind, windMrad);
				if (!windValid)
					windValid = wind.TrySimulation(input, distance, angle, travelTime, signedWind, windMrad);
			}

			m_aRanges.Insert(distance);
			m_aMrad.Insert(SCR_Math.ConvertFromRadians(angle, SCR_EOpticsAngleUnits.MILLIRADIANS));
			m_aWind.Insert(windMrad);
			m_aWindValid.Insert(windValid);
		}

		wind.Release();

		if (m_aRanges.IsEmpty())
			m_sError = "#MRAD_Error_NoBallistics";

		m_iPage = Math.ClampInt(m_iPage, 0, GetNumberOfPages() - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! + = turn the correction right. Reference mode: +reference speed.
	protected float GetSignedCrosswind(MRAD_Settings settings)
	{
		if (!settings.GetUseActualWind())
			return Math.Max(0.0, settings.GetReferenceWindSpeed());

		ChimeraWorld world = GetGame().GetWorld();
		if (!world || !m_Holder)
			return 0;

		TimeAndWeatherManagerEntity weather = world.GetTimeAndWeatherManager();
		if (!weather)
			return 0;

		vector forward = m_Holder.GetWorldTransformAxis(2);
		float horizontal = Math.Sqrt(forward[0] * forward[0] + forward[2] * forward[2]);
		if (horizontal <= 0.0001)
			return 0;

		// Measured in the lab (2026-11-16): direction 270 drifts a round flying north to the west (-X),
		// so (sin, cos) of the direction is where the wind blows TO. Drift left = correction right (+).
		vector right = Vector(forward[2] / horizontal, 0, -forward[0] / horizontal);
		float azimuth = weather.GetWindDirection() * Math.DEG2RAD;
		vector windTo = Vector(Math.Sin(azimuth), 0, Math.Cos(azimuth)) * Math.Max(0.0, weather.GetWindSpeed());
		return -(windTo[0] * right[0] + windTo[2] * right[2]);
	}

	//------------------------------------------------------------------------------------------------
	//! Height of the scope's line of sight above the bore axis (m), 0 when unknown.
	protected float GetSightHeight(BaseWeaponComponent weapon)
	{
		if (!m_Holder || !weapon)
			return 0;

		BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(m_Holder.FindComponent(BaseWeaponManagerComponent));
		if (!manager || manager.GetCurrentWeapon() != weapon)
			return 0;

		vector muzzle[4];
		if (!manager.GetCurrentMuzzleTransform(muzzle))
			return 0;

		vector sightPosition;
		SCR_2DOpticsComponent optic = SCR_2DOpticsComponent.Cast(weapon.GetSights());
		if (optic)
		{
			vector local[4];
			IEntity parent = optic.GetCameraLocalTransform(local);
			if (!parent)
				return 0;

			vector parentWorld[4];
			parent.GetWorldTransform(parentWorld);
			sightPosition = local[3].Multiply4(parentWorld);
		}
		else
		{
			BaseSightsComponent sights = weapon.GetSights();
			vector sightsWorld[4];
			if (!sights || !sights.GetSightsTransform(sightsWorld, false))
				return 0;

			sightPosition = sightsWorld[3];
		}

		float height = vector.Dot(sightPosition - muzzle[3], muzzle[1].Normalized());
		return Math.Clamp(height, 0, 0.3);
	}

	//------------------------------------------------------------------------------------------------
	protected string FormatWind(int index)
	{
		if (!m_aWindValid[index])
			return "-";

		float value = m_aWind[index];
		string text = FormatMrad(Math.AbsFloat(value));
		if (!m_bActualWind || Math.AbsFloat(value) < 0.05)
			return text;

		// Arrows exist in the handwritten font (U+2192 / U+2190).
		if (value > 0)
			return ARROW_RIGHT + text;

		return ARROW_LEFT + text;
	}

	//------------------------------------------------------------------------------------------------
	//! U+00D7 (UTF-8 C3 97) -> "x", byte by byte (no non-ASCII literal in the source).
	static string ReplaceMultiplySign(string text)
	{
		string result;
		int length = text.Length();
		for (int i = 0; i < length; i++)
		{
			string current = text.Get(i);
			if (current.ToAscii() == 215)
			{
				result += "x";
				continue;
			}

			if (i + 1 < length)
			{
				int first = current.ToAscii();
				int second = text.Get(i + 1).ToAscii();
				if ((first == 195 || first == -61) && (second == 151 || second == -105))
				{
					result += "x";
					i++;
					continue;
				}
			}

			result += current;
		}

		return result;
	}

	//------------------------------------------------------------------------------------------------
	static string FormatMrad(float value)
	{
		string text = value.ToString(-1, 1);
		text.Replace(".", ",");
		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected override string BuildText()
	{
		if (!m_Holder)
			return m_sText;

		string text = m_sWeaponName + "\n";
		text += m_sAmmoName;

		if (!m_sError.IsEmpty())
			return text + "\n\n" + WidgetManager.Translate(m_sError);

		// Wind column header = wind speed it is calculated for ("4 m/s"); "wind 4,0" did not fit at 50.
		string windSpeed = FormatMrad(m_fWindSpeed);
		if (windSpeed.EndsWith(",0"))
			windSpeed = windSpeed.Substring(0, windSpeed.Length() - 2);

		// "Meters" spans two tab cells: "up" and the wind speed stay over their columns.
		text += string.Format("\nMeters\tup\t|\t%1 m/s", windSpeed);

		int rows = GetRowsPerPage();
		int first = m_iPage * rows;
		for (int i = first; i < first + rows; i++)
		{
			// Empty lines on a short last page keep the page number at the bottom.
			if (i < m_aRanges.Count())
				text += string.Format("\n%1\t-\t%2\t-\t%3", m_aRanges[i], FormatMrad(m_aMrad[i]), FormatWind(i));
			else
				text += "\n";
		}


		return text;
	}
}

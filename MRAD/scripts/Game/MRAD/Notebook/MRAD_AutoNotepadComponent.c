// Range card filled from the weapon of the player who holds the notepad.
//	M21 SWS
//	7.62x51mm M118
//	wind 1 m/s
//	Meters	up	side	head
//	100	1,3	0,06	0,00
// "side" is the crosswind correction for the reference wind (1 m/s by default, multiply
// by the actual wind); "head" is the extra elevation for a headwind of the same speed
// (+ headwind: add, - tailwind: subtract). Both are in MRAD. With "actual wind" the
// line above the header reads "actual wind" and the columns hold the current corrections.
// Calculated locally on the holder's machine, nothing is replicated.
[ComponentEditorProps(category: "GameScripted/MRAD", description: "Range card of the holder's weapon")]
class Drg_AutoNotepadRtComponentClass : Drg_NotepadRtComponentClass
{
}

class Drg_AutoNotepadRtComponent : Drg_NotepadRtComponent
{
	protected static const int RANGE_STEP = 100;
	protected static const int MAX_ROWS_ON_SHEET = 20;
	protected static const string ARROW_RIGHT = "→";
	protected static const string ARROW_LEFT = "←";
	// The title (weapon, round, wind speed) stays a single rich text; the table itself
	// is four separate rich texts with explicit x positions, so the columns can breathe
	// (the tab stops of the handwritten font are too narrow and could not be widened).
	protected static const float TITLE_HEIGHT = 115;
	protected static const float COLUMN_Y = 140;
	protected static const float COLUMN_HEIGHT = 780;
	protected static const int COLUMN_COUNT = 4;
	protected static const float COLUMN_X_0 = 0;
	protected static const float COLUMN_X_1 = 150;
	protected static const float COLUMN_X_2 = 290;
	protected static const float COLUMN_X_3 = 420;
	protected static const float COLUMN_W_0 = 145;
	protected static const float COLUMN_W_1 = 135;
	protected static const float COLUMN_W_2 = 125;
	protected static const float COLUMN_W_3 = 150;
	// Text box over the whole sheet; 20 rows (100-2000 m) + wind line + header.
	// 39 fits all 24 lines on one page; the wind column is split into side and head,
	// and both keep two decimals (a 1 m/s reference is too small for one decimal).
	protected static const int FONT_SIZE = 39;
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
	protected ref array<RichTextWidget> m_aColumns = {};

	//------------------------------------------------------------------------------------------------
	protected override void OnWidgetsCreated()
	{
		FrameSlot.SetPos(m_wRich, TEXT_X, TEXT_Y);
		FrameSlot.SetSize(m_wRich, TEXT_W, TITLE_HEIGHT);
		m_wRich.SetExactFontSize(FONT_SIZE);
		m_wRich.SetLineSpacing(LINE_SPACING);

		m_aColumns.Clear();
		CreateColumn(COLUMN_X_0, COLUMN_W_0);
		CreateColumn(COLUMN_X_1, COLUMN_W_1);
		CreateColumn(COLUMN_X_2, COLUMN_W_2);
		CreateColumn(COLUMN_X_3, COLUMN_W_3);

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
	protected void CreateColumn(float x, float width)
	{
		RichTextWidget column = RichTextWidget.Cast(GetGame().GetWorkspace().CreateWidget(WidgetType.RichTextWidgetTypeID, WidgetFlags.VISIBLE, m_wRich.GetColor(), 0, m_wRich.GetParent()));
		if (!column)
			return;

		FrameSlot.SetPos(column, TEXT_X + x, COLUMN_Y);
		FrameSlot.SetSize(column, width, COLUMN_HEIGHT);
		column.SetFont(PAGE_FONT);
		column.SetExactFontSize(FONT_SIZE);
		column.SetLineSpacing(LINE_SPACING);
		m_aColumns.Insert(column);
	}

	//------------------------------------------------------------------------------------------------
	protected override void ClearRenderTarget()
	{
		m_aColumns.Clear();
		super.ClearRenderTarget();
	}

	//------------------------------------------------------------------------------------------------
	override void Draw()
	{
		if (!m_wRich || !m_wRtTexture)
			return;

		m_wRtTexture.SetEnabled(true);

		bool table = m_Holder && m_sError.IsEmpty() && m_aColumns.Count() == COLUMN_COUNT;
		if (table)
		{
			FrameSlot.SetSize(m_wRich, TEXT_W, TITLE_HEIGHT);
			m_wRich.SetText(BuildTitle());
		}
		else
		{
			// Error/static page: the whole sheet is one text box again.
			FrameSlot.SetSize(m_wRich, TEXT_W, TEXT_H);
			m_wRich.SetText(BuildText());
		}

		for (int index = 0; index < m_aColumns.Count(); index++)
		{
			RichTextWidget column = m_aColumns[index];
			column.SetVisible(table);
			if (table)
				column.SetText(BuildColumn(index));
		}

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
	protected ref array<float> m_aHead = {};
	protected ref array<bool> m_aHeadValid = {};
	protected float m_fWindSpeed;
	protected bool m_bActualWind;
	protected int m_iPage;

	//------------------------------------------------------------------------------------------------
	override void OnTakenInHand(ChimeraCharacter character)
	{
		m_Holder = character;
		MRAD_NotepadInputController.SetHeld(true);
		Recalculate();
		super.OnTakenInHand(character);
	}

	//------------------------------------------------------------------------------------------------
	override void OnPutAway()
	{
		MRAD_NotepadInputController.SetHeld(false);
		super.OnPutAway();
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		MRAD_NotepadInputController.SetHeld(false);
		super.OnDelete(owner);
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
		m_aHead.Clear();
		m_aHeadValid.Clear();
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
		float signedWind;
		float signedHead;
		if (m_bActualWind)
		{
			GetSignedWind(signedWind, signedHead);
		}
		else
		{
			// Reference columns: the speed itself (1 m/s by default), the spotter multiplies.
			m_fWindSpeed = Math.Max(0.0, settings.GetReferenceWindSpeed());
			signedWind = m_fWindSpeed;
			signedHead = m_fWindSpeed;
		}

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
			float windMrad = 0;
			float headMrad = 0;
			bool windValid = false;
			bool headValid = false;
			float simulatedAngle;
			bool modelAngle = useModel && model.TryElevation(distance, launchSpeed, angle, sightHeight, simulatedAngle);
			if (modelAngle)
			{
				angle = simulatedAngle;
				windValid = model.TryCross(distance, launchSpeed, angle, signedWind, windMrad);
			}

			if (!windValid && hasInput)
			{
				// Other projectile movement types: game wind table -> native prediction.
				windValid = wind.TryTable(angle, input.m_fSpeedCoef, signedWind, windMrad);
				if (!windValid)
					windValid = wind.TrySimulation(input, distance, angle, travelTime, signedWind, windMrad);
			}

			// Headwind/tailwind changes the flight time and therefore the drop. Only the
			// simulated flight is accurate enough for this column; other movements show "-".
			if (modelAngle)
			{
				headValid = model.TryHead(distance, launchSpeed, angle, signedHead, headMrad);
			}

			m_aRanges.Insert(distance);
			m_aMrad.Insert(SCR_Math.ConvertFromRadians(angle, SCR_EOpticsAngleUnits.MILLIRADIANS));
			m_aWind.Insert(windMrad);
			m_aWindValid.Insert(windValid);
			m_aHead.Insert(headMrad);
			m_aHeadValid.Insert(headValid);
		}

		wind.Release();

		if (m_aRanges.IsEmpty())
			m_sError = "#MRAD_Error_NoBallistics";

		m_iPage = Math.ClampInt(m_iPage, 0, GetNumberOfPages() - 1);
	}

	//------------------------------------------------------------------------------------------------
	//! Current wind split along the view: + crosswind = correction right,
	//! + headwind = wind blows towards the shooter (slows the round down).
	protected void GetSignedWind(out float signedCrosswind, out float signedHeadwind)
	{
		signedCrosswind = 0;
		signedHeadwind = 0;

		ChimeraWorld world = GetGame().GetWorld();
		if (!world || !m_Holder)
			return;

		TimeAndWeatherManagerEntity weather = world.GetTimeAndWeatherManager();
		if (!weather)
			return;

		vector forward = m_Holder.GetWorldTransformAxis(2);
		float horizontal = Math.Sqrt(forward[0] * forward[0] + forward[2] * forward[2]);
		if (horizontal <= 0.0001)
			return;

		// Measured in the lab (2026-11-16): direction 270 drifts a round flying north to the west (-X),
		// so (sin, cos) of the direction is where the wind blows TO. Drift left = correction right (+).
		vector view = Vector(forward[0] / horizontal, 0, forward[2] / horizontal);
		vector right = Vector(view[2], 0, -view[0]);
		float azimuth = weather.GetWindDirection() * Math.DEG2RAD;
		vector windTo = Vector(Math.Sin(azimuth), 0, Math.Cos(azimuth)) * Math.Max(0.0, weather.GetWindSpeed());
		signedCrosswind = -(windTo[0] * right[0] + windTo[2] * right[2]);
		signedHeadwind = -(windTo[0] * view[0] + windTo[2] * view[2]);
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
		string text = FormatHundredths(value);
		if (!m_bActualWind || Math.AbsFloat(value) < 0.005)
			return text;

		// Arrows exist in the handwritten font (U+2192 / U+2190).
		if (value > 0)
			return ARROW_RIGHT + text;

		return ARROW_LEFT + text;
	}

	//------------------------------------------------------------------------------------------------
	//! + headwind: the round lands low, add elevation. - tailwind: it lands high, take some off.
	protected string FormatHead(int index)
	{
		if (!m_aHeadValid[index])
			return "-";

		float value = m_aHead[index];
		if (Math.AbsFloat(value) < 0.005)
			return "0,00";

		string text = FormatHundredths(value);
		if (value > 0)
			return "+" + text;

		return "-" + text;
	}

	//------------------------------------------------------------------------------------------------
	//! Two decimals with a comma, without relying on float formatting details.
	static string FormatHundredths(float value)
	{
		int hundredths = Math.Round(Math.AbsFloat(value) * 100.0);
		int whole = hundredths / 100;
		int fraction = hundredths % 100;
		string tail = fraction.ToString();
		if (fraction < 10)
			tail = "0" + tail;

		return string.Format("%1,%2", whole, tail);
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
	protected string BuildTitle()
	{
		string text = m_sWeaponName + "\n";
		text += m_sAmmoName;

		// Both wind columns are for the same speed; the spotter multiplies by the actual wind.
		if (m_bActualWind)
			return text + "\nactual wind";

		string windSpeed = FormatMrad(m_fWindSpeed);
		if (windSpeed.EndsWith(",0"))
			windSpeed = windSpeed.Substring(0, windSpeed.Length() - 2);

		return text + string.Format("\nwind %1 m/s", windSpeed);
	}

	//------------------------------------------------------------------------------------------------
	protected string BuildColumn(int index)
	{
		string header = "Meters";
		switch (index)
		{
			case 1:
				header = "up";
				break;
			case 2:
				header = "side";
				break;
			case 3:
				header = "head";
				break;
		}

		string text = header;
		int rows = GetRowsPerPage();
		int first = m_iPage * rows;
		for (int i = first; i < first + rows; i++)
		{
			if (i >= m_aRanges.Count())
			{
				text += "\n";
				continue;
			}

			switch (index)
			{
				case 0:
					text += "\n" + m_aRanges[i].ToString();
					break;
				case 1:
					text += "\n" + FormatMrad(m_aMrad[i]);
					break;
				case 2:
					text += "\n" + FormatWind(i);
					break;
				default:
					text += "\n" + FormatHead(i);
					break;
			}
		}

		return text;
	}

	//------------------------------------------------------------------------------------------------
	protected override string BuildText()
	{
		if (!m_Holder)
			return m_sText;

		if (!m_sError.IsEmpty())
			return BuildTitle() + "\n\n" + WidgetManager.Translate(m_sError);

		return BuildTitle();
	}
}

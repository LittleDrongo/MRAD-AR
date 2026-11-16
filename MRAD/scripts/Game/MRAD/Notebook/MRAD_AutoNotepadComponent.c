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
	protected static const int MAX_ROWS_ON_SHEET = 12;
	protected static const string ARROW_RIGHT = "→";
	protected static const string ARROW_LEFT = "←";

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

		int maxRange = Math.ClampInt(settings.GetMaxRange(), RANGE_STEP, 5000);
		for (int distance = RANGE_STEP; distance <= maxRange; distance += RANGE_STEP)
		{
			float travelTime;
			float aimHeight = BallisticTable.GetAimHeightOfNextProjectile(distance, travelTime, muzzle, false);
			// Negative time: the round does not reach this distance.
			if (travelTime < 0)
				break;

			float angle = Math.Atan2(aimHeight, distance);
			m_aRanges.Insert(distance);
			m_aMrad.Insert(SCR_Math.ConvertFromRadians(angle, SCR_EOpticsAngleUnits.MILLIRADIANS));

			// Game wind table -> projectile simulation -> ShellMove model (MRAD 2.0 code).
			float windMrad;
			bool windValid;
			if (hasInput)
			{
				windValid = wind.TryTable(angle, input.m_fSpeedCoef, signedWind, windMrad);
				if (!windValid)
					windValid = wind.TrySimulation(input, distance, angle, travelTime, signedWind, windMrad);
				if (!windValid)
					windValid = wind.TryGameModel(input, distance, angle, signedWind, windMrad);
			}

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

		// Weather direction is where the wind comes FROM, degrees.
		vector right = Vector(forward[2] / horizontal, 0, -forward[0] / horizontal);
		float azimuth = weather.GetWindDirection() * Math.DEG2RAD;
		vector windFrom = Vector(Math.Sin(azimuth), 0, Math.Cos(azimuth)) * Math.Max(0.0, weather.GetWindSpeed());
		return windFrom[0] * right[0] + windFrom[2] * right[2];
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

		string text = "\t" + m_sWeaponName + "\n";
		text += "\t" + m_sAmmoName;

		if (!m_sError.IsEmpty())
			return text + "\n\n\t" + WidgetManager.Translate(m_sError);

		text += string.Format("\n\tMeters\t|\tup\t|\twind %1", FormatMrad(m_fWindSpeed));

		int pages = GetNumberOfPages();
		if (pages > 1)
			text += string.Format("\t%1/%2", m_iPage + 1, pages);

		int rows = GetRowsPerPage();
		int first = m_iPage * rows;
		int last = Math.Min(first + rows, m_aRanges.Count());
		for (int i = first; i < last; i++)
		{
			text += string.Format("\n\t%1\t-\t%2\t-\t%3", m_aRanges[i], FormatMrad(m_aMrad[i]), FormatWind(i));
		}

		return text;
	}
}

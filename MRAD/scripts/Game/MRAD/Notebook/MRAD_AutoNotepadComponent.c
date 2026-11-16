[ComponentEditorProps(category: "GameScripted/MRAD", description: "Dynamic first-weapon-slot range card")]
class Drg_AutoNotepadRtComponentClass : Drg_NotepadRtComponentClass
{
}

class Drg_AutoNotepadRtComponent : Drg_NotepadRtComponent
{
	[RplProp(onRplName: "OnPageReplicated")]
	protected int m_iPage;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected int m_iMaxRange = 1500;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected int m_iRowsPerPage = 10;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected float m_fReferenceWind = 4.0;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected bool m_bActualWind;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected float m_fSignedWind = 4.0;

	// The authority transmits the calculated rows. Observers never depend on
	// visibility of another player's magazine or on a different prediction build.
	[RplProp(onRplName: "OnInputsReplicated")]
	protected ref array<int> m_aRanges = {};
	[RplProp(onRplName: "OnInputsReplicated")]
	protected ref array<float> m_aDrops = {};
	[RplProp(onRplName: "OnInputsReplicated")]
	protected ref array<float> m_aWinds = {};
	[RplProp(onRplName: "OnInputsReplicated")]
	protected ref array<bool> m_aWindValid = {};
	[RplProp(onRplName: "OnInputsReplicated")]
	protected string m_sBulletSnapshot;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected string m_sBulletDetailSnapshot;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected string m_sErrorSnapshot = "#MRAD_Error_NoWeapon";
	[RplProp(onRplName: "OnInputsReplicated")]
	protected bool m_bSuppressedSnapshot;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected bool m_bEstimatedSnapshot;
	[RplProp(onRplName: "OnInputsReplicated")]
	protected bool m_bMissingWindSnapshot;

	protected ref MRAD_RangeCardData m_Data;
	protected string m_sCacheKey;
	protected IEntity m_CachedWeapon;
	protected BaseMuzzleComponent m_CachedMuzzle;
	protected BaseMagazineComponent m_CachedMagazine;
	protected bool m_bDirty = true;
	protected int m_iBuildCount;

	override bool OpenFor(ChimeraCharacter character)
	{
		if (SCR_Global.IsEditMode() || !character)
			return false;
		bool opened = super.OpenFor(character);
		GetGame().GetCallqueue().Remove(PollInputs);
		if (m_bHeld)
		{
			PollInputs();
			GetGame().GetCallqueue().CallLater(PollInputs, 250, true);
		}
		return opened;
	}

	override void Close()
	{
		if (GetGame())
			GetGame().GetCallqueue().Remove(PollInputs);
		super.Close();
	}

	protected bool IsLocallyHeld()
	{
		PlayerController controller = GetGame().GetPlayerController();
		return controller && controller.GetControlledEntity() == m_Character;
	}

	protected void PollInputs()
	{
		if (!m_bHeld || !m_Character)
			return;
		MRAD_NotepadGadgetComponent gadget = MRAD_NotepadGadgetComponent.Cast(GetOwner().FindComponent(MRAD_NotepadGadgetComponent));
		if (!gadget || gadget.GetMode() != EGadgetMode.IN_HAND || gadget.GetCharacterOwner() != m_Character)
		{
			Close();
			return;
		}
		if (IsLocallyHeld())
		{
			MRAD_Settings settings = MRAD_Settings.Get();
			// Retry until replicated values acknowledge the requested settings.
			// A first-open RPC may precede replication of the item's gadget mode.
			if (settings.GetMaxRange() != m_iMaxRange || settings.GetRowsPerPage() != m_iRowsPerPage || settings.GetReferenceWindSpeed() != m_fReferenceWind || settings.GetUseActualWind() != m_bActualWind)
			{
				SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
				if (controller)
				{
					controller.MRAD_RequestNotebookSettings(this, settings.GetMaxRange(), settings.GetRowsPerPage(), settings.GetReferenceWindSpeed(), settings.GetUseActualWind());
				}
			}
		}
		if (Replication.IsServer())
		{
			float wind = MRAD_RangeCardCalculator.GetSignedCrosswind(m_Character, m_fReferenceWind, m_bActualWind);
			// Stable hundredths avoid rebuilds from sub-display floating point noise.
			wind = Math.Round(wind * 100.0) / 100.0;
			if (wind != m_fSignedWind)
			{
				m_fSignedWind = wind;
				m_bDirty = true;
				Replication.BumpMe();
			}
		}
		RefreshData();
	}

	protected void RefreshData()
	{
		if (!Replication.IsServer())
		{
			if (m_bDirty || !m_Data)
			{
				ReadSnapshot();
				m_bDirty = false;
				Draw();
			}
			return;
		}
		MRAD_RangeCardInput input = MRAD_RangeCardInputResolver.Resolve(m_Character);
		string key = input.GetKey() + string.Format("|%1|%2|%3|%4", input.m_sError, m_iMaxRange, m_iRowsPerPage, m_fSignedWind.ToString(0, 2));
		if (!m_bDirty && key == m_sCacheKey && m_Data && input.m_WeaponEntity == m_CachedWeapon && input.m_Muzzle == m_CachedMuzzle && input.m_Magazine == m_CachedMagazine)
			return;
		m_sCacheKey = key;
		m_CachedWeapon = input.m_WeaponEntity;
		m_CachedMuzzle = input.m_Muzzle;
		m_CachedMagazine = input.m_Magazine;
		m_bDirty = false;
		m_Data = MRAD_RangeCardCalculator.Build(input, m_iMaxRange, m_fSignedWind);
		m_iBuildCount++;
		WriteSnapshot();
		if (Replication.IsServer())
		{
			int page = m_Data.WrapPage(m_iPage, m_iRowsPerPage);
			if (page != m_iPage)
			{
				m_iPage = page;
				Replication.BumpMe();
			}
		}
		Draw();
	}

	protected void WriteSnapshot()
	{
		m_aRanges.Clear();
		m_aDrops.Clear();
		m_aWinds.Clear();
		m_aWindValid.Clear();
		foreach (MRAD_RangeCardRow row : m_Data.m_aRows)
		{
			m_aRanges.Insert(row.m_iRange);
			m_aDrops.Insert(row.m_fDropMrad);
			m_aWinds.Insert(row.m_fWindMrad);
			m_aWindValid.Insert(row.m_bHasWind);
		}
		m_sBulletSnapshot = m_Data.m_sBulletName;
		m_sBulletDetailSnapshot = m_Data.m_sBulletDetail;
		m_sErrorSnapshot = m_Data.m_sError;
		m_bSuppressedSnapshot = m_Data.m_bSuppressed;
		m_bEstimatedSnapshot = m_Data.m_bEstimatedWind;
		m_bMissingWindSnapshot = m_Data.m_bMissingWind;
		Replication.BumpMe();
	}

	protected void ReadSnapshot()
	{
		m_Data = new MRAD_RangeCardData();
		m_Data.m_sBulletName = m_sBulletSnapshot;
		m_Data.m_sBulletDetail = m_sBulletDetailSnapshot;
		m_Data.m_sError = m_sErrorSnapshot;
		m_Data.m_bSuppressed = m_bSuppressedSnapshot;
		m_Data.m_bEstimatedWind = m_bEstimatedSnapshot;
		m_Data.m_bMissingWind = m_bMissingWindSnapshot;
		m_Data.m_fWindSpeed = Math.AbsFloat(m_fSignedWind);
		int count = Math.Min(m_aRanges.Count(), m_aDrops.Count());
		count = Math.Min(count, Math.Min(m_aWinds.Count(), m_aWindValid.Count()));
		for (int i = 0; i < count; i++)
		{
			MRAD_RangeCardRow row = new MRAD_RangeCardRow();
			row.m_iRange = m_aRanges[i];
			row.m_fDropMrad = m_aDrops[i];
			row.m_fWindMrad = m_aWinds[i];
			row.m_bHasWind = m_aWindValid[i];
			m_Data.m_aRows.Insert(row);
		}
	}

	void ApplySettingsAuthority(int maxRange, int rowsPerPage, float referenceWind, bool actualWind)
	{
		if (!Replication.IsServer())
			return;
		maxRange = (Math.ClampInt(maxRange, 100, 5000) / 100) * 100;
		rowsPerPage = Math.ClampInt(rowsPerPage, 5, 12);
		referenceWind = Math.Round(Math.Clamp(referenceWind, 0, 30) * 10.0) / 10.0;
		if (m_iMaxRange == maxRange && m_iRowsPerPage == rowsPerPage && m_fReferenceWind == referenceWind && m_bActualWind == actualWind)
			return;
		m_iMaxRange = maxRange;
		m_iRowsPerPage = rowsPerPage;
		m_fReferenceWind = referenceWind;
		m_bActualWind = actualWind;
		m_bDirty = true;
		Replication.BumpMe();
	}

	void ChangePageAuthority(int direction)
	{
		if (!Replication.IsServer() || (direction != -1 && direction != 1) || !m_bHeld)
			return;
		RefreshData();
		if (!m_Data || GetNumberOfPages() <= 1)
			return;
		m_iPage = m_Data.WrapPage(m_iPage + direction, m_iRowsPerPage);
		Replication.BumpMe();
		Draw();
	}

	override void RequestPage(int direction)
	{
		if (!IsLocallyHeld() || (direction != 1 && direction != -1))
			return;
		SCR_PlayerController controller = SCR_PlayerController.Cast(GetGame().GetPlayerController());
		if (controller)
			controller.MRAD_RequestNotebookPage(this, direction);
	}

	override int GetNumberOfPages()
	{
		if (!m_Data)
			return 1;
		return m_Data.GetPageCount(m_iRowsPerPage);
	}

	MRAD_RangeCardData GetData()
	{
		// A closed/JIP replica already has the authority snapshot even when
		// no in-hand UI poll has deserialized it yet. Never recalculate here.
		if (!Replication.IsServer() && (m_bDirty || !m_Data))
			RefreshData();
		return m_Data;
	}
	int GetPageIndex() { return m_iPage; }
	int GetBuildCount() { return m_iBuildCount; }
	protected void OnInputsReplicated() { m_bDirty = true; }
	protected void OnPageReplicated() { Draw(); }

	protected void SetText(string name, string value)
	{
		TextWidget widget = TextWidget.Cast(m_wRTRoot.FindAnyWidget(name));
		if (widget)
			widget.SetText(value);
	}

	override void Draw()
	{
		if (!m_wRTRoot || !m_wRtTexture)
			return;
		Widget oldText = m_wRTRoot.FindAnyWidget("drg_Text");
		Widget dynamicContent = m_wRTRoot.FindAnyWidget("mrad_Dynamic");
		if (!dynamicContent)
		{
			// Third-party 1.x layouts retain a useful plain text fallback.
			if (m_Data)
				m_sText = CalculateRadians();
			super.Draw();
			return;
		}
		m_wRtTexture.SetEnabled(true);
		if (oldText)
			oldText.SetVisible(false);
		dynamicContent.SetVisible(true);
		if (!m_Data)
		{
			m_wRtTexture.SetEnabled(false);
			return;
		}
		SetText("mrad_Bullet", string.Format(WidgetManager.Translate("#MRAD_Bullet"), m_Data.GetLocalizedBulletName()));
		string suppressed;
		if (m_Data.m_bSuppressed)
			suppressed = WidgetManager.Translate("#MRAD_Suppressed");
		SetText("mrad_Suppressed", suppressed);
		SetText("mrad_RangeHeader", WidgetManager.Translate("#MRAD_Range"));
		SetText("mrad_DropHeader", WidgetManager.Translate("#MRAD_Drop") + "\nMRAD");
		SetText("mrad_WindHeader", string.Format(WidgetManager.Translate("#MRAD_WindHeader"), MRAD_RangeCardData.FormatNumber(Math.AbsFloat(m_fSignedWind))) + "\nMRAD");
		// Each visible value owns one single-line widget. TextWidget does not
		// advance multi-line rows reliably on the RTTexture surface.
		int firstRow = m_Data.WrapPage(m_iPage, m_iRowsPerPage) * m_iRowsPerPage;
		for (int rowIndex = 0; rowIndex < 12; rowIndex++)
		{
			string rangeText, dropText, windText;
			int dataIndex = firstRow + rowIndex;
			if (rowIndex < m_iRowsPerPage && dataIndex < m_Data.m_aRows.Count())
			{
				MRAD_RangeCardRow dataRow = m_Data.m_aRows[dataIndex];
				rangeText = dataRow.m_iRange.ToString();
				dropText = MRAD_RangeCardData.FormatNumber(dataRow.m_fDropMrad);
				if (dataRow.m_bHasWind)
					windText = MRAD_RangeCardData.FormatNumber(dataRow.m_fWindMrad);
				else
					windText = "вЂ”";
			}
			SetText("mrad_Range" + rowIndex.ToString(), rangeText);
			SetText("mrad_Drop" + rowIndex.ToString(), dropText);
			SetText("mrad_Wind" + rowIndex.ToString(), windText);
		}
		string status;
		if (!m_Data.m_sError.IsEmpty())
			status = WidgetManager.Translate(m_Data.m_sError);
		else if (m_Data.m_bMissingWind)
			status = WidgetManager.Translate("#MRAD_Error_NoWind");
		else if (m_Data.m_bEstimatedWind)
			status = WidgetManager.Translate("#MRAD_EstimatedWind");
		else if (m_bActualWind)
			status = WidgetManager.Translate("#MRAD_ActualWind");
		else
			status = WidgetManager.Translate("#MRAD_ReferenceWind");
		SetText("mrad_Status", status);
		SetText("mrad_Page", string.Format(WidgetManager.Translate("#MRAD_Page"), m_Data.WrapPage(m_iPage, m_iRowsPerPage) + 1, GetNumberOfPages()));
		m_wRtTexture.SetEnabled(false);
	}

	// Public entry points retained for inherited third-party scripts.
	void RecalculateWidget() { m_bDirty = true; if (m_bHeld) RefreshData(); }
	BaseWeaponComponent GetMuzzleFromWeapon() { return MRAD_RangeCardInputResolver.Resolve(m_Character).m_Weapon; }
	string CalculateRadians()
	{
		if (!m_Data)
			return string.Empty;
		if (!m_Data.m_sError.IsEmpty())
			return WidgetManager.Translate(m_Data.m_sError);
		string ranges, drops, winds;
		m_Data.GetPageColumns(m_iPage, m_iRowsPerPage, ranges, drops, winds);
		return m_Data.GetLocalizedBulletName() + "\n\n" + ranges + "\n\nMRAD\n" + drops + "\n\nWind MRAD\n" + winds;
	}
}

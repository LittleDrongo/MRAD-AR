// Non-visual development checks. No singleton settings or player profiles are changed.
class MRAD_QATestSettings : MRAD_Settings
{
	void SetInvalidValuesForTest()
	{
		m_fReferenceWindSpeed = -4.0;
		m_iMaxRange = 1777;
		m_iRowsPerPage = 0;
		m_fFineStepMrad = 0.0;
		m_fCoarseStepMrad = 100.0;
		m_fElevationLimitMrad = -10.0;
		m_fWindageLimitMrad = 100.0;
		Sanitize();
	}
}

[WorkbenchPluginAttribute(name: "MRAD non-visual checks", wbModules: {"ResourceManager"})]
class MRAD_ValidatePlugin : WorkbenchPlugin
{
	protected int m_iChecks;
	protected int m_iFailures;

	protected void Check(bool passed, string name)
	{
		m_iChecks++;
		if (passed)
			Print("MRAD_QA_PASS " + name);
		else
		{
			m_iFailures++;
			Print("MRAD_QA_FAIL " + name, LogLevel.ERROR);
		}
	}

	protected bool Near(float a, float b, float tolerance = 0.00001)
	{
		return Math.AbsFloat(a - b) <= tolerance;
	}

	protected void TestSettings()
	{
		MRAD_QATestSettings settings = new MRAD_QATestSettings();
		Check(Near(settings.GetReferenceWindSpeed(), 4.0), "default reference wind is 4 mps");
		Check(Near(settings.GetFineStepMrad(), 0.1) && Near(settings.GetCoarseStepMrad(), 1.0), "default fine/coarse steps follow FR-2.2");
		Check(settings.GetMaxRange() == 1500 && settings.GetRowsPerPage() == 10, "default notebook bounds");
		Check(!settings.GetUseActualWind(), "reference wind is the default mode");
		settings.SetInvalidValuesForTest();
		Check(Near(settings.GetReferenceWindSpeed(), 0.0), "negative wind cannot enter reference settings");
		Check(settings.GetMaxRange() == 1700, "range cap rounds down to a 100 m row");
		Check(settings.GetRowsPerPage() >= 1, "bad page size cannot divide by zero");
		Check(settings.GetFineStepMrad() >= 0.1 && settings.GetCoarseStepMrad() <= 10.0, "input steps stay inside protocol limits");
		Check(settings.GetElevationLimitMrad() > 0.0 && settings.GetWindageLimitMrad() <= 50.0, "invalid sight limits cannot escape safety bounds");
	}

	protected void TestPages()
	{
		MRAD_RangeCardData data = new MRAD_RangeCardData();
		Check(data.GetPageCount(10) == 1 && data.WrapPage(-1, 10) == 0, "empty notebook keeps one valid page");
		for (int i = 1; i <= 23; i++)
		{
			MRAD_RangeCardRow row = new MRAD_RangeCardRow();
			row.m_iRange = i * 100;
			row.m_fDropMrad = 1.25;
			row.m_fWindMrad = -0.24;
			row.m_bHasWind = true;
			data.m_aRows.Insert(row);
		}
		Check(data.GetPageCount(10) == 3, "23 rows paginate into 10+10+3");
		Check(data.GetPageCount(0) == 23, "zero rows-per-page is guarded");
		Check(data.WrapPage(-1, 10) == 2 && data.WrapPage(3, 10) == 0, "both page boundaries wrap");
		string ranges, drops, winds;
		data.GetPageColumns(2, 10, ranges, drops, winds);
		Check(ranges == "2100\n2200\n2300", "last partial page includes each remaining range exactly once");
		Check(winds == "-0.2\n-0.2\n-0.2", "negative wind sign and fixed precision survive page rendering");
		int page;
		for (int turn = 0; turn < 100; turn++)
			page = data.WrapPage(page + 1, 10);
		Check(page == 1, "100 model-level page changes preserve cyclic state");
		data.GetPageColumns(-1, 10, ranges, drops, winds);
		Check(ranges == "2100\n2200\n2300", "negative page request renders the last page");
	}

	protected void TestAnglesAndFormatting()
	{
		Check(Near(SCR_Math.ConvertFromRadians(0.001, SCR_EOpticsAngleUnits.MILLIRADIANS), 1.0), "engine angle units agree with one MRAD");
		Check(MRAD_ZeroingStateComponent.ToTicks(0.1) == 1 && MRAD_ZeroingStateComponent.ToTicks(-0.1) == -1, "fine steps have symmetric integer representation");
		Check(MRAD_ZeroingStateComponent.ToTicks(3.0) == 30, "three coarse steps represent 3 MRAD");
		Check(MRAD_RangeCardData.FormatNumber(1.0) == "1.0", "whole values retain one decimal");
		Check(MRAD_RangeCardData.FormatNumber(-0.01) == "0.0", "small negative values cannot display negative zero");
		Check(MRAD_RangeCardData.FormatNumber(123.46) == "123.5", "long values are not truncated and round once");
		Check(MRAD_RangeCardData.FormatNumber(-2.34) == "-2.3", "decimal separator and negative sign are stable");
	}

	protected void TestWindBoundaries()
	{
		Check(Near(MRAD_RangeCardCalculator.ProjectWindCorrection("0 0 1", 90, 4), 4), "east wind requires right correction when facing north");
		Check(Near(MRAD_RangeCardCalculator.ProjectWindCorrection("0 0 1", 270, 4), -4), "west wind requires left correction when facing north");
		Check(Near(MRAD_RangeCardCalculator.ProjectWindCorrection("1 0 0", 0, 4), -4), "north wind requires left correction when facing east");
		Check(Near(MRAD_RangeCardCalculator.ProjectWindCorrection("0 0 1", 0, 4), 0) && Near(MRAD_RangeCardCalculator.ProjectWindCorrection("0 0 1", 180, 4), 0), "headwind and tailwind have no lateral correction");
		Check(Near(MRAD_RangeCardCalculator.ProjectWindCorrection("0 0.8 0.6", 90, 4), 4), "view pitch does not change horizontal crosswind");
		Check(Near(MRAD_RangeCardCalculator.ProjectWindCorrection("0 1 0", 90, 4), 0), "vertical view has no arbitrary lateral axis");
		MRAD_RangeCardWindEntry entry = new MRAD_RangeCardWindEntry();
		// Намеренно короткая и несортированная таблица: native binary search
		// предполагал 90 записей и мог обращаться к индексу -1.
		entry.m_aSolutions.Insert(Vector(0.003, 300, 3));
		entry.m_aValues.Insert(Vector(0.6, 0, 0));
		entry.m_aSolutions.Insert(Vector(0.001, 100, 1));
		entry.m_aValues.Insert(Vector(0.2, 0, 0));
		float value;
		Check(entry.TryAngle(0.001, value) && Near(value, 0.2), "wind table first sample is safe");
		Check(entry.TryAngle(0.003, value) && Near(value, 0.6), "wind table last sample is safe");
		Check(entry.TryAngle(0.002, value) && Near(value, 0.4), "sparse unsorted wind samples interpolate");
		Check(!entry.TryAngle(0.0005, value) && !entry.TryAngle(0.004, value), "wind table does not extrapolate outside samples");
		entry.m_aValues.Clear();
		Check(!entry.TryAngle(0.002, value), "mismatched wind arrays fail without indexing");
		MRAD_RangeCardData empty = MRAD_RangeCardCalculator.Build(null, 1000, 4);
		Check(empty.m_aRows.IsEmpty() && !empty.m_sError.IsEmpty(), "missing calculator input has an explicit empty state");
	}

	override void Run()
	{
		RunCommandline();
	}

	override void RunCommandline()
	{
		TestSettings();
		TestPages();
		TestAnglesAndFormatting();
		TestWindBoundaries();
		Check(MRAD_OpticsMathChecks.Run() == 0, "native camera matrix checks");
		PrintFormat("MRAD_VALIDATE_RESULT checks=%1 failures=%2", m_iChecks, m_iFailures);
		int result;
		if (m_iFailures > 0)
			result = 1;
		Workbench.Exit(result);
	}
}

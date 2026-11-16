// Настройки профиля игрока. Бинды сохраняет штатный InputBinding отдельно.
// Явные JSON-поля позволяют добавлять настройки, сохраняя старые профили.
class MRAD_Settings
{
	protected static ref MRAD_Settings s_Instance;
	protected const string DIRECTORY = "$profile:MRAD";
	protected const string FILE_NAME = "$profile:MRAD/settings.json";

	protected float m_fReferenceWindSpeed = 4.0;
	protected bool m_bUseActualWind;
	protected int m_iMaxRange = 1500;
	protected int m_iRowsPerPage = 15;
	protected float m_fFineStepMrad = 0.1;
	protected float m_fCoarseStepMrad = 1.0;
	protected float m_fElevationLimitMrad = 50.0;
	protected float m_fWindageLimitMrad = 50.0;
	protected bool m_bShowHUD = true;
	protected bool m_bShowWindHUD = true;
	protected int m_iRevision = 1;

	static MRAD_Settings Get()
	{
		if (!s_Instance)
		{
			s_Instance = new MRAD_Settings();
			s_Instance.Load();
		}
		return s_Instance;
	}

	float GetReferenceWindSpeed() { return m_fReferenceWindSpeed; }
	bool GetUseActualWind() { return m_bUseActualWind; }
	int GetMaxRange() { return m_iMaxRange; }
	int GetRowsPerPage() { return m_iRowsPerPage; }
	float GetFineStepMrad() { return m_fFineStepMrad; }
	float GetCoarseStepMrad() { return m_fCoarseStepMrad; }
	float GetElevationLimitMrad() { return m_fElevationLimitMrad; }
	float GetWindageLimitMrad() { return m_fWindageLimitMrad; }
	bool GetShowHUD() { return m_bShowHUD; }
	bool GetShowWindHUD() { return m_bShowWindHUD; }
	int GetRevision() { return m_iRevision; }

	//! Toggle of the wind indicator from the MRAD_ToggleWindHUD action; saved at once.
	bool SetShowWindHUD(bool show)
	{
		if (m_bShowWindHUD == show)
			return true;

		m_bShowWindHUD = show;
		m_iRevision++;
		return Save();
	}

	// Изменения действуют сразу; false означает только ошибку сохранения профиля.
	bool Apply(float referenceWindSpeed, bool useActualWind, int maxRange, int rowsPerPage, float fineStep, float coarseStep, float elevationLimit, float windageLimit, bool showHUD, bool showWindHUD)
	{
		m_fReferenceWindSpeed = referenceWindSpeed;
		m_bUseActualWind = useActualWind;
		m_iMaxRange = maxRange;
		m_iRowsPerPage = rowsPerPage;
		m_fFineStepMrad = fineStep;
		m_fCoarseStepMrad = coarseStep;
		m_fElevationLimitMrad = elevationLimit;
		m_fWindageLimitMrad = windageLimit;
		m_bShowHUD = showHUD;
		m_bShowWindHUD = showWindHUD;
		Sanitize();
		m_iRevision++;
		return Save();
	}

	bool RestoreDefaults()
	{
		return Apply(4.0, false, 1500, 15, 0.1, 1.0, 50.0, 50.0, true, true);
	}

	protected void Sanitize()
	{
		m_fReferenceWindSpeed = Math.Round(Math.Clamp(m_fReferenceWindSpeed, 0.0, 30.0) * 10.0) / 10.0;
		m_iMaxRange = Math.ClampInt(m_iMaxRange, 100, 5000);
		m_iMaxRange = (m_iMaxRange / 100) * 100;
		m_iRowsPerPage = Math.ClampInt(m_iRowsPerPage, 5, 15);
		// Состояние оптики хранится целыми десятыми MRAD; другие шаги запрещены.
		m_fFineStepMrad = Math.Round(Math.Clamp(m_fFineStepMrad, 0.1, 5.0) * 10.0) / 10.0;
		m_fCoarseStepMrad = Math.Round(Math.Clamp(m_fCoarseStepMrad, 0.1, 10.0) * 10.0) / 10.0;
		m_fElevationLimitMrad = Math.Round(Math.Clamp(m_fElevationLimitMrad, 1.0, 50.0) * 10.0) / 10.0;
		m_fWindageLimitMrad = Math.Round(Math.Clamp(m_fWindageLimitMrad, 1.0, 50.0) * 10.0) / 10.0;
	}

	protected void Load()
	{
		if (!FileIO.FileExists(FILE_NAME))
			return;

		JsonLoadContext context = new JsonLoadContext();
		if (!context.LoadFromFile(FILE_NAME))
		{
			Print("MRAD: settings.json could not be read; using defaults", LogLevel.WARNING);
			return;
		}

		// Отсутствующее поле оставляет значение по умолчанию, а не обнуляет его.
		context.ReadValueDefault("referenceWindSpeed", m_fReferenceWindSpeed, 4.0);
		context.ReadValueDefault("useActualWind", m_bUseActualWind, false);
		context.ReadValueDefault("maxRange", m_iMaxRange, 1500);
		context.ReadValueDefault("rowsPerPage", m_iRowsPerPage, 15);
		context.ReadValueDefault("fineStepMrad", m_fFineStepMrad, 0.1);
		context.ReadValueDefault("coarseStepMrad", m_fCoarseStepMrad, 1.0);
		context.ReadValueDefault("elevationLimitMrad", m_fElevationLimitMrad, 50.0);
		context.ReadValueDefault("windageLimitMrad", m_fWindageLimitMrad, 50.0);
		context.ReadValueDefault("showHUD", m_bShowHUD, true);
		context.ReadValueDefault("showWindHUD", m_bShowWindHUD, true);
		Sanitize();
	}

	bool Save()
	{
		FileIO.MakeDirectory(DIRECTORY);

		JsonSaveContext context = new JsonSaveContext();
		context.WriteValue("version", 1);
		context.WriteValue("referenceWindSpeed", m_fReferenceWindSpeed);
		context.WriteValue("useActualWind", m_bUseActualWind);
		context.WriteValue("maxRange", m_iMaxRange);
		context.WriteValue("rowsPerPage", m_iRowsPerPage);
		context.WriteValue("fineStepMrad", m_fFineStepMrad);
		context.WriteValue("coarseStepMrad", m_fCoarseStepMrad);
		context.WriteValue("elevationLimitMrad", m_fElevationLimitMrad);
		context.WriteValue("windageLimitMrad", m_fWindageLimitMrad);
		context.WriteValue("showHUD", m_bShowHUD);
		context.WriteValue("showWindHUD", m_bShowWindHUD);
		return context.SaveToFile(FILE_NAME);
	}
}

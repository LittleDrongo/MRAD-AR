// Данные страницы отделены от виджетов: один расчёт можно повторно показать
// после закрытия блокнота и при перелистывании без обращения к баллистике.
class MRAD_RangeCardRow
{
	int m_iRange;
	float m_fDropMrad;
	float m_fWindMrad;
	bool m_bHasWind;
}

class MRAD_RangeCardData
{
	ref array<ref MRAD_RangeCardRow> m_aRows = {};
	string m_sBulletName;
	string m_sBulletDetail;
	string m_sError;
	ResourceName m_sProjectile;
	float m_fWindSpeed;
	bool m_bSuppressed;
	bool m_bMissingWind;
	bool m_bEstimatedWind;

	string GetLocalizedBulletName()
	{
		string name = WidgetManager.Translate(m_sBulletName);
		if (!m_sBulletDetail.IsEmpty())
			name += " " + WidgetManager.Translate(m_sBulletDetail);
		return name;
	}

	int GetPageCount(int rowsPerPage)
	{
		rowsPerPage = Math.Max(1, rowsPerPage);
		return Math.Max(1, (m_aRows.Count() + rowsPerPage - 1) / rowsPerPage);
	}

	// Приводит и отрицательный индекс к допустимой странице.
	int WrapPage(int page, int rowsPerPage)
	{
		int count = GetPageCount(rowsPerPage);
		return ((page % count) + count) % count;
	}

	static string FormatNumber(float value)
	{
		// Убираем отрицательный ноль; формат не зависит от локали ОС.
		if (Math.AbsFloat(value) < 0.05)
			value = 0.0;
		return value.ToString(-1, 1);
	}

	void GetPageColumns(int page, int rowsPerPage, out string ranges, out string drops, out string winds)
	{
		ranges = string.Empty;
		drops = string.Empty;
		winds = string.Empty;
		rowsPerPage = Math.Max(1, rowsPerPage);
		int first = WrapPage(page, rowsPerPage) * rowsPerPage;
		int end = Math.Min(first + rowsPerPage, m_aRows.Count());
		for (int i = first; i < end; i++)
		{
			MRAD_RangeCardRow row = m_aRows[i];
			if (i > first)
			{
				ranges += "\n";
				drops += "\n";
				winds += "\n";
			}
			ranges += row.m_iRange.ToString();
			drops += FormatNumber(row.m_fDropMrad);
			if (row.m_bHasWind)
				winds += FormatNumber(row.m_fWindMrad);
			else
				winds += "—";
		}
	}
}

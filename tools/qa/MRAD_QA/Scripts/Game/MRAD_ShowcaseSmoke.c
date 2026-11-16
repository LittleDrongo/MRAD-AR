// QA-only smoke check of the original showcase world, without editing that world.
// The explicit CLI flag is required; normal game modes receive no extra work.
modded class SCR_BaseGameMode
{
	protected override void OnGameStart()
	{
		super.OnGameStart();
		if (System.IsCLIParam("mradShowcaseSmoke"))
		{
			Print("MRAD_SHOWCASE_BEGIN native game mode started");
			GetGame().GetCallqueue().CallLater(MRAD_ShowcaseSmoke.Run, 5000, false);
		}
	}
}

class MRAD_ShowcaseSmoke
{
	protected int m_iNotebooks;
	protected int m_iFailures;

	protected bool InspectEntity(IEntity entity)
	{
		Drg_NotepadRtComponent notebook = Drg_NotepadRtComponent.Cast(entity.FindComponent(Drg_NotepadRtComponent));
		if (!notebook)
			return true;
		m_iNotebooks++;
		MRAD_NotepadGadgetComponent gadget = MRAD_NotepadGadgetComponent.Cast(entity.FindComponent(MRAD_NotepadGadgetComponent));
		Physics body = entity.GetPhysics();
		int geometries;
		if (body)
			geometries = body.GetNumGeoms();
		bool valid = gadget && geometries > 0 && (!System.IsConsoleApp() || !notebook.HasRenderTarget());
		PrintFormat("MRAD_SHOWCASE_NOTEBOOK position=%1 gadget=%2 geometries=%3 hasRT=%4 valid=%5", entity.GetOrigin(), gadget != null, geometries, notebook.HasRenderTarget(), valid);
		if (!valid)
		{
			m_iFailures++;
			Print("MRAD_SHOWCASE_FAIL placed notebook lacks its gadget/collider or allocated a headless RT", LogLevel.ERROR);
		}
		return true;
	}

	protected void Inspect()
	{
		// Arland is within this bounded region. Query actual loaded entities,
		// not source-file text or newly spawned stand-ins.
		GetGame().GetWorld().QueryEntitiesByAABB("-10000 -2000 -10000", "10000 10000 10000", InspectEntity, null, EQueryEntitiesFlags.ALL);
		if (m_iNotebooks == 0)
		{
			m_iFailures++;
			Print("MRAD_SHOWCASE_FAIL original world contains no queryable MRAD notebooks", LogLevel.ERROR);
		}
		PrintFormat("MRAD_SHOWCASE_RESULT notebooks=%1 failures=%2", m_iNotebooks, m_iFailures);
		GetGame().GetCallqueue().CallLater(MRAD_ShowcaseSmoke.Close, 100, false);
	}

	static void Close() { GetGame().RequestClose(); }

	static void Run()
	{
		MRAD_ShowcaseSmoke probe = new MRAD_ShowcaseSmoke();
		probe.Inspect();
	}
}

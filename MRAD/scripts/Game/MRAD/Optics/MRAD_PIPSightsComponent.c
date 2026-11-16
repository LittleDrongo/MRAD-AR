[EntityEditorProps(category: "GameScripted/Weapon/Sights", description: "MRAD PIP sights; add MRAD_ZeroingStateComponent on the same entity", color: "0 0 255 255")]
class MRAD_PIPSightsComponentClass : SCR_2DPIPSightsComponentClass
{
}

class MRAD_PIPSightsComponent : SCR_2DPIPSightsComponent
{
	protected MRAD_ZeroingStateComponent m_MRADState;
	protected ref MRAD_OpticsInputController m_MRADInput;

	override void OnInit(IEntity owner)
	{
		super.OnInit(owner);
		m_MRADState = MRAD_ZeroingStateComponent.FindForSights(this);
		if (!m_MRADState && !SCR_Global.IsEditMode())
			Print("MRAD: MRAD_PIPSightsComponent requires MRAD_ZeroingStateComponent on the same entity", LogLevel.WARNING);
	}

	override void OnSightADSActivated()
	{
		super.OnSightADSActivated();
		if (SCR_Global.IsEditMode())
			return;
		if (!m_MRADInput)
			m_MRADInput = new MRAD_OpticsInputController(this);
		m_MRADInput.Activate();
	}

	override void OnSightADSDeactivated()
	{
		if (m_MRADInput)
			m_MRADInput.Deactivate();
		super.OnSightADSDeactivated();
	}

	override void OnSightADSPostFrame(IEntity owner, float timeSlice)
	{
		super.OnSightADSPostFrame(owner, timeSlice);
		if (m_MRADInput)
			m_MRADInput.Update(timeSlice);
	}

	override IEntity GetCameraLocalTransform(out vector matrix[4])
	{
		IEntity parent = super.GetCameraLocalTransform(matrix);
		if (!SCR_Global.IsEditMode() && IsSightADSActive())
		{
			if (!m_MRADState)
				m_MRADState = MRAD_ZeroingStateComponent.FindForSights(this);
			if (m_MRADState)
				m_MRADState.ApplyCameraRotation(matrix);
		}
		return parent;
	}

	override vector GetMisalignmentAngles(notnull CameraBase camera)
	{
		vector angles = super.GetMisalignmentAngles(camera);
		if (m_MRADState && IsSightADSActive() && !SCR_Global.IsEditMode())
			return m_MRADState.ApplyReticleReferenceRotation(angles);
		return angles;
	}

	override void OnDelete(IEntity owner)
	{
		if (m_MRADInput)
			m_MRADInput.Deactivate();
		m_MRADInput = null;
		super.OnDelete(owner);
	}
}

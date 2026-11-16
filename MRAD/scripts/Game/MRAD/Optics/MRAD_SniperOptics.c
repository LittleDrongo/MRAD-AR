// MRAD adjustment for every sniper-type optic, without touching any prefab.
//
// All magnified scopes in the game (vanilla and mods) are SCR_2DOpticsComponent
// (SCR_2DSightsComponent / SCR_2DPIPSightsComponent). A scope counts as sniper type when:
//  - it sits on a sniper rifle (EWeaponType.WT_SNIPERRIFLE, vanilla LongRangeRifle_Base) and magnifies 2x or more;
//  - or it sits on a rifle / machine gun and magnifies MRAD_MIN_ZOOM_OTHER (6x) or more.
//
// 0 MRAD = sight line parallel to the bore: the vanilla range table of the scope is not used
// (no "300 m"). Elevation turns the optic camera through the vanilla camera-pitch path
// (the same path EPZ_CAMERA_TURN scopes use for their range table), so 2D and PIP
// scopes, the reticle and the misalignment all follow it. Windage adds the matching yaw.
// Values are kept per scope item on the local machine: only the shooter's view changes,
// the shot itself is the normal network-synced aim.
// Per-prefab switch: an optic with its own reticle/range table (PSO-4 and similar)
// can be excluded from MRAD by ticking "Exclude from MRAD" on its sights component,
// or by adding the empty MRAD_SightExclusionComponent marker.
modded class SCR_2DOpticsComponentClass
{
	[Attribute("0", UIWidgets.CheckBox, "Exclude this optic from MRAD: keep its own reticle and range table", category: "MRAD")]
	bool m_bMRADExcluded;
}

[ComponentEditorProps(category: "GameScripted/MRAD", description: "Marks an optic prefab as excluded from MRAD")]
class MRAD_SightExclusionComponentClass : ScriptComponentClass
{
}

class MRAD_SightExclusionComponent : ScriptComponent
{
}

modded class SCR_2DOpticsComponent
{
	static const float MRAD_MIN_ZOOM_SNIPER = 2.0;
	static const float MRAD_MIN_ZOOM_OTHER = 6.0;
	static const int MRAD_TICKS_PER_MRAD = 10;

	protected int m_iMRADElevationTicks;
	protected int m_iMRADWindageTicks;
	protected bool m_bMRADActive;
	protected ref MRAD_OpticsInputController m_MRADInput;

	protected IEntity m_MRADCheckedWeapon;
	protected bool m_bMRADCheckedResult;

	//------------------------------------------------------------------------------------------------
	//! True while this scope is in ADS and works in MRAD mode.
	bool MRAD_IsActive()
	{
		return m_bMRADActive;
	}

	//------------------------------------------------------------------------------------------------
	int MRAD_GetElevationTicks()
	{
		return m_iMRADElevationTicks;
	}

	//------------------------------------------------------------------------------------------------
	int MRAD_GetWindageTicks()
	{
		return m_iMRADWindageTicks;
	}

	//------------------------------------------------------------------------------------------------
	float MRAD_GetElevationMrad()
	{
		return m_iMRADElevationTicks / (float)MRAD_TICKS_PER_MRAD;
	}

	//------------------------------------------------------------------------------------------------
	float MRAD_GetWindageMrad()
	{
		return m_iMRADWindageTicks / (float)MRAD_TICKS_PER_MRAD;
	}

	//------------------------------------------------------------------------------------------------
	void MRAD_SetTicks(int elevationTicks, int windageTicks)
	{
		m_iMRADElevationTicks = elevationTicks;
		m_iMRADWindageTicks = windageTicks;
		if (m_bMRADActive)
			m_fCurrentCameraPitch = GetCameraPitchTarget();
	}

	//------------------------------------------------------------------------------------------------
	protected static float MRAD_TicksToDegrees(int ticks)
	{
		return ticks / (float)MRAD_TICKS_PER_MRAD * 0.001 * Math.RAD2DEG;
	}

	//------------------------------------------------------------------------------------------------
	BaseWeaponComponent MRAD_FindWeapon()
	{
		IEntity node = GetOwner();
		while (node)
		{
			BaseWeaponComponent weapon = BaseWeaponComponent.Cast(node.FindComponent(BaseWeaponComponent));
			if (weapon)
				return weapon;

			node = node.GetParent();
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	float MRAD_GetMaxZoom()
	{
		float zoom = GetMagnification();
		SCR_SightsZoomFOVInfo zoomInfo = SCR_SightsZoomFOVInfo.Cast(GetFOVInfo());
		if (zoomInfo)
		{
			array<float> zooms = zoomInfo.ZoomsToArray();
			foreach (float step : zooms)
			{
				zoom = Math.Max(zoom, step);
			}
		}

		return zoom;
	}

	//------------------------------------------------------------------------------------------------
	//! Sniper-type scope on a hand-held weapon. Result is cached per weapon the scope is mounted on.
	//! Optic prefab opted out of MRAD (checkbox on the sights component or the marker).
	bool MRAD_IsExcluded()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return false;

		if (owner.FindComponent(MRAD_SightExclusionComponent))
			return true;

		SCR_2DOpticsComponentClass data = SCR_2DOpticsComponentClass.Cast(GetComponentData(owner));
		return data && data.m_bMRADExcluded;
	}

	//------------------------------------------------------------------------------------------------
	bool MRAD_IsSniperOptic()
	{
		// Исключение статично для префаба и проверяется до кэша.
		if (MRAD_IsExcluded())
			return false;

		BaseWeaponComponent weapon = MRAD_FindWeapon();
		if (!weapon)
			return false;

		IEntity weaponEntity = weapon.GetOwner();
		if (weaponEntity == m_MRADCheckedWeapon && weaponEntity)
			return m_bMRADCheckedResult;

		bool result;
		float zoom = MRAD_GetMaxZoom();
		switch (weapon.GetWeaponType())
		{
			case EWeaponType.WT_SNIPERRIFLE:
				result = zoom >= MRAD_MIN_ZOOM_SNIPER;
				break;

			case EWeaponType.WT_RIFLE:
			case EWeaponType.WT_MACHINEGUN:
				result = zoom >= MRAD_MIN_ZOOM_OTHER;
				break;
		}

		m_MRADCheckedWeapon = weaponEntity;
		m_bMRADCheckedResult = result;
		return result;
	}

	//------------------------------------------------------------------------------------------------
	//! This scope is the current sights of the locally controlled character.
	bool MRAD_IsLocalPlayerSights()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player)
			return false;

		BaseWeaponManagerComponent manager = BaseWeaponManagerComponent.Cast(player.FindComponent(BaseWeaponManagerComponent));
		return manager && manager.GetCurrentSights() == this;
	}

	//------------------------------------------------------------------------------------------------
	override void OnSightADSActivated()
	{
		m_bMRADActive = !SCR_Global.IsEditMode() && MRAD_IsSniperOptic() && MRAD_IsLocalPlayerSights();
		if (m_bMRADActive)
			m_fCurrentCameraPitch = GetCameraPitchTarget();

		super.OnSightADSActivated();

		if (!m_bMRADActive)
			return;

		if (!m_MRADInput)
			m_MRADInput = new MRAD_OpticsInputController(this);

		m_MRADInput.Activate();
	}

	//------------------------------------------------------------------------------------------------
	override void OnSightADSDeactivated()
	{
		if (m_MRADInput)
			m_MRADInput.Deactivate();

		super.OnSightADSDeactivated();
		m_bMRADActive = false;
	}

	//------------------------------------------------------------------------------------------------
	protected override void OnSightADSPostFrame(IEntity owner, float timeSlice)
	{
		super.OnSightADSPostFrame(owner, timeSlice);

		if (!m_bMRADActive)
			return;

		// A turret click is instant: no interpolation towards the target angle.
		m_fCurrentCameraPitch = GetCameraPitchTarget();

		if (m_MRADInput)
			m_MRADInput.Update(timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla: EPZ_CAMERA_TURN scopes return -rangeAngle (camera down = shot up).
	//! MRAD: the dialled elevation only, the scope's own range table is ignored.
	protected override float GetCameraPitchTarget()
	{
		if (!m_bMRADActive)
			return super.GetCameraPitchTarget();

		return -MRAD_TicksToDegrees(m_iMRADElevationTicks);
	}

	//------------------------------------------------------------------------------------------------
	protected float MRAD_GetYawOffset()
	{
		if (!m_bMRADActive)
			return 0;

		// Positive windage moves the point of impact right: camera turns left.
		return -MRAD_TicksToDegrees(m_iMRADWindageTicks);
	}

	//------------------------------------------------------------------------------------------------
	//! Same as vanilla, plus the windage yaw. Without windage the vanilla method runs unchanged.
	override IEntity GetCameraLocalTransform(out vector matrix[4])
	{
		float yawOffset = MRAD_GetYawOffset();
		if (yawOffset == 0)
			return super.GetCameraLocalTransform(matrix);

		IEntity parent = GetOwner();
		SCR_2DOpticsComponentClass data = SCR_2DOpticsComponentClass.Cast(GetComponentData(parent));
		if (!data)
			return parent;

		vector position = data.GetCameraOffset() + GetSightsFrontPosition(true);
		vector cameraAngles = data.GetCameraAngles();
		vector yawPitchRoll = Vector(cameraAngles[1] + yawOffset, cameraAngles[0] + m_fCurrentCameraPitch, cameraAngles[2]);

		Math3D.AnglesToMatrix(yawPitchRoll, matrix);
		matrix[3] = position;

		vector temp[4];
		Math3D.MatrixCopy(matrix, temp);

		IEntity lastNode = parent;
		BaseWeaponComponent weaponRoot;
		while (lastNode)
		{
			weaponRoot = BaseWeaponComponent.Cast(lastNode.FindComponent(BaseWeaponComponent));
			if (weaponRoot)
				break;

			vector localTransform[4];
			lastNode.GetLocalTransform(localTransform);

			TNodeId pivotIndex = lastNode.GetPivot();
			if (pivotIndex != -1)
			{
				IEntity parentNode = lastNode.GetParent();
				if (parentNode)
				{
					vector pivotTM[4];
					parentNode.GetAnimation().GetBoneMatrix(pivotIndex, pivotTM);
					if (!SCR_Math3D.IsMatrixEmpty(pivotTM))
						Math3D.MatrixMultiply4(pivotTM, localTransform, localTransform);
				}
			}

			Math3D.MatrixMultiply4(temp, localTransform, temp);
			lastNode = lastNode.GetParent();
		}

		if (weaponRoot)
		{
			Math3D.MatrixCopy(temp, matrix);
			parent = lastNode;
		}

		return parent;
	}

	//------------------------------------------------------------------------------------------------
	//! Same as vanilla, plus the windage yaw, so the 2D reticle stays on the turned sight line.
	override vector GetMisalignmentAngles(notnull CameraBase camera)
	{
		float yawOffset = MRAD_GetYawOffset();
		if (yawOffset == 0)
			return super.GetMisalignmentAngles(camera);

		SCR_2DOpticsComponentClass data = SCR_2DOpticsComponentClass.Cast(GetComponentData(GetOwner()));
		if (!data)
			return vector.Zero;

		vector cameraToSightMat[4];
		SCR_EntityHelper.GetRelativeLocalTransform(GetOwner(), camera, cameraToSightMat);

		vector cameraAngles = data.GetCameraAngles();
		vector sightMat[3];
		vector sightAngles = {-cameraAngles[1] - yawOffset, -cameraAngles[0] - m_fCurrentCameraPitch, 0};
		Math3D.AnglesToMatrix(sightAngles, sightMat);

		Math3D.MatrixMultiply3(sightMat, cameraToSightMat, sightMat);

		vector turretMat[4];
		GetSightsTransform(turretMat, true);
		Math3D.MatrixInvMultiply3(sightMat, turretMat, sightMat);

		vector angles = Math3D.MatrixToAngles(sightMat);
		angles[2] = angles[2] + cameraAngles[2];
		return SCR_Math3D.FixEulerVector180(angles);
	}

	//------------------------------------------------------------------------------------------------
	override void OnDelete(IEntity owner)
	{
		if (m_MRADInput)
			m_MRADInput.Deactivate();

		m_MRADInput = null;
		super.OnDelete(owner);
	}
}

//------------------------------------------------------------------------------------------------
// Reticle-offset scopes (PSO-1 etc.) move the reticle by their range table: not in MRAD mode.
modded class SCR_2DSightsComponent
{
	protected override float GetReticleOffsetYTarget()
	{
		if (MRAD_IsActive())
			return m_fReticleOffsetY;

		return super.GetReticleOffsetYTarget();
	}
}

// Optional isolated-engine experiment: observe real native shots; never call Launch.
// Enabled only by -mradNativeFire on the QA runtime. The supplied entities are owned
// by MRAD_RuntimeProbe; this observer restores the registered attachment before returning.
class MRAD_NativeFireProbe
{
	protected MRAD_RuntimeProbe m_Owner;
	protected CharacterControllerComponent m_Controller;
	protected EventHandlerManagerComponent m_Events;
	protected BaseWeaponComponent m_Weapon;
	protected SCR_MuzzleEffectComponent m_MuzzleEffect;
	protected SCR_MuzzleEffectComponent m_SuppressorEffect;
	protected BaseWeaponStatsManagerComponent m_Stats;
	protected SCR_WeaponAttachmentSuppressorAttributes m_Attributes;
	protected IEntity m_Suppressor;
	protected int m_iMuzzleIndex;
	protected int m_iPhase;
	protected int m_iTicks;
	protected int m_iCooldown = 10;
	protected int m_iFailures;
	protected bool m_bActive;
	protected bool m_bPhaseComplete;
	protected bool m_bOriginalDisableControls;
	protected bool m_bControlsAdjusted;
	protected bool m_bTriggerPressed;
	protected float m_fOriginalCoefficient;
	protected ref array<float> m_aOriginalSpeeds = {};
	protected ref array<float> m_aModifiedSpeeds = {};
	protected ref array<IEntity> m_aProjectiles = {};

	void Start(MRAD_RuntimeProbe owner, ChimeraCharacter character, BaseWeaponStatsManagerComponent stats, int muzzleIndex, IEntity suppressor, SCR_WeaponAttachmentSuppressorAttributes attributes)
	{
		m_Owner = owner;
		m_Stats = stats;
		m_iMuzzleIndex = muzzleIndex;
		m_Suppressor = suppressor;
		m_Attributes = attributes;
		if (character)
		{
			m_Controller = CharacterControllerComponent.Cast(character.FindComponent(CharacterControllerComponent));
			m_Events = EventHandlerManagerComponent.Cast(character.FindComponent(EventHandlerManagerComponent));
		}
		if (!m_Controller || !m_Events || !m_Stats || !m_Suppressor || !m_Attributes || muzzleIndex < 0 || !m_Stats.GetMuzzleVelocityCoefficient(muzzleIndex, m_fOriginalCoefficient) || m_fOriginalCoefficient <= 0)
		{
			Fail("native shot fixture lacks controller, event manager or registered attachment");
			Finish();
			return;
		}
		m_bActive = true;
		CharacterWeaponManagerComponent manager = CharacterWeaponManagerComponent.Cast(character.FindComponent(CharacterWeaponManagerComponent));
		if (manager)
		{
			array<WeaponSlotComponent> slots = {};
			manager.GetWeaponsSlots(slots);
			foreach (WeaponSlotComponent slot : slots)
			{
				if (slot && slot.GetWeaponSlotIndex() == 0 && slot.GetWeaponEntity())
				{
					m_Weapon = slot;
					bool selected = manager.SelectWeapon(slot);
					PrintFormat("MRAD_NATIVE_FIRE_SELECT selected=%1 current=%2", selected, manager.GetCurrentWeapon());
					break;
				}
			}
		}
		m_bOriginalDisableControls = m_Controller.GetDisableWeaponControls();
		m_bControlsAdjusted = true;
		m_Controller.SetDisableWeaponControls(false);
		m_Controller.SetWeaponNoFireTime(0);
		// Match vanilla's delayed character event registration. Dedicated NPCs did
		// not deliver the synchronous variant even though magazine count decreased.
		m_Events.RegisterScriptHandler("OnProjectileShot", this, OnProjectileShot);
		// These exact components are declared by the native M16 and suppressor prefabs.
		// Their OnFired invoker receives the real projectile directly from the engine.
		if (m_Weapon && m_Weapon.GetCurrentMuzzle())
		{
			GenericComponent muzzleTree = GenericComponent.Cast(m_Weapon.GetCurrentMuzzle());
			if (muzzleTree)
				m_MuzzleEffect = SCR_MuzzleEffectComponent.Cast(muzzleTree.FindComponent(SCR_MuzzleEffectComponent));
		}
		m_SuppressorEffect = SCR_MuzzleEffectComponent.Cast(m_Suppressor.FindComponent(SCR_MuzzleEffectComponent));
		if (m_MuzzleEffect)
			m_MuzzleEffect.GetOnWeaponFired().Insert(OnMuzzleFired);
		if (m_SuppressorEffect && m_SuppressorEffect != m_MuzzleEffect)
			m_SuppressorEffect.GetOnWeaponFired().Insert(OnMuzzleFired);
		PrintFormat("MRAD_NATIVE_FIRE_OBSERVERS muzzle=%1 suppressor=%2 delayedCharacterEvent=1", m_MuzzleEffect, m_SuppressorEffect);
		m_Controller.SetWeaponRaised(true);
		bool safetyChanged = m_Controller.SetSafety(false, false);
		PrintFormat("MRAD_NATIVE_FIRE_BEGIN originalCoefficient=%1 safetyChanged=%2", m_fOriginalCoefficient, safetyChanged);
		GetGame().GetCallqueue().CallLater(Tick, 100, true);
	}

	protected void Fail(string reason)
	{
		m_iFailures++;
		Print("MRAD_NATIVE_FIRE_FAIL " + reason, LogLevel.ERROR);
	}

	protected void SetTrigger(bool pressed)
	{
		m_bTriggerPressed = pressed;
		if (!m_Controller)
			return;
		m_Controller.SetFireWeaponWanted(pressed);
		CharacterInputContext input = m_Controller.GetInputContext();
		if (input)
		{
			input.SetRaiseWeapon(true);
			input.SetWeaponPullTrigger(pressed);
		}
	}

	protected void Tick()
	{
		if (!m_bActive)
			return;
		m_iTicks++;
		CharacterInputContext input = m_Controller.GetInputContext();
		if (input)
			input.SetRaiseWeapon(true);
		if (m_iTicks == 1 || Math.Mod(m_iTicks, 20) == 0)
		{
			PrintFormat("MRAD_NATIVE_FIRE_STATE tick=%1 canFire=%2 permitted=%3 raised=%4 disabled=%5 dead=%6 unconscious=%7", m_iTicks, m_Controller.CanFire(), m_Controller.GetCanFireWeapon(), m_Controller.IsWeaponRaised(), m_Controller.GetDisableWeaponControls(), m_Controller.IsDead(), m_Controller.IsUnconscious());
			if (input)
				PrintFormat("MRAD_NATIVE_FIRE_INPUT raised=%1 trigger=%2", input.WeaponIsRaised(), input.WeaponIsPullingTrigger());
			BaseMuzzleComponent muzzle;
			if (m_Weapon)
				muzzle = m_Weapon.GetCurrentMuzzle();
			if (muzzle)
				PrintFormat("MRAD_NATIVE_FIRE_MUZZLE chambered=%1 ammo=%2 firemode=%3", muzzle.IsCurrentBarrelChambered(), muzzle.GetAmmoCount(), m_Weapon.GetCurrentFireModeName());
		}
		if (m_iTicks > 120)
		{
			Fail("timed out while waiting for three real shots at each coefficient");
			Finish();
			return;
		}
		if (m_bPhaseComplete)
		{
			SetTrigger(false);
			if (m_iPhase == 1)
			{
				Finish();
				return;
			}
			if (!m_Stats.SetMuzzleVelocityCoefficient(m_Suppressor, m_iMuzzleIndex, 0.8))
			{
				Fail("registered suppressor rejected the temporary coefficient");
				Finish();
				return;
			}
			m_iPhase = 1;
			m_bPhaseComplete = false;
			m_iCooldown = 5;
		}
		if (m_iCooldown > 0)
		{
			SetTrigger(false);
			m_iCooldown--;
			return;
		}
		// Single-fire must receive a release independently of event delivery.
		if (m_bTriggerPressed)
		{
			SetTrigger(false);
			m_iCooldown = 3;
			return;
		}
		m_Controller.SetWeaponRaised(true);
		SetTrigger(true);
	}

	protected void OnProjectileShot(int playerID, BaseWeaponComponent weapon, IEntity entity)
	{
		ObserveProjectile(entity, true, "character_event");
	}

	protected void OnMuzzleFired(IEntity effectEntity, BaseMuzzleComponent muzzle, IEntity projectileEntity)
	{
		if (!m_bActive)
			return;
		// The subscription itself belongs exclusively to the fixture's two components.
		// Log native arguments before null filtering: a null projectile must not be
		// mistaken for the absence of a native OnFired callback.
		PrintFormat("MRAD_NATIVE_FIRE_MUZZLE_EVENT effect=%1 muzzle=%2 projectile=%3", effectEntity, muzzle, projectileEntity);
		ObserveProjectile(projectileEntity, false, "muzzle_effect");
	}

	protected void ObserveProjectile(IEntity entity, bool delayedEvent, string eventSource)
	{
		if (!m_bActive || !entity)
			return;
		if (m_aProjectiles.Find(entity) >= 0)
			return;
		m_aProjectiles.Insert(entity);
		SetTrigger(false);
		m_iCooldown = 3;
		if (m_bPhaseComplete)
			return;
		ProjectileMoveComponent movement = ProjectileMoveComponent.Cast(entity.FindComponent(ProjectileMoveComponent));
		if (!movement)
		{
			Fail("native projectile event did not expose a ProjectileMoveComponent");
			m_bPhaseComplete = true;
			return;
		}
		vector velocity = movement.GetVelocity();
		PrintFormat("MRAD_NATIVE_FIRE_EVENT phase=%1 velocity=%2 source=%3 delayedEvent=%4", m_iPhase, velocity, eventSource, delayedEvent);
		if (velocity.Length() > 1.0)
			RecordShot(entity, m_iPhase, delayedEvent);
		else
			GetGame().GetCallqueue().CallLater(RecordShot, 0, false, entity, m_iPhase, true);
	}

	protected void RecordShot(IEntity projectile, int phase, bool delayed)
	{
		if (!m_bActive || phase != m_iPhase || m_bPhaseComplete)
			return;
		ProjectileMoveComponent movement;
		if (projectile)
			movement = ProjectileMoveComponent.Cast(projectile.FindComponent(ProjectileMoveComponent));
		if (!movement || movement.GetVelocity().Length() <= 1.0)
		{
			Fail("native projectile velocity unavailable at event and next callqueue turn");
			m_bPhaseComplete = true;
			return;
		}
		float speed = movement.GetVelocity().Length();
		float coefficient;
		m_Stats.GetMuzzleVelocityCoefficient(m_iMuzzleIndex, coefficient);
		PrintFormat("MRAD_NATIVE_FIRE_SHOT phase=%1 coefficient=%2 speed=%3 delayed=%4 bulletCoefficient=%5", phase, coefficient, speed, delayed, movement.GetBulletSpeedCoef());
		if (phase == 0)
		{
			m_aOriginalSpeeds.Insert(speed);
			m_bPhaseComplete = m_aOriginalSpeeds.Count() >= 3;
		}
		else
		{
			m_aModifiedSpeeds.Insert(speed);
			m_bPhaseComplete = m_aModifiedSpeeds.Count() >= 3;
		}
	}

	protected float Mean(array<float> values)
	{
		if (values.IsEmpty())
			return 0;
		float sum;
		foreach (float value : values)
			sum += value;
		return sum / values.Count();
	}

	protected void Finish()
	{
		m_bActive = false;
		GetGame().GetCallqueue().Remove(Tick);
		GetGame().GetCallqueue().Remove(RecordShot);
		SetTrigger(false);
		if (m_Controller && m_bControlsAdjusted)
			m_Controller.SetDisableWeaponControls(m_bOriginalDisableControls);
		if (m_Events)
			m_Events.RemoveScriptHandler("OnProjectileShot", this, OnProjectileShot);
		if (m_MuzzleEffect)
			m_MuzzleEffect.GetOnWeaponFired().Remove(OnMuzzleFired);
		if (m_SuppressorEffect && m_SuppressorEffect != m_MuzzleEffect)
			m_SuppressorEffect.GetOnWeaponFired().Remove(OnMuzzleFired);
		bool restored;
		if (m_Stats && m_Attributes && m_Suppressor && m_iMuzzleIndex >= 0)
			restored = m_Attributes.ApplyModifiers(m_Stats, m_iMuzzleIndex, m_Suppressor);
		if (!restored)
			Fail("native attachment attributes could not be restored");
		float originalMean = Mean(m_aOriginalSpeeds);
		float modifiedMean = Mean(m_aModifiedSpeeds);
		float ratio;
		float expectedRatio;
		if (originalMean > 0 && m_fOriginalCoefficient > 0)
		{
			ratio = modifiedMean / originalMean;
			expectedRatio = 0.8 / m_fOriginalCoefficient;
		}
		if (m_aOriginalSpeeds.Count() != 3 || m_aModifiedSpeeds.Count() != 3)
			Fail("native coefficient comparison lacks three observed shots per state");
		else if (Math.AbsFloat(ratio - expectedRatio) > 0.04)
			Fail("observed native velocity ratio does not match the registered coefficient ratio");
		PrintFormat("MRAD_NATIVE_FIRE_RESULT originalShots=%1 modifiedShots=%2 originalMean=%3 modifiedMean=%4 ratio=%5 expectedRatio=%6 restored=%7 failures=%8", m_aOriginalSpeeds.Count(), m_aModifiedSpeeds.Count(), originalMean, modifiedMean, ratio, expectedRatio, restored, m_iFailures);
		foreach (IEntity projectile : m_aProjectiles)
		{
			if (projectile && !projectile.IsDeleted())
				delete projectile;
		}
		m_aProjectiles.Clear();
		if (m_Owner)
			m_Owner.OnNativeFireFinished(m_iFailures);
	}
}

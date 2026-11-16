// Снимок только игровых данных. Никаких внешних таблиц оружия мод не содержит.
class MRAD_RangeCardInput
{
	BaseWeaponComponent m_Weapon;
	IEntity m_WeaponEntity;
	BaseMuzzleComponent m_Muzzle;
	BaseMagazineComponent m_Magazine;
	ref Resource m_Resource;
	IEntitySource m_Source;
	ResourceName m_sProjectile;
	string m_sBulletName;
	string m_sBulletDetail;
	string m_sError;
	float m_fSpeedCoef = 1.0;
	float m_fInitSpeed;
	bool m_bSuppressed;

	string GetKey()
	{
		return string.Format("%1|%2|%3", m_sProjectile, m_fSpeedCoef.ToString(0, 6), m_bSuppressed);
	}
}

class MRAD_RangeCardInputResolver
{
	// GetCharacterOwner() у гаджета сохраняет правильного персонажа и в машине.
	// GetRootParent() в этом случае может вернуть транспорт.
	static MRAD_RangeCardInput Resolve(ChimeraCharacter character)
	{
		MRAD_RangeCardInput result = new MRAD_RangeCardInput();
		result.m_sError = "#MRAD_Error_NoWeapon";
		if (!character)
			return result;

		CharacterWeaponManagerComponent manager = CharacterWeaponManagerComponent.Cast(character.FindComponent(CharacterWeaponManagerComponent));
		if (!manager)
			return result;

		array<WeaponSlotComponent> slots = {};
		manager.GetWeaponsSlots(slots);
		foreach (WeaponSlotComponent slot : slots)
		{
			if (!slot || slot.GetWeaponSlotIndex() != 0)
				continue;
			result.m_WeaponEntity = slot.GetWeaponEntity();
			if (result.m_WeaponEntity)
				result.m_Weapon = slot;
			break;
		}

		// Пустой primary не заменяем другим слотом или оружием в руках.
		if (!result.m_Weapon)
			return result;

		result.m_Muzzle = result.m_Weapon.GetCurrentMuzzle();
		if (!result.m_Muzzle)
		{
			result.m_sError = "#MRAD_Error_NoMuzzle";
			return result;
		}

		result.m_Magazine = result.m_Muzzle.GetMagazine();
		if (result.m_Magazine)
			result.m_sProjectile = result.m_Magazine.GetAmmoType(0);
		else
			result.m_sProjectile = result.m_Muzzle.GetDefaultMagazineOrProjectileName();

		if (!result.m_sProjectile.IsEmpty())
			result.m_Resource = Resource.Load(result.m_sProjectile);
		if (result.m_Resource && result.m_Resource.IsValid() && result.m_Resource.GetResource())
			result.m_Source = result.m_Resource.GetResource().ToEntitySource();

		if (!result.m_Source || !ReadProjectileSpeed(result.m_Source, result.m_fInitSpeed))
		{
			// Default может быть магазином. Его содержимое не выдаём за заряженный
			// патрон: для обычной винтовки без магазина сохраняется ошибка.
			if (!result.m_Magazine)
				result.m_sError = "#MRAD_Error_NoMagazine";
			else
				result.m_sError = "#MRAD_Error_NoBallistics";
			return result;
		}

		result.m_bSuppressed = result.m_Muzzle.IsMuzzleSuppressed();
		result.m_fSpeedCoef = result.m_Muzzle.GetBulletInitSpeedCoef();
		array<BaseMuzzleComponent> muzzles = {};
		result.m_Weapon.GetMuzzlesList(muzzles);
		int muzzleIndex = muzzles.Find(result.m_Muzzle);
		// Stats manager есть, например, у M16A2; у штатного M21 его нет.
		// В vanilla prefab это компонент сущности оружия, а не character slot.
		BaseWeaponStatsManagerComponent stats = BaseWeaponStatsManagerComponent.Cast(result.m_WeaponEntity.FindComponent(BaseWeaponStatsManagerComponent));
		float attachmentCoef = 1.0;
		if (stats && muzzleIndex >= 0 && stats.GetMuzzleVelocityCoefficient(muzzleIndex, attachmentCoef) && attachmentCoef > 0)
			result.m_fSpeedCoef *= attachmentCoef;
		if (result.m_fSpeedCoef <= 0)
		{
			result.m_sError = "#MRAD_Error_NoBallistics";
			return result;
		}

		result.m_sBulletName = ReadProjectileName(result.m_Source);
		if (result.m_sBulletName.IsEmpty() && result.m_Magazine)
		{
			MagazineUIInfo info = MagazineUIInfo.Cast(result.m_Magazine.GetUIInfo());
			if (info)
			{
				result.m_sBulletName = info.GetAmmoCaliber();
				result.m_sBulletDetail = info.GetAmmoType();
			}
		}
		if (result.m_sBulletName.IsEmpty())
			result.m_sBulletName = FilePath.StripExtension(FilePath.StripPath(result.m_sProjectile.GetPath()));
		result.m_sError = string.Empty;
		return result;
	}

	static bool ReadProjectileSpeed(IEntitySource source, out float speed)
	{
		speed = 0;
		if (!source)
			return false;
		for (int i = 0; i < source.GetComponentCount(); i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (!component || !component.GetClassName().ToType().IsInherited(ProjectileMoveComponent))
				continue;
			component.Get("InitSpeed", speed);
			return speed > 0;
		}
		return false;
	}

	protected static string ReadProjectileName(IEntitySource source)
	{
		for (int i = 0; i < source.GetComponentCount(); i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (!component || !component.GetClassName().ToType().IsInherited(InventoryItemComponent))
				continue;
			BaseContainer attributes = component.GetObject("Attributes");
			if (!attributes)
				continue;
			BaseContainer display = attributes.GetObject("ItemDisplayName");
			if (!display)
				continue;
			string name;
			display.Get("Name", name);
			if (!name.IsEmpty())
				return name;
		}
		return string.Empty;
	}
}

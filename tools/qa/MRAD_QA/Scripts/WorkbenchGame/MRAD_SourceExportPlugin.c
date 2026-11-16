// Development-only exporter. All output stays inside this process's isolated profile.
[WorkbenchPluginAttribute(name: "MRAD source research export", wbModules: {"ResourceManager"})]
class MRAD_SourceExportPlugin : WorkbenchPlugin
{
	protected int m_iExported;
	protected ref FileHandle m_Manifest;

	override void Run()
	{
		RunCommandline();
	}

	protected void ExportFile(string source)
	{
		string flat = source;
		flat.Replace("\\", "__");
		flat.Replace("/", "__");
		flat.Replace(":", "_");
		flat.Replace("$", "");
		string destination = "$profile:MRAD_SourceExport/" + flat;
		if (FileIO.CopyFile(source, destination))
		{
			m_iExported++;
			string relative = source;
			int separator = relative.IndexOf(":");
			if (separator >= 0)
				relative = relative.Substring(separator + 1, relative.Length() - separator - 1);
			m_Manifest.WriteLine(source + "\t" + destination + "\t" + Workbench.GetResourceName(relative));
		}
		else
			Print("MRAD_EXPORT_COPY_FAILED " + source, LogLevel.WARNING);
	}

	protected void ExportTree(string directory, string extension)
	{
		array<string> files = {};
		FileIO.FindFiles(files.Insert, directory, extension);
		foreach (string file : files)
		{
			ExportFile(file);
			if (FileIO.FileExists(file + ".meta"))
				ExportFile(file + ".meta");
		}
	}

	protected void InspectConfigArray(ResourceName path, string field)
	{
		Resource resource = BaseContainerTools.LoadContainer(path);
		if (!resource || !resource.IsValid())
		{
			PrintFormat("MRAD_CONFIG_LOAD_FAILED path=%1 field=%2", path, field);
			return;
		}
		BaseContainer container = resource.GetResource().ToBaseContainer();
		if (!container)
		{
			PrintFormat("MRAD_CONFIG_CONTAINER_FAILED path=%1 field=%2", path, field);
			return;
		}
		BaseContainerList entries = container.GetObjectArray(field);
		int count = -1;
		if (entries)
			count = entries.Count();
		PrintFormat("MRAD_CONFIG_ARRAY path=%1 canonical=%2 field=%3 count=%4", path, Workbench.GetResourceName(path), field, count);
	}

	protected void DumpSchema(BaseContainer container, string label, int depth = 0)
	{
		if (!container || depth > 2)
			return;
		PrintFormat("MRAD_PHYSICS_SCHEMA label=%1 class=%2 vars=%3", label, container.GetClassName(), container.GetNumVars());
		for (int i = 0; i < container.GetNumVars(); i++)
		{
			string name = container.GetVarName(i);
			DataVarType type = container.GetDataVarType(i);
			string defaultValue;
			container.GetDefaultAsString(name, defaultValue);
			PrintFormat("MRAD_PHYSICS_FIELD label=%1 name=%2 type=%3 objectBase=%4 default=%5", label, name, type, container.GetObjectBaseClass(i), defaultValue);
			if (type == DataVarType.INTEGER || type == DataVarType.FLAGS)
			{
				array<string> names = {};
				array<int> values = {};
				container.GetEnumValues(i, names, values);
				for (int e = 0; e < names.Count() && e < values.Count(); e++)
					PrintFormat("MRAD_PHYSICS_ENUM label=%1 field=%2 name=%3 value=%4", label, name, names[e], values[e]);
			}
			if (type == DataVarType.OBJECT)
				DumpSchema(container.GetObject(name), label + "." + name, depth + 1);
			else if (type == DataVarType.OBJECT_ARRAY)
			{
				BaseContainerList children = container.GetObjectArray(name);
				if (children)
				{
					for (int j = 0; j < children.Count() && j < 3; j++)
						DumpSchema(children.Get(j), label + "." + name, depth + 1);
				}
			}
		}
	}

	protected void InspectPhysics(ResourceName path)
	{
		Resource resource = Resource.Load(path);
		if (!resource || !resource.IsValid())
			return;
		IEntitySource source = resource.GetResource().ToEntitySource();
		if (!source)
			return;
		for (int i = 0; i < source.GetComponentCount(); i++)
		{
			IEntityComponentSource component = source.GetComponent(i);
			if (component && component.GetClassName() == "RigidBody")
				DumpSchema(component, path);
		}
	}

	protected void InspectSourceBallistics()
	{
		Resource first = Resource.Load("{B7DC0C3C3D1784C2}Prefabs/Weapons/Ammo/Ammo_762x51_Ball_M118.et");
		Resource second = Resource.Load("{C14C5DE6F97F06F0}Prefabs/Weapons/Ammo/Ammo_762x51_Ball_M80.et");
		if (!first || !first.IsValid() || !second || !second.IsValid())
			return;
		IEntitySource sourceFirst = first.GetResource().ToEntitySource();
		IEntitySource sourceSecond = second.GetResource().ToEntitySource();
		if (!sourceFirst || !sourceSecond)
			return;
		for (int distance = 100; distance <= 1000; distance += 100)
		{
			float timeFirst, timeSecond;
			float heightFirst = BallisticTable.GetHeightFromProjectileSource(distance, timeFirst, sourceFirst, 1.0, true);
			float heightSecond = BallisticTable.GetHeightFromProjectileSource(distance, timeSecond, sourceSecond, 1.0, true);
			PrintFormat("MRAD_SOURCE_DROP range=%1 m118Height=%2 m118Time=%3 m80Height=%4 m80Time=%5", distance, heightFirst, timeFirst, heightSecond, timeSecond);
		}
	}

	override void RunCommandline()
	{
		FileIO.MakeDirectory("$profile:MRAD_SourceExport");
		m_Manifest = FileIO.OpenFile("$profile:MRAD_SourceExport/manifest.tsv", FileMode.WRITE);
		if (!m_Manifest)
		{
			Print("MRAD_EXPORT_FAILED cannot open manifest", LogLevel.ERROR);
			Workbench.Exit(2);
			return;
		}

		array<string> wanted = {
			"scr_datadisplaygadget.c", "scr_ballistictablecomponent.c", "scr_visualisedballisticconfig.c",
			"scr_projectilewindtable.c", "scr_projectilewinddata.c", "scr_ballisticdata.c", "scr_ballisticconfig.c",
			"scr_basedatapageconfig.c", "scr_changedatapageaction.c", "scr_gadgetcomponent.c",
			"scr_weaponstatsmanagercomponent.c", "scr_weaponattachmentsuppressorattributes.c",
			"scr_settingssupermenu.c", "scr_settingsmanager.c", "scr_settingssubmenubase.c",
			"scr_keybindsetting.c", "scr_keybindrowcomponent.c", "scr_settingsmanagerkeybindmodule.c",
			"scr_settingsmenulistcomponent.c", "scr_settingssubmenugameplay.c", "scr_settingssubmenukeyboard.c",
			"scr_2dopticscomponent.c", "scr_2dpipsightscomponent.c", "scr_2dsightscomponent.c",
			"charactercameraads.c", "scr_playercontroller.c", "scr_charactercontrollercomponent.c",
			"baseweaponstatsmanagercomponent.c", "basemuzzlecomponent.c", "basemagazinecomponent.c",
			"weaponslotcomponent.c", "characterweaponmanagercomponent.c", "ballistictable.c",
			"projectilemovecomponent.c", "scr_projectilewindagedatageneratorplugin.c"
		};
		array<string> scripts = {};
		array<string> exactSources = {
			"Scripts/Game/Components/Gadgets/SCR_DataDisplayGadget.c",
			"Scripts/Game/Components/Gadgets/SCR_BallisticTableComponent.c",
			"Scripts/Game/Components/Gadgets/Configs/SCR_VisualisedBallisticConfig.c",
			"Scripts/Game/Components/Gadgets/Configs/SCR_BallisticConfig.c",
			"Scripts/Game/Components/Gadgets/Configs/SCR_BaseDataPageConfig.c",
			"Scripts/Game/Components/Gadgets/Configs/SCR_BallisticData.c",
			"Scripts/Game/Projectiles/SCR_ProjectileWindTable.c",
			"Scripts/Game/Projectiles/SCR_ProjectileWindData.c",
			"Scripts/Game/UserActions/SCR_ChangeDataPageAction.c",
			"Scripts/Game/Weapon/SCR_WeaponStatsManagerComponent.c",
			"Scripts/Game/Inventory/SCR_WeaponAttachmentSuppressorAttributes.c",
			"Scripts/Game/UI/Menu/SubMenu/SCR_SettingsSuperMenu.c",
			"Scripts/Game/UI/Menu/SettingsMenu/SCR_SettingsManager.c",
			"Scripts/Game/UI/Menu/SettingsMenu/SCR_SettingsSubMenuBase.c",
			"Scripts/Game/UI/Menu/SettingsMenu/SCR_SettingsManagerKeybindModule.c",
			"Scripts/Game/UI/Menu/SettingsMenu/SCR_KeybindSetting.c",
			"Scripts/Game/UI/Menu/SettingsMenu/SCR_KeybindSettingsSubMenu.c",
			"Scripts/Game/UI/Menu/SettingsMenu/SCR_KeybindRowComponent.c",
			"Scripts/Game/UI/Menu/MainMenuUI.c",
			"Scripts/Game/UI/Components/MainMenu/SCR_AccountWidgetComponent.c",
			"Scripts/Game/UI/Components/WidgetLibrary/SCR_TabView/SCR_TabViewComponent.c",
			"Scripts/Game/Weapon/SCR_AnimatedBeltComponent.c",
			"Scripts/Game/ScenarioFramework/Actions/SCR_ScenarioFrameworkActionEventOnProjectileShot.c",
			"Scripts/GameCode/Effects/SCR_MuzzleEffectComponent.c",
			"Scripts/Game/Effects/SCR_MuzzleEffectComponent.c",
			"Scripts/GameCode/World/TimeAndWeatherManagerEntity.c",
			"Scripts/Game/World/TimeAndWeatherManagerEntity.c",
			"Scripts/Game/generated/World/LocalWeatherSituation.c",
			"Scripts/Game/UI/HUD/SCR_2DOpticsComponent.c",
			"Scripts/Game/Weapon/Sights/SCR_2DPIPSightsComponent.c",
			"Scripts/Game/Weapon/Sights/SCR_2DSightsComponent.c",
			"Scripts/Game/Components/Gadgets/SCR_GadgetComponent.c",
			"Scripts/Game/Character/Cameras/ADS/CharacterCameraADS.c",
			"Scripts/Game/Character/SCR_CharacterControllerComponent.c",
			"Scripts/Game/Player/SCR_PlayerController.c",
			"Scripts/Game/generated/Components/BaseMuzzleComponent.c",
			"Scripts/Game/generated/Weapon/BaseMuzzleComponent.c",
			"Scripts/Game/generated/Weapon/BaseMagazineComponent.c",
			"Scripts/Game/generated/Weapon/BaseWeaponStatsManagerComponent.c",
			"Scripts/Game/generated/Weapon/BaseWeaponComponent.c",
			"Scripts/Game/generated/Weapon/WeaponComponent.c",
			"Scripts/Game/generated/Components/GameComponent.c",
			"Scripts/Game/generated/Weapon/MuzzleComponent.c",
			"Scripts/Game/generated/Weapon/BallisticTable.c",
			"Scripts/Game/generated/Projectile/ProjectileMoveComponent.c",
			"Scripts/WorkbenchGame/WorldEditor/SCR_ProjectileWindageDataGeneratorPlugin.c",
			"Configs/System/chimeraInputCommon.conf",
			"Configs/System/keyBindingMenu.conf",
			"Language/localization.en_us.conf",
			"Prefabs/Weapons/Rifles/M14/Rifle_M21.et",
			"Prefabs/Weapons/Rifles/M14/Rifle_M21_base.et",
			"Prefabs/Weapons/Core/LongRangeRifle_Base.et",
			"Prefabs/Weapons/Core/Rifle_Base.et",
			"Prefabs/Weapons/Rifles/M16/Rifle_M16A2.et",
			"Prefabs/Weapons/Rifles/M16/Rifle_M16A2_base.et",
			"Prefabs/Weapons/Attachments/Muzzle/Suppressor_M16/Suppressor_M16.et",
			"Prefabs/Weapons/Attachments/Muzzle/Suppressor_M16/Suppressor_M16_base.et",
			"Prefabs/Weapons/Core/Suppressor_base.et",
			"Prefabs/Weapons/Rifles/M16/Variants/Rifle_M16A2_suppressor_4x20_OliveGreen_Sand_Stripes.et",
			"Prefabs/Weapons/Magazines/Magazine_762x51_M14_20rnd_SpecialBall.et",
			"Prefabs/Weapons/Magazines/Magazine_762x51_M14_20rnd_Base.et",
			"Prefabs/Weapons/Ammo/Ammo_762x51_Ball_M118.et",
			"Prefabs/Weapons/Ammo/Ammo_762x51_Ball_M80.et",
			"Prefabs/Weapons/Ammo/Ammo_556x45_Ball_M855.et",
			"Configs/Weapons/AIBallisticTables/AIBT_762x51_Ball_M118.conf",
			"Prefabs/Items/Equipment/BallisticTable/BallisticTable_US_base.et",
			"Prefabs/Items/Equipment/BallisticTable/BallisticTable_base.et",
			"Prefabs/Weapons/Attachments/Optics/Optic_ARTII/Optic_ARTII.et",
			"Prefabs/Weapons/Attachments/Optics/Optic_ARTII/Optic_ARTII_base.et",
			"Prefabs/Weapons/Attachments/Optics/Optic_PSO1/Optic_PSO1.et",
			"Prefabs/Weapons/Attachments/Optics/Optic_PSO1/Optic_PSO1_base.et",
			"UI/layouts/Menus/SettingsMenu/BindingMenu/KeybindRow.layout",
			"UI/layouts/Menus/SettingsMenu/KeybindSettings.layout",
			"UI/layouts/Menus/SettingsMenu/GameplaySettings.layout",
			"UI/layouts/Menus/SettingsMenu/GameSettings.layout"
		};
		foreach (string exact : exactSources)
		{
			ExportFile("$ArmaReforger:" + exact);
			if (FileIO.FileExists("$ArmaReforger:" + exact + ".meta"))
				ExportFile("$ArmaReforger:" + exact + ".meta");
		}
		FileIO.FindFiles(scripts.Insert, "scripts/", ".c");
		foreach (string script : scripts)
		{
			string lower = script;
			lower.ToLower();
			foreach (string name : wanted)
			{
				if (lower.Contains(name))
				{
					ExportFile(script);
					break;
				}
			}
		}

		ExportTree("Configs/System/", ".conf");
		ExportTree("UI/layouts/Menus/SettingsMenu/", ".layout");
		ExportTree("Prefabs/Weapons/Attachments/Optics/", ".et");
		ExportTree("Prefabs/Weapons/Ammo/", ".et");
		ExportTree("Prefabs/Items/Equipment/", ".et");
		string worldRun = "$core:Scripts/WorkbenchCommon/WorldRunTool.c";
		if (FileIO.FileExists(worldRun))
			ExportFile(worldRun);
		InspectConfigArray("$ArmaReforger:Configs/System/chimeraInputCommon.conf", "Actions");
		InspectConfigArray("Configs/System/chimeraInputCommon.conf", "Actions");
		InspectConfigArray("$ArmaReforger:Configs/System/chimeraInputCommon.conf", "Contexts");
		InspectConfigArray("Configs/System/chimeraInputCommon.conf", "Contexts");
		InspectConfigArray("$ArmaReforger:Configs/System/keyBindingMenu.conf", "m_KeyBindingCategories");
		InspectConfigArray("{4EE7794C9A3F11EF}Configs/System/keyBindingMenu.conf", "m_KeyBindingCategories");
		InspectPhysics("{2A7C01B93CC92ACE}Prefabs/Items/Equipment/RangeCard/RangeCard_Notepad_AUTO.et");
		InspectPhysics("{7CEEC3FF35A201A2}Prefabs/Items/Core/Compass_Base.et");
		InspectPhysics("{EBBC859CCD94D96D}Prefabs/Props/EmptyStorages/USSR_AmmoBox_empty.et");
		// Exact base name observed in RigidBody.Geometries metadata; no guessed subtype.
		Resource geometrySchema = BaseContainerTools.CreateContainer("PhysicsGeometry");
		if (geometrySchema && geometrySchema.IsValid())
			DumpSchema(geometrySchema.GetResource().ToBaseContainer(), "PhysicsGeometry");
		else
			Print("MRAD_PHYSICS_SCHEMA_CREATE_FAILED PhysicsGeometry", LogLevel.WARNING);
		// Native class registration string observed in installed 1.9 Workbench.
		Resource boxSchema = BaseContainerTools.CreateContainer("PhysicsBoxGeometry");
		if (boxSchema && boxSchema.IsValid())
			DumpSchema(boxSchema.GetResource().ToBaseContainer(), "PhysicsBoxGeometry");
		else
			Print("MRAD_PHYSICS_SCHEMA_CREATE_FAILED PhysicsBoxGeometry", LogLevel.WARNING);
		InspectSourceBallistics();
		m_Manifest.Close();
		PrintFormat("MRAD_EXPORT_DONE scriptsScanned=%1 exported=%2", scripts.Count(), m_iExported);
		Workbench.Exit(0);
	}
}

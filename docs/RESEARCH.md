# MRAD 2.0 — исследование и решения R-1…R-10

База изменения: `cc9a0b3` (`docs`). Целевая среда: установленная Arma Reforger / Workbench 1.9.0.37. Техническое задание сохранено без изменения.

Первичные источники — Doxygen и script/prefab sources **установленной версии 1.9**, извлечённые штатным Workbench API в отдельный профиль. Код vanilla не скопирован в этот репозиторий. Поведение native методов, которого нет в script source, проверяется запуском; наличие сигнатуры само по себе не считается подтверждением результата.

| Пункт | Решение и источник |
|---|---|
| R-1: слот и патрон | `GetWeaponsSlots`, `WeaponSlotComponent.GetWeaponSlotIndex()==0`, `GetWeaponEntity`, текущий muzzle и `GetMagazine().GetAmmoType(0)`. Пустой слот не заменяется другим. Тип отдельно оставшегося патрона в патроннике не доступен через найденный публичный API; это граница универсальности. |
| R-2: muzzle и глушитель | Учитываются игровой `GetBulletInitSpeedCoef`, attachment coefficient при доступном stats manager и `IsMuzzleSuppressed`. Fallback default resource принимается только если это projectile, а не магазин. |
| R-3: native расчёт | Снижение рассчитывается штатным `BallisticTable.GetHeightFromProjectileSource` с коэффициентом скорости. Runtime сравнивает его с `GetAimHeightOfNextProjectile` после реального заряжания QA персонажа. Результаты modifier-теста и пределы проверки — в VERIFICATION. |
| R-4: ветер | `ProjectileWindTableConfig` читается из projectile source. Собственный безопасный доступ исключает native `midId-1` и допущение о 90 строках. При отсутствии пригодного диапазона проверяется native prediction; затем применяется явно помеченная модель только для игрового `ShellMoveComponent`, с параметрами его prefab. 360 сравнений с native predictor в World Editor дали максимальную разницу 0.002396 MRAD; неподдерживаемые movement classes не получают выдуманного результата. |
| R-5: погода | Скорость/направление берутся из TimeAndWeatherManager, направление взгляда — из CharacterAimingComponent. Native локализация 1.9, `AR-Editor_Attribute_WindDirection_Description` и `AR-Notification_EDITOR_ATTRIBUTES_WIND_CHANGED`, прямо определяет направление **откуда** дует ветер. Знак исправлен согласно этому контракту; шесть проверок проекции подтверждают правую/левую поправку, встречный/попутный ветер и границы направления взгляда. |
| R-6: произвольный MRAD | Runtime hook `GetCameraLocalTransform` у поддерживаемого прицела и согласованная поправка `GetMisalignmentAngles`. Общий ComponentClass не изменяется; диапазоны пристрелки не размножаются. Подробности в RESEARCH_OPTICS. |
| R-7: страницы и сеть | Native DataDisplay не обновляет страницу без RT, поэтому реализованы собственные серверные page/data props и запросы через owned PlayerController. Клиенты получают готовые строки; доступ к чужому инвентарю не требуется. |
| R-8: настройки и бинды | Native Controls config merge, InputBinding и SCR_KeybindRowComponent; добавочная вкладка через SCR_SettingsSuperMenu. Реальные merged counts: 307→316 actions, 100→101 contexts, 9→10 categories. Подробности в RESEARCH_SETTINGS. |
| R-9: RT | Lifecycle привязан к IN_HAND, проверяются edit mode и `System.IsConsoleApp`. Detach выполняется перед удалением widget hierarchy. Кэш и polling освобождаются при выходе из режима/удалении. |
| R-10: совместимость | Opt-in компоненты и prefab inheritance, исходные GUID сохранены. ACE Scopes и другие моды, меняющие camera/reticle hooks, требуют адаптера или отдельной проверки; они не объявлены совместимыми без запуска. |

## Необходимые расширения vanilla

Используются два расширяемых native класса:

1. `modded SCR_PlayerController`: отдельные методы MRAD для безопасного транспорта запросов страниц/настроек блокнота и поправок прицела. Реализации разнесены по файлам Notebook и Optics; штатные методы контроллера не переопределяются. Проверяется реально управляемый персонаж, его режим и конкретный предмет/прицел.
2. `modded SCR_SettingsSuperMenu`: вызывает `super.OnMenuOpen()` и добавляет одну вкладку MRAD после проверки отсутствия дубликата. Другие вкладки сохраняются.

Глобальное изменение SCR_GadgetComponent из 1.x удалено. Новая функциональность гаджета — наследник SCR_DataDisplayGadgetComponent, подключённый только к MRAD prefab.

## Подробные отчёты

- [Runtime, сигнатуры и исходники](RESEARCH_RUNTIME.md)
- [Оптика, камера и сетевой контракт](RESEARCH_OPTICS.md)
- [Settings, native ввод и очистка ресурсов](RESEARCH_SETTINGS.md)
- [Фактическая проверка и открытые критерии](VERIFICATION.md)

## Принятые значения по умолчанию

Опорный ветер 4 м/с; максимальная дальность 1500 м; 10 строк на страницу; шаги 0.1/1.0 MRAD; пределы ±50 MRAD; HUD включён. Reset существует как девятое действие без предустановленной клавиши. Настройки профиля постоянные, состояние каждого прицела сохраняется в течение жизни его сетевого экземпляра. Сохранение произвольного sight state через перезапуск сценария зависит от persistence системы сценария и не подменяется глобальным JSON игрока.

# MRAD 2.0 — исследование runtime и автоматизированной проверки

Дата: 16 ноября 2026. Проверено на установленных Game / Workbench **1.9.0.37**.
Этот документ отделяет факты исходников и API от ещё не проведённых игровых экспериментов.

## Среда и источники

- Game: `E:\Steam\steamapps\common\Arma Reforger\ArmaReforgerSteamDiag.exe`, FileVersion `1.9.0.37`.
- Workbench: `E:\Steam\steamapps\common\Arma Reforger Tools\Workbench\ArmaReforgerWorkbenchSteamDiag.exe`.
- Локальная Doxygen: `E:\Steam\steamapps\common\Arma Reforger Tools\Workbench\docs\ArmaReforgerScriptAPIPublic\html` и соседний `EnfusionScriptAPI\html`.
- Изолированная рабочая копия: `E:\CodexWork\MRAD-AR`.
- Профиль проверки: `E:\CodexWork\MRAD-QA-profile`; собственный `workbench.ini` передаётся через `-forceSettings`.
- Исходники 1.9 извлечены штатным `FileIO.CopyFile` из смонтированных PAK в `$profile:MRAD_SourceExport`, без изменения поставки игры.
- Внешние первичные инструкции: [Startup Parameters](https://community.bohemia.net/wiki/Arma_Reforger:Startup_Parameters), [Workbench Plugin Tutorial](https://community.bohemia.net/wiki/Arma_Reforger:Workbench_Plugin_Tutorial).

Действующий пользовательский Workbench не закрывался. Проверочные процессы создавались отдельно с `-noFocus` и отдельными настройками.

## Результаты обязательного исследования

| Требование | Проверенный факт | Решение / оставшийся риск |
|---|---|---|
| R-1, оружие | `GetWeaponsSlots` и `GetWeaponSlotIndex` существуют. `WeaponSlotComponent` даёт `GetWeaponEntity`; `BaseWeaponComponent` даёт `GetCurrentMuzzle`, `GetCurrentMagazine`. | Выбирать слот index 0. Пустой слот — пустое состояние; текущее оружие в руках не заменяет обязательный источник. |
| R-1, патрон | `BaseMagazineComponent.GetAmmoType(int idx=0)` возвращает `ResourceName`. | Это тип из магазина. Публичный API не даёт отдельного getter типа уже находящегося в патроннике снаряда. После смены магазина возможна разница; не выдавать имя магазина за гарантированное имя следующего выстрела. |
| R-2 | Muzzle имеет `GetBulletInitSpeedCoef`, `GetMagazine`, `GetDefaultMagazineOrProjectileName`, `IsMuzzleSuppressed`. `MuzzleComponent` дополнительно имеет `GetDefaultMagazinePrefab()->BaseContainer`. | Путь по умолчанию может обозначать магазин или снаряд; тип следует определить по структуре ресурса, а не считать каждый fallback снарядом. |
| R-2, suppressor | `SCR_WeaponAttachmentSuppressorAttributes.ApplyModifiers` вызывает `SetMuzzleVelocityCoefficient(attachedEntity,muzzleIndex,m_fMuzzleSpeedCoefficient)`; `ClearModifiers` снимает его. На штатном подавленном M16A2 менеджер вернул коэффициент 1.01 и suppressed=true. | В runtime5 штатный зарегистрированный suppressor принят как ключ для временного коэффициента 0.8; восстановление его собственных attributes вернуло 1.01. Синтетический незарегистрированный entity как ключ отвергнут. |
| R-3 | На заполненном патроннике штатного M16A2 `GetAimHeightOfNextProjectile` и `GetInitialProjectileSpeed` не изменились при коэффициентах suppressor 1.01 / 0.8 / 1.01. `GetHeightFromProjectileSource` с явным произведением коэффициентов изменился. | Гипотеза об игнорировании attachment coefficient этими двумя native helpers подтверждена для указанного оружия и установленного engine 1.9.0.37. Используем явный source query; готовый угол на коэффициент не умножается. Фактическая скорость выпущенного engine снаряда этим опытом ещё не измерена. |
| R-4 | Wind table читается из поля `ProjectileWindTableConfig` компонента `ProjectileMoveComponent` в source. Поле может отсутствовать. | Не предполагаем наличие таблицы у каждого снаряда. |
| R-4, matching | `GetDataByAngle` и `GetDataByDisance` сравнивают скорость ветра и коэффициент скорости на точное равенство. | Для других значений возвращается null. Интерполяция по этим осям автоматически не выполняется. |
| R-4, samples | `SCR_ProjectileWindData.GetDataByDisance` ожидает ровно 90 записей; `GetDataByAngle` обращается к `midId-1`. | Стандартные методы не следует слепо вызывать для произвольных коротких/граничных таблиц. Проверить крайние записи либо использовать проверяемый собственный доступ к данным. |
| R-5 | Weather getter даёт скорость в м/с и направление в градусах. Native `Language/localization.en_us.conf` прямо определяет угол как направление **откуда** дует ветер (`AR-Editor_Attribute_WindDirection_Description`, `AR-Notification_EDITOR_ATTRIBUTES_WIND_CHANGED`). | Для компенсации вправо нужна положительная проекция вектора «откуда» на правую ось взгляда. Первоначальное предположение toward отвергнуто первичным источником; физический игровой опыт остаётся дополнительной проверкой, а не основанием угадывать знак. |
| R-5, view | `CharacterControllerComponent.GetAimingComponent().GetAimingDirectionWorld()` возвращает актуальное направление прицеливания в мире. | Для блокнота в руках предпочтительнее transform убранного оружия. Пересчёт при изменении направления нужен только в режиме текущего ветра. |
| R-7 | `SCR_DataDisplayGadgetComponent` уже имеет lifecycle RT, pages, RPC и RplSave/Load; есть `SCR_ChangeDataPageAction`. | Для динамических страниц надо переопределять источник данных и отделить headless state от rendering: штатный ChangePage делает return без RT. |
| R-9 | RT создаётся в `ModeSwitch(IN_HAND)` и освобождается в `ModeClear(IN_HAND)` / `OnDelete`. | Сохранить этот жизненный цикл, пропускать editor/headless render. Для custom layout учесть ожидаемое имя `RTTexture0` или переопределить initialization. |

## Подтверждённые сигнатуры

Из локальной Doxygen / generated script sources:

```c
int CharacterWeaponManagerComponent.GetWeaponsSlots(out notnull array<WeaponSlotComponent> outSlots);
int CharacterWeaponManagerComponent.GetWeapons(out notnull array<BaseWeaponComponent> outWeapons);
IEntity WeaponSlotComponent.GetWeaponEntity();
int WeaponSlotComponent.GetWeaponSlotIndex();
string WeaponSlotComponent.GetWeaponSlotType();
int BaseWeaponComponent.GetMuzzlesList(out notnull array<BaseMuzzleComponent> outMuzzles);
BaseMuzzleComponent BaseWeaponComponent.GetCurrentMuzzle();
BaseMagazineComponent BaseWeaponComponent.GetCurrentMagazine();
BaseMagazineComponent BaseMuzzleComponent.GetMagazine();
float BaseMuzzleComponent.GetBulletInitSpeedCoef();
bool BaseMuzzleComponent.IsMuzzleSuppressed();
bool BaseMuzzleComponent.IsCurrentBarrelChambered();
ResourceName BaseMuzzleComponent.GetDefaultMagazineOrProjectileName();
BaseContainer MuzzleComponent.GetDefaultMagazinePrefab();
ResourceName BaseMagazineComponent.GetAmmoType(int idx=0);
bool BaseWeaponStatsManagerComponent.GetMuzzleVelocityCoefficient(int iMuzzleIndex, out float coeff);
bool BaseWeaponStatsManagerComponent.SetMuzzleVelocityCoefficient(IEntity attachedEntity, int iMuzzleIndex, float coeff);
```

Сигнатуры расчёта:

```c
static float BallisticTable.GetAimHeightOfNextProjectile(float distance, out float time, BaseMuzzleComponent muzzleComp, bool bDirectFire=true);
static float BallisticTable.GetHeightFromProjectileSource(float distance, out float time, IEntitySource source, float initSpeedCoef=1.0, bool bDirectFire=true);
static SCR_ProjectileWindTable SCR_ProjectileWindTable.GetProjectileWindageData(notnull IEntitySource source);
array<float> SCR_ProjectileWindTable.GetDataByAngle(float angle, float windSpeed=10, float initSpeedCoef=1);
array<float> SCR_ProjectileWindTable.GetDataByDisance(float distance, bool directFire, float windSpeed, float initSpeedCoef=1);
```

`GetDataByDisance` действительно написан с опечаткой в API. Возвращаемые индексы:

| Индекс | Значение |
|---|---|
| 0 | Угол возвышения, радианы |
| 1 | Дальность, метры |
| 2 | Максимальная высота |
| 3 | Поправка на поперечный ветер, MRAD |
| 4 | Разница дальности для продольного ветра, метры |
| 5 | Угол встречи |

При дальности вне native ballistic table метод возвращает отрицательное время. Отдельного публичного `GetMaxRange` у ballistic/muzzle API не обнаружено: максимальную допустимую строку устанавливают по результату запросов и настройке предела дальности.

### Native wind simulation и единицы

Проверены первичные исходники 1.9: `Scripts/WorkbenchGame/WorldEditor/SCR_ProjectileWindageDataGeneratorPlugin.c` и `Scripts/Game/generated/Projectile/ProjectileMoveComponent.c`.

```c
vector ProjectileMoveComponent.GetProjectileSimulationResult(
    vector initPosition, float initSpeed, float initElevationAngle,
    float initAzimuth = 0, vector windSpeed = vector.Zero,
    float targetHeight = 0, bool mustFallDown = true,
    float maxSimulationTime = -1, float maxHorizontalDistance = -1);
float ProjectileMoveComponent.GetBulletSpeedCoef();
void ProjectileMoveComponent.SetBulletCoef(float coef);
```

Оба угла задаются в **градусах**. Штатный генератор читает `InitSpeed` из source компонента, умножает его на выбранный коэффициент, затем перебирает углы 1–90°. Направление полёта — +Z; поперечный ветер — +X, встречный — −Z, попутный — +Z. Возвращается конечная позиция симуляции; взаимодействия с миром нет. Значение по умолчанию для времени — 60 секунд, для дальности — 10 км.

Документация предупреждает: метод работает только при включённом projectile debugging. Сам штатный generator не вызывает отдельного enable-метода. API `EnableProjectileDebugging` в исследованных generated sources / Doxygen не найден; придумывать его нельзя. Работу в клиенте release надо подтверждать отдельным запуском, а не наличием метода в Doxygen.

`GetBulletSpeedCoef` возвращает коэффициент **самого снаряда**, например для зарядных колец миномёта. Это не полный коэффициент: native комментарий требует умножать его на muzzle coefficient. Это отдельная ось от attachment modifier; универсальность расчёта для таких снарядов не следует считать доказанной тестом обычной винтовки.

## Render-target и штатные страницы

Источник: `Scripts/Game/Components/Gadgets/SCR_DataDisplayGadget.c`.

`SCR_DataDisplayGadgetComponentClass` хранит layout и массив конфигов страниц. Instance хранит `m_wRenderTargetTexture`, `m_wRootWidget`, `m_iNumberOfPages`, `m_iCurrentPageID`. Состояние нельзя помещать в ComponentClass, общий для экземпляров.

Штатный initialization ищет `RTTexture0`, назначает `SetRenderTarget(owner)` и выключает постоянный redraw. Обновление временно включает RT, рисует страницу, выключает RT. Очистка удаляет hierarchy, проверяет `owner.IsDeleted()`, затем вызывает `RemoveRenderTarget(owner)`.

Штатный `ChangePage` не изменяет состояние при отсутствии RT. `UpdateData` выходит без root widget. Поэтому эта готовая реализация сама по себе не является доказательством корректной серверной авторитетности динамических страниц: на dedicated server виджетов нет. Транспорт страниц и данных необходимо проверять отдельно от клиентского отображения.

`SCR_ChangeDataPageAction` показывает действие только при `GetNumberOfPages()>1` и `GetMode()==IN_HAND`, а `CanBroadcastScript()` возвращает false. Рассылка состояния выполняется компонентом, а не дублем user action.

## Что подтверждено запуском

1. В изолированном Workbench с исходным MRAD успешно скомпилированы Game и WorkbenchGame. Это baseline совместимости, **не** проверка новой реализации 2.0.
2. `-wbSilent` в данной установленной версии завершает startup, но не запускает команду custom plugin. Нельзя считать успешный startup доказательством выполнения тестов.
3. `-noFocus -exitAfterInit` и `Start-Process -WindowStyle Hidden` позволяют выполнить plugin, который сам завершает свой Workbench через `Workbench.Exit`.
4. API `System.FindFiles` из старого online tutorial отсутствует в 1.9. Подтверждён `FileIO.FindFiles`, но он не перечислил файлы смонтированных PAK в этом эксперименте.
5. `FileIO.CopyFile` по точному пути `$ArmaReforger:Scripts/...` работает для packaged sources. `FileIO.CopyFile` может создать пустой файл при неудаче: нужно учитывать возвращаемый bool.
6. `FileHandle`, удерживаемый в поле helper-класса, должен иметь `ref`, иначе объект может исчезнуть до завершающего `Close`.

Лог успешного экспорта: `E:\CodexWork\MRAD-QA-profile\logs-export6\script.log`. Содержит `MRAD_EXPORT_DONE scriptsScanned=0 exported=26`; ошибок компиляции нет. Предупреждения baseline относятся к vanilla deprecations. Неподтверждённые пути ресурсов отмечены отдельно как COPY_FAILED.

### Последующие проверки реализации

- `tests-20261116T141728288027Z`: родительский интеграционный runner подтвердил compilation текущей реализации и `MRAD_VALIDATE_RESULT checks=24 failures=0`. Это были начальные проверки settings/pages/formatting, до добавления wind boundary и camera-matrix checks.
- `export-20261116T141820172410Z`: массивы после MRAD override содержат 316 actions / 101 contexts / 10 keybinding categories против vanilla 307 / 100 / 9. Объединение добавляет ожидаемые 9 / 1 / 1 записи и сохраняет vanilla.
- `runtime-20261116T171512/logs/script.log`: первый запуск QA world на установленном ServerDiag завершился, но **R-3 не прошёл**: созданный отдельно M21 был без патрона в патроннике; native next-projectile расчёт дал height=0/time=0. Source расчёт M118 дал положительные высоты/времена на 100/300/700/1000 м. Нельзя трактовать это как доказательство того, что native метод игнорирует коэффициенты.
- В том же runtime текущий снаряд — `{B7DC0C3C3D1784C2}Prefabs/Weapons/Ammo/Ammo_762x51_Ball_M118.et`; `InitSpeed=790`, `GetBulletSpeedCoef=1`, `ProjectileWindTableConfig` пустой. Два вызова native prediction вернули `0 0 0`. Следовательно, существование штатной wind-таблицы у обычной M118 и работоспособность prediction в этом server runtime **опровергнуты для данного эксперимента**.
- Имя файла `ServerDiag` не определяет script symbol `ENABLE_DIAG`: в первом runtime он не установлен. Логировать это как «проверка retail exe» было бы неверно; выполнен именно указанный ServerDiag 1.9.0.37.

Второй запуск, `runtime-20261116T142116295785Z/logs/script.log`, поместил M21 в primary slot собственного `Character_Base` и выбрал оружие. Через 3 секунды патронник был заполнен. Native next-projectile и source расчёты **совпали** на четырёх дальностях 100/300/700/1000 м. Это подтверждает baseline при коэффициенте 1; проверка attachment modifier всё ещё не выполнена, поскольку entity-level stats manager у этого экземпляра отсутствовал (`muzzles=1, index=0, stats=NULL`). В третьем запуске `runtime-20261116T142424903953Z` проверен также nested поиск из actual WeaponComponent: он тоже вернул null. Поэтому пустой stats нельзя маскировать выдуманным getter.

Второй запуск также подтвердил 49 проверок созданных optics/rifle examples без ошибок, native типы компонентов, replication item ID, накопление/ограничения/reset и независимость двух состояний. Калькулятор создал 10 строк 100–1000 м, его drop совпал с native source при тех же входах; calm wind дал ноль, +4/−4 — симметричные значения. Все эти wind строки имели `estimated=1`, что не заменяет проверку точности.

Причина отсутствия stats установлена по export11: `Rifle_M16A2_base.et` содержит `SCR_WeaponStatsManagerComponent "{6188FC1045F23E2A}"` на уровне entity; M21 его не объявляет. Generated `BaseWeaponComponent.c` и `WeaponComponent.c` не имеют отдельного getter stats manager. Таким образом, обычный `weaponEntity.FindComponent(BaseWeaponStatsManagerComponent)` является правильным lookup для штатного M16A2. Для проверки R-3 следует использовать M16A2 с существующим менеджером, а отсутствие менеджера у M21 не считать ошибкой метода поиска.

### R-3: проверка штатного suppressor

В `runtime-20261116T143200732756Z` штатный M16A2 дал существующий stats manager, muzzle index 0 и заполненный патронник. Попытка использовать новый GenericEntity как modifier key вернула false. Этот опыт отвергает прежнюю синтетическую fixture: принимается зарегистрированный attachment, поэтому отсутствие эффекта нельзя объяснять физикой, пока регистрация не подтверждена.

В `runtime-20261116T143917818453Z/logs/script.log` использован штатный prefab `{29DD901C0664FC61}Prefabs/Weapons/Rifles/M16/Variants/Rifle_M16A2_suppressor_4x20_OliveGreen_Sand_Stripes.et`. Его существующий suppressor найден по `SCR_WeaponAttachmentSuppressorAttributes`; `suppressed=1`, `chambered=1`, muzzle coefficient=1, attachment coefficient=1.01. `SetMuzzleVelocityCoefficient` с этим attachment и значением 0.8 вернул true; затем `ApplyModifiers` того же attachment вернул true и восстановил 1.01.

При всех трёх состояниях `GetInitialProjectileSpeed()` возвращал 930. Native next-projectile height/time также остались прежними. Source query с коэффициентом 1 совпал с native; явное произведение muzzle и attachment изменило результат:

| Дальность, м | Native next-projectile, м, для всех трёх состояний | Source с коэффициентом 1.01, м | Source с коэффициентом 0.8, м |
|---|---:|---:|---:|
| 100 | 0.0871789 | 0.0858847 | 0.124533 |
| 300 | 0.784611 | 0.773483 | 1.18778 |
| 700 | 5.95381 | 5.87212 | 9.2269 |
| 1000 | 17.067 | 16.8313 | 26.455 |

Итог: `MRAD_RUNTIME_DONE validRows=12 failures=0`; в том же запуске 49 optics, 19 notebook и 16 settings checks завершились без ошибок. Settings UI явно пропущен на console/headless. Проверка 100 close/open циклов здесь подтверждает lifecycle состояния без RT, а не отсутствие утечек настоящего render target.

Эти числа подтверждают поведение доступных расчётных helpers и чтение настоящего зарегистрированного коэффициента. Они **не являются измерением реальной скорости или точки попадания выпущенного engine снаряда**. Для такого дополнительного измерения публичный API предлагает `CharacterControllerComponent.SetFireWeaponWanted(bool)` и `ProjectileMoveComponent.GetVelocity()`; нужен наблюдатель штатного события выстрела, без ручного `Launch` или подмены снаряда.

## Реализация калькулятора

`MRAD_RangeCardCalculator.Build(input,maxRange,signedCrosswind)` возвращает данные без виджетов. Диапазон — шаг 100 м с ограничением настроек; строки с неположительными временем/высотой отбрасываются. Угол снижения вычисляется непосредственно из native source query. Значение `signedCrosswind>0` означает **компенсацию вправо**; опорный режим даёт `+W`. Для текущего ветра используется горизонтальная правая ось текущего взгляда персонажа и положительная проекция вектора направления к источнику ветра. Конвенция native направления подтверждена ниже.

`MRAD_RangeCardWind` читает исходные массивы wind config через `BaseContainer`, проверяет размеры и ограничивает интерполяцию диапазоном существующих образцов. Поддерживаются короткие и несортированные массивы; нет обращения к `index-1` или требования ровно 90 записей. Если коэффициента скорости нет, интерполяция допускается только между двумя имеющимися коэффициентами. За пределами таблицы выполняется попытка native prediction. Нулевой/не достигший заданной дальности результат считается недоступным.

Последний fallback — `MRAD_GameShellWindModel`, ограниченный известным игровым `ShellMoveComponent`. Он читает `Mass` и `AirDrag` из текущего source, использует `Physics.VGravity` и ту же начальную скорость с коэффициентами. Используются только игровые параметры. Неизвестный movement class, отсутствующие поля или превышение расчётного бюджета приводят к явному `NoWind`, а не вымышленному числу. Результат модели помечается `m_bEstimatedWind`.

Схема дискретного обновления найдена сравнением с доступным native predictor: скорость обновляется явно, положение — по среднему старой и новой скорости, с шагом 1/30 секунды. Это **эмпирически установленное совпадение**, не прочитанная реализация C++. В коде нет отдельных численных поправок или ветвлений для M118/M855. Native prediction создаёт один незапущенный projectile instance на пересчёт и освобождает его после расчёта; `Launch`/`EnableSimulation` для вычисления таблицы не вызываются.

### Проверка ветра и отвергнутые варианты

Публичный native predictor оказался доступен в WorldEditor, хотя на ServerDiag возвращал нулевые векторы. Проверка выполнена штатным путём `WorldEditor.GetApi().GetWorld()` и вызовом `GetProjectileSimulationResult` на снаряде из его реального prefab.

| Вариант | Подтверждённый результат |
|---|---|
| Оценка по времени из ballistic table | 22 ошибки из 40, максимальная 0.593835 MRAD; удалена из product. |
| RK4 с малым шагом | На первых 40 образцах max 0.0695803 MRAD; расширенная матрица выявила 168 ошибок из 360, max 2.47752 MRAD. Меньший шаг сам по себе не воспроизводит дискретный native predictor. |
| Согласованная с native схема, шаг 1/30 | `wind-20261116T144429420704Z/logs/script.log`: 360 valid / 360 samples, failures=0, unavailable=0, max error=0.00239563 MRAD. |

Последняя матрица содержит два штатных снаряда — M118 `{B7DC0C3C3D1784C2}` и M855 `{AC26AB660097633D}`; коэффициенты начальной скорости 1 / 0.8 / 1.01; ветер −30 / −4 / +4 / +30 м/с; дальности 100–1500 м с шагом 100. На этой матрице требуемые ±0.2 MRAD соблюдены. Это сравнение с игровым predictor, не с измеренными попаданиями; оно не доказывает результат для произвольных модовых projectile classes. В том же Workbench log проверено AC-15: editor=1, AUTO hasRT=0, held=0.

Для R-5 найден дополнительный первичный источник: установленный `Language/localization.en_us.conf`. Родные строки с продолжением разобраны до массивов одинакового размера (16455 Ids и 16455 Texts). Запись index 6876 — `AR-Editor_Attribute_WindDirection_Description` — содержит «Change the direction from which the wind is blowing»; index 11629 — `AR-Notification_EDITOR_ATTRIBUTES_WIND_CHANGED` — содержит «wind is blowing from». Это подтверждает конвенцию **from**, которой не хватало в Doxygen. Вектор `(sin(angle),0,cos(angle))*speed` представляет направление к источнику ветра; для компенсации вправо берётся положительная проекция на правую ось взгляда. Отрицательная проекция, основанная на первоначальном предположении toward, имеет неверный знак.

Дополнительный `LocalWeatherSituation.GetLocalWindSway()->vector` не имеет пояснения, является ли он полным ветром, поэтому заменять им документированные speed/direction без опыта нельзя. Симметрия опорного ±W сама по себе также не проверяет направление физического текущего ветра.

## CLI для воспроизводимой проверки

Проверенная основа launcher:

```text
ArmaReforgerWorkbenchSteamDiag.exe
  -noFocus -exitAfterInit -disableCrashReporter -noBackend -noThrow -VMErrorMode fatal
  -profile "E:\CodexWork\MRAD-QA-profile"
  -forceSettings "E:\CodexWork\MRAD-QA-profile\workbench.ini"
  -addonsDir "E:\Steam\steamapps\common\Arma Reforger\addons,E:\CodexWork\MRAD-AR,E:\CodexWork\MRAD-AR\tools\qa"
  -gproj "E:\CodexWork\MRAD-AR\tools\qa\MRAD_QA\addon.gproj"
  -wbModule=ResourceManager -plugin=MRAD_SourceExportPlugin
```

Каждый запуск должен иметь новый `-logsDir`. Launcher использует рабочую директорию с exe и `Start-Process -WindowStyle Hidden -PassThru`; PID сохраняется для контроля только своего процесса.

Для проверки новой реализации штатно документировано `-wbModule=ScriptEditor -validate PC`, затем HEADLESS; ожидаемый exit code 0 / -1. Выполнение этой команды нужно дополнительно проверять по сообщениям compile в логе, а не только по exit code.

Чистые проверки реализованы в `MRAD_ValidatePlugin`: вызывается вместо exporter через `-plugin=MRAD_ValidatePlugin`, итоговый marker `MRAD_VALIDATE_RESULT`. Проверки не меняют singleton настроек и не сохраняют профиль. Они не заменяют RT, multiplayer, input или FPS acceptance.

Для runtime используется отдельный QA world и `MRAD_RuntimeProbe` с ограниченным набором диагностических действий. Проверенный server entry point: `ArmaReforgerServerDiag.exe -server <world.ent> -addonsDir <directories> -addons <QA GUID> -profile <isolated directory> -noBackend -maxFPS 30`. В установленном debug route `-server` параметры `-bindIP` / `-port` не дали заявленного ограничения: логи показали фактический порт 2001. Launcher проверяет его доступность перед стартом; в описании не обещается loopback bind, которого этот опыт не подтвердил. `Game.RequestClose()` завершает собственный процесс после проверки; результаты конкретных запусков приведены выше.

QA addon GUID: `CBA77A6A1C5E4B20`; world: `{CBA77A6A1C5E4B21}Worlds/MRAD_QA/MRAD_QA.ent`. Мир наследует тот же vanilla `worlds/MP/MpTest/MpTest.ent`, что уже использовал проект. Текущий слой задаёт штатный M16A2 с suppressor. Runner сравнивает native next-projectile и source queries при коэффициентах 1.01 / 0.8 / 1.01, используя ключ настоящего зарегистрированного attachment и восстанавливая его attributes. Затем проверяет calculator, notebook и isolated settings JSON. Это проверка существующего native attachment и расчётных API, а не автоматическое присоединение/снятие attachment игроком. Runner удаляет только созданные им сущности и закрывает свой процесс. Логовые markers: `MRAD_RUNTIME_COEFFICIENT`, `MRAD_RUNTIME_DROP`, `MRAD_RUNTIME_SIMULATION`, `MRAD_RUNTIME_DONE`.

Дополнительный флаг `-mradNativeFire` включает отдельный `MRAD_NativeFireProbe`: подписка на native `OnProjectileShot` персонажа по эталону `SCR_AnimatedBeltComponent`, три наблюдаемых выстрела при исходном attachment coefficient и три при 0.8, чтение `ProjectileMoveComponent.GetVelocity()`. Helper не вызывает `Launch` и не подменяет создание снаряда. Он ограничен 12 секундами, снимает trigger и подписку, восстанавливает attributes, удаляет только наблюдавшиеся снаряды своей fixture. Marker `MRAD_NATIVE_FIRE_RESULT` должен иметь по три образца на состояние и failures=0; само наличие helper не означает, что этот дополнительный опыт уже прошёл. Если синхронное событие произошло до заполнения velocity, следующий callqueue turn помечается `delayed=1`, и его нельзя выдавать за точное мгновение выхода из muzzle.

Launcher `tools/qa/run_runtime.py --native-fire` добавляет этот флаг и требует отдельный успешный marker с `originalShots=3 modifiedShots=3`; обычного exit0 или общего runtime marker недостаточно. Первый опыт `runtime-20261116T145804407456Z` завершился без событий выстрела за 12 секунд: originalShots=0, modifiedShots=0, restored=1, failures=2. Это не доказательство игнорирования коэффициента самой физикой.

В followup `runtime-20261116T150250497543Z` настоящий ammo counter уменьшился с 30 до 29 при `raised=1`, `canFire=1`, `permitted=1`; native выстрел произошёл, хотя listener не получил projectile. В следующем `runtime-20261116T151011252765Z` независимые импульсы trigger обеспечили уменьшение ammo с 30 до 8. Найдены оба настоящих `SCR_MuzzleEffectComponent` — внутри текущего muzzle и на suppressor — и их invokers подписаны, но ни character event, ни muzzle invoker не предоставили наблюдаемый projectile. Поэтому это ограничение наблюдателя на данной headless fixture, а не результат «оружие не стреляет». Штатный script `GameCode/Effects/SCR_MuzzleEffectComponent.c` не содержит headless guard в `OnFired`, но его native caller закрыт: утверждать rendering-only поведение по одному отсутствию callback нельзя. Измерение фактической velocity остаётся незавершённым до отдельного успешного опыта с доступным projectile.

`MRAD_NotebookInventoryChecks` дополняет NotebookProbe отдельной native fixture: два настоящих rifle prefab меняются в primary slot 100 раз; каждая замена должна изменить фактические данные и увеличить build counter ровно на один. Следующий page request должен использовать кэш. Дополнительно выполняется `InventoryStorageManagerComponent.TryReplaceItem` для настоящего M14 M118 magazine на M80 в storage/slot штатного muzzle. Проверяется фактическая identity магазина, resource снаряда, изменение drop и один rebuild. Это не имитация пользовательских кликов или анимации reload и не измерение FPS; синхронная неподтверждённая inventory операция не засчитывается как успех.

В `gui-20261116T150837660916Z` реальные 100 замен оружия, данные нового projectile, один rebuild на каждую замену и кэш между запросами прошли. Native magazine replacement вернул `permitted=1 accepted=1 observed=1`, обновил resource и build counter. Единственное неуспешное сравнение старого drop использовало слабую локальную ссылку на snapshot, владельца которой cache refresh заменяет. Независимый `export-20261116T151441145988Z` подтвердил, что native M118/M80 действительно различны: высота на 100 м — 0.104603 / 0.0915036 м; на 1000 м — 16.2364 / 13.953 м. QA исправлен на strong `ref`, сохранённый scalar и сравнение всех десяти старых/новых строк с их собственными native source queries. Это изменение QA lifetime, не изменение расчёта продукта.

Исправленный `runtime-20261116T152825899613Z/logs/script.log` завершился с `MRAD_INVENTORY_RESULT checks=16 failures=0 weaponChanges=100 magazineChanges=1`. Все десять старых и новых строк совпали с отдельными native queries. После удаления собственного replacement magazine движок действительно вернул `muzzle.GetMagazine()==null`; следующий публичный page request дал `#MRAD_Error_NoMagazine` и пустые rows. Никакие входные данные для этой ошибки не подменялись. Общий runtime marker: `validRows=12 failures=0`.

Флаг `-mradUIVisual` после основного runtime открывает настоящий Settings/MRAD и даёт 10 секунд для захвата собственного окна. В headless режиме helper явно пропускает visual проверку. Наличие этой ветки не заменяет `MRAD_SETTINGS_VISUAL_READY` и просмотр реально отрисованного окна.

Exporter дополнительно печатает `MRAD_CONFIG_ARRAY` для массивов input `Actions`, `Contexts` и `m_KeyBindingCategories`. Сравниваются явный vanilla alias и общий путь/GUID. Это позволяет проверить ресурс после объединения overrides; одинаковый count сам по себе ещё не доказывает, что alias загрузил отдельную исходную копию.

## Ограничения минимальной проверки

- PC/HEADLESS compilation проверяет типы и API, но не поведение камер/попаданий.
- Модельные page/number checks проверяют формат и границы; они не закрывают AC-16 по утечкам RT.
- Headless native diagnostics могут сравнить коэффициенты и игровые таблицы, но не показывают UI.
- AC-7, AC-11 требуют наблюдений между сервером и клиентом; один dedicated process не доказывает репликацию.
- AC-12/13 требуют реального переназначения/перезапуска или проверки сохранённого binding state.
- `ProjectileMoveComponent.GetProjectileSimulationResult` явно требует включённого projectile debugging. Наличие метода в публичном API не доказывает его работоспособность в обычном клиенте. Этот риск должен разрешиться отдельным экспериментом.

## Исходная notebook physics geometry

Первый GUI log содержал ошибку создания `RigidBody` для `Assets/Props/Civilian/RangeCard/Notepad_LP.xob` без geometry. Исходный prefab включал `ModelGeometry=1`, а данный XOB не предоставлял collider. Ошибка относится к существовавшему model/physics сочетанию; нельзя считать её исправленной только из-за успешной компиляции скриптов.

В `export-20261116T151441145988Z` native metadata `RigidBody.Geometries` указывает `objectBase=PhysicsGeometry`. Следующий `export-20261116T152016400348Z` успешно создал контейнер этого точного класса через `BaseContainerTools.CreateContainer` и перечислил шесть полей: `Layers`, `LayerPreset`, `SurfaceProperties`, `Mass`, `Offset`, `Orientation`. Базовый класс не содержит размеров или формы. Read-only регистрационные строки установленного Workbench 1.9.0.37 содержат конкретный `PhysicsBoxGeometry`; `export-20261116T152815950039Z` подтвердил его контейнер с дополнительным седьмым полем `Extents`, type=3, default=`1 1 1`. Это наблюдённое имя native класса и проверенная схема, а не предположение по C++-подобному названию.

Размеры notebook берутся через `IEntity.GetBounds(out mins,out maxs)`. Дополнительно QA читает реальные `Physics.GetNumGeoms`, `GetGeomWorldBounds`, `GetGeomInteractionLayer`, `GetGeomName`, общий mask и mass. Разница model bounds и collider bounds позволяет проверить положение и смысл `Extents` непосредственно в движке; заранее приравнивать его полной или половинной величине нельзя. Marker `MRAD_NOTEPAD_MODEL_BOUNDS` относится к визуальному model, `MRAD_NOTEPAD_GEOMETRY` — к созданным collider. Эти read-only diagnostics сами по себе не добавляют geometry и не доказывают завершение исправления prefab.

Перед исправлением `runtime-20261116T152825899613Z` подтвердил `physics present=0`. Model size — `(0.112390, 0.0138095, 0.172461)` м, center — `(-0.00303810, 0.00696443, -0.000644848)` м; min=`(-0.0592332, 0.0000597, -0.0868753)`, max=`(0.0531570, 0.0138692, 0.0855856)`. Prefab исправлен явным `PhysicsBoxGeometry` с этими размерами и offset, `ModelGeometry=0`; исходный model GUID сохранён.

После исправления `runtime-20261116T152902964211Z` завершился с exit0 и успешным общим runtime marker. Фактическое тело: `present=1 geoms=1 layers=20480 mass=1`. `GetGeomWorldBounds` вернул размер `(0.112390, 0.0138092, 0.172461)` м, совпадающий с visual model с точностью вывода float. Тем самым непосредственно подтверждено: `PhysicsBoxGeometry.Extents` задаёт **полные размеры**. В этом логе нет прежней ошибки `without geometry` и нет ошибки неизвестного prefab keyword. Исправление проверено созданным native collider, а не только отсутствием ошибки компиляции.

## R-10: изменения установленной ветки 1.9

Прочитаны первичные публикации Bohemia: [Changes and Improvements in 1.9](https://reforger.armaplatform.com/news/modding-update-sept-24-2026) и [1.9.0.37 Changelog](https://reforger.armaplatform.com/news/changelog-november-4-2026). Для MRAD существенны новые имена `Timestamp` / `DateTimeView` / `Duration`, перенос `ElideText` к `TextWidget`, переименование `SCR_ELoiterItemID` и inventory validation API. Поиск в продуктовых и QA `.c` не нашёл одноимённых классов либо обращений к этим старым именам. Текущий `SCR_ChangeDataPageAction` подтверждён установленным исходником и компиляцией, поэтому старое имя action не переносится из 1.x.

В 1.9 также добавлено штатное перепаковывание магазинов. Снимок MRAD отслеживает фактический projectile resource и identity магазина, но не предоставляет отсутствующий публичный getter отдельно оставшегося chambered round. Проверка смешанных составов и нестандартных модовых charge mechanisms остаётся границей покрытия.

Выполненные native опыты относятся к установленным SteamDiag / ServerDiag и Workbench 1.9.0.37. Даже при `ENABLE_DIAG=0` это не утверждение о запуске retail exe или о совместимости с будущей версией игры.

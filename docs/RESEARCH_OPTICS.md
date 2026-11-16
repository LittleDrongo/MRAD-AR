# MRAD 2.0 — исследование оптики (R-6)

## Подтверждено до реализации

Проверена локальная документация установленного Workbench **1.9.0.37**:
`E:\Steam\steamapps\common\Arma Reforger Tools\Workbench\docs\`.

| Вопрос | Источник | Вывод и решение |
|---|---|---|
| Классы оптики | `ArmaReforgerScriptAPIPublic/html/interfaceSCR__2DOpticsComponent.html`, `interfaceSCR__2DSightsComponent.html`, `interfaceSCR__2DPIPSightsComponent.html` | Иерархия: `SCR_2DOpticsComponent` → `SCR_2DSightsComponent` → `SCR_2DPIPSightsComponent`. Для обычного прицела наследуем `SCR_2DSightsComponent`, сохраняя штатные zoom/illumination; для PIP — `SCR_2DPIPSightsComponent`. В ТЗ направление первой стрелки иерархии указано неверно. |
| Runtime-поворот камеры | `interfaceSCR__2DOpticsComponent.html`, метод `IEntity GetCameraLocalTransform(out vector matrix[4])` | Это публичный script-метод. Наследник может дополнить результат `super`, не меняя общие данные `ComponentClass`. Положение камеры и штатный zeroing сохраняются. |
| Потребители трансформации | Экспорт штатным Workbench из установленной 1.9.0.37: `SCR_2DOpticsComponent.c:897`, `CharacterCameraADS.c:670`; `SCR_2DPIPSightsComponent.c:523,920` | И 2D ADS solver, и создание/обновление PIP вызывают `GetCameraLocalTransform`. Проверена именно установленная реализация; runtime-результат ещё требует теста. |
| Ориентация сетки | Тот же экспорт: `SCR_2DOpticsComponent.c::GetMisalignmentAngles` | Метод отдельно использует `data.GetCameraAngles`. К результату `super` применяется та же добавочная ориентация, чтобы встроенная компенсация misalignment не возвращала сетку к прежней линии прицеливания. |
| Отказ от Workbench-only API | `interfaceBaseSightsComponent.html`, `ForceSightsZeroValue` | Документация прямо ограничивает метод Workbench. В игровом коде MRAD он не используется. |
| Состояние на экземпляр | `EnfusionScriptAPI/html/interfaceRplProp.html`, `interfaceScriptComponent.html` | Дополнительный `MRAD_ZeroingStateComponent` содержит целые десятые MRAD с `RplProp`. Не хранить изменяемые углы в `*ComponentClass`: эти данные разделяются экземплярами. |
| RPC и владение | `EnfusionScriptAPI/html/group__Replication.html`, `interfaceRplRpc.html` | Клиентский RPC к серверу от не принадлежащего клиенту объекта отбрасывается. Запрос идёт через принадлежащий игроку `SCR_PlayerController`; сервер проверяет управляемого персонажа, ADS и его текущий прицел. |
| Проверка ADS на сервере | `interfaceCharacterControllerComponent.html`, `interfaceBaseWeaponManagerComponent.html` | Есть `IsWeaponADS()`, `IsDead()`, `IsUnconscious()`, `GetCurrentSights()`. Не использовать локальные camera-callback flags как единственное доказательство серверного ADS. |
| Ввод | `EnfusionScriptAPI/html/interfaceInputManager.html`, `interfaceMenuManager.html` | Подтверждены `ActivateContext`, `AddActionListener`, `RemoveActionListener`, `IsAnyMenuOpen`. Подписки и контекст работают только у локального игрока в ADS и при закрытом меню. |

Внешние источники исходного кода, просмотренные как справочные:

- <https://arexplorer.zeroy.com/_s_c_r__2_d_optics_component_8c_source.html> (1.7.0.54).
- <https://arexplorer.zeroy.com/_s_c_r__2_d_sights_component_8c_source.html> (1.7.0.54).
- <https://arexplorer.zeroy.com/_s_c_r__2_d_p_i_p_sights_component_8c_source.html> (1.7.0.54).
- <https://arexplorer.zeroy.com/_character_camera_a_d_s_8c_source.html> (1.7.0.54).
- <https://github.com/acemod/ACE-Anvil/tree/master/addons/scopes>: проверен принцип изменения camera angles; код ACE не перенесён, shared `ComponentClass` состояние не используется.

## Сетевой контракт

- Единица хранения — **0.1 MRAD**, целое число; десять обычных нажатий дают ровно десять ticks.
- Клиент отправляет дельту одной оси либо сброс, ID компонента и собственные допустимые пределы. Нулевая дельта без сброса согласует изменённые пределы. Произвольное итоговое состояние от клиента не принимается.
- Сервер отклоняет запросы к чужому/неактивному прицелу, изменение сразу двух осей, шаг вне 1–100 ticks и некорректные пределы.
- Эффективный предел — минимум пользовательского ограничения, настройки сервера и атрибута прицела; абсолютный максимум 50 MRAD. Это позволяет player-local настройкам уменьшить предел, но не расширить разрешение сервера.
- Шаг выбран в локальных настройках, передаётся целыми ticks, валидируется сервером. Поэтому индивидуальные fine/coarse шаги работают и с dedicated server.
- На входе в ADS и при изменении revision настроек отправляется запрос согласования пределов. Сервер подтверждает принятый запрос отдельным reliable owner ACK. Если item ID ещё не готов или сервер ещё не получил состояние ADS, клиент повторяет только идемпотентную нулевую дельту до восьми раз с интервалом 0.5 с. Шаги ввода не повторяются. Если новый предел ниже сохранённой поправки, сервер ограничивает её после получения запроса, без необходимости нажимать стрелку.
- Состояние меняет только сервер. Клиентская камера отображает подтверждённые реплицированные значения; предсказание отсутствует.
- Значения хранятся на предмете прицела в текущей игровой сессии. Передача другому игроку сохраняет их. Сброс снимает только добавочные MRAD-поправки, не изменяя штатную дальность zeroing.

## Компоненты и интеграция

На одной entity прицела должны находиться один из `MRAD_2DSightsComponent` / `MRAD_PIPSightsComponent` и `MRAD_ZeroingStateComponent`. Сохраняется унаследованная replication hierarchy предмета. Если state отсутствует, прицел сохраняет штатное поведение и пишет одно диагностическое предупреждение при инициализации.

Наследники сохраняют `super` для активации, отключения, post-frame, удаления, вычисления трансформации и misalignment. Поэтому штатные zoom, illumination, animation, PIP, native range zeroing и far observer не заменяются собственной копией этих механизмов.

HUD показывает **дополнительную вертикальную / горизонтальную поправку** в MRAD. Пределы показываются по подтверждённому сервером состоянию. При выключенном постоянном HUD сообщение о достижении предела всё равно доступно. Сброс — самостоятельное переназначаемое действие `MRAD_ResetZeroing`; конфликтующая с перезарядкой клавиша по умолчанию ему не назначена.

Горячие клавиши/контекст называются согласно контракту настроек: `MRAD_ElevationUpFine`, `MRAD_ElevationDownFine`, `MRAD_WindageLeftFine`, `MRAD_WindageRightFine` и варианты `Coarse`; `MRAD_OpticsContext`. Одновременно активен только один локальный input controller. При открытии меню подписки снимаются; при выходе из ADS или удалении компонента снимаются подписки и удаляется HUD.

ACE Scopes также расширяет родительский класс оптики и меняет camera angles. Одновременные независимые поправки ACE и MRAD могут складываться. MRAD не обращается к внутренним полям ACE и не отключает чужие функции; до отдельного AC-14 такую комбинацию нельзя объявлять проверенной. Для изолированного теста MRAD отключить ACE Scopes.

## Примеры

| ResourceName | Назначение |
|---|---|
| `{A7DE1002B62C4801}Prefabs/Weapons/MRAD/Optics/MRAD_Optic_ARTII_2D.et` | ART II с `MRAD_2DSightsComponent` и отдельным state-компонентом. |
| `{A7DE1002B62C4802}Prefabs/Weapons/MRAD/Optics/MRAD_Optic_ARTII_PIP.et` | ART II с `MRAD_PIPSightsComponent`; сохраняет штатный переключаемый PIP/2D путь. |
| `{A7DE1002B62C4803}Prefabs/Weapons/MRAD/Rifles/MRAD_Rifle_M21_2D.et` | Штатный M21 с установленным первым прицелом. |
| `{A7DE1002B62C4804}Prefabs/Weapons/MRAD/Rifles/MRAD_Rifle_M21_PIP.et` | Штатный M21 со вторым прицелом. |

Исходные GUID, component ID и поля attachment взяты из файлов установленной 1.9.0.37, экспортированных штатным `FileIO.CopyFile` из package FS: `Optic_ARTII_base.et` (`F7592BD37116D7C3`), `Rifle_M21.et` (`B31929F65F0D0279`), `Rifle_M21_base.et` (`3FB5275A6505B323`). Новых моделей или сторонних аддонов примеры не требуют. Нативные диапазоны zeroing, геометрия и игровые настройки модели наследуются; массив из сотен положений MRAD не создаётся.

Загрузка и фактический spawn подтвердили замену исходного PIP-компонента на класс 2D с тем же ID `{5D0CC83435B55855}`: в 2D-примере нет оставшегося vanilla PIP-компонента. `MRAD_OpticsProbe` в server runtime установленной 1.9.0.37 выполнил **49 проверок без ошибок**: четыре prefab, точные классы, прикреплённые прицелы M21, replication ID состояния, независимость экземпляров, накопление, отклонение некорректных запросов, пределы и сброс. Evidence runs: `runtime-20261116T142116295785Z`, `runtime-20261116T142424903953Z`. Это отдельная проверка ресурсов и серверного state; она не заменяет клиентский ADS/MP тест.

## Фактическая проверка сети и JIP

В установленной **1.9.0.37** выполнен отдельный сетевой запуск с server-процессом, первым клиентом и новым клиентским процессом после выхода первого. Evidence: `E:\CodexWork\MRAD-QA-profile\network-20261116T151544271791Z`. Все три процесса завершились с кодом 0; итог runner `passed=true`, без `MRAD_NETWORK_FAIL`.

| Peer/этап | Проверенные реальные реплики |
|---|---|
| Первый клиент, начальное состояние | AUTO: 10 строк, page index 1; MRAD scope: elevation 10 ticks, windage 30 ticks. |
| Тот же клиент после изменения сервером | AUTO: 12 строк, page index 0; scope: 17/19 ticks. Клиент сравнил каждую строку и её validity/status с отдельным authority snapshot. |
| Новый JIP-клиент | Сразу получил второе состояние: 12 строк, page 0, scope 17/19 ticks. Первое состояние ему локально не задавалось. |
| Сервер | Два final ACK от двух последовательно подключившихся настоящих клиентских процессов; `phase=2 finalAcks=2 failures=0`. |

Manifest проверенного snapshot: SHA-256 `5c5a3394f51766fdecd0e692893f9a69034758acd33048b6a5920f33364caa98`. Проверка script checksum самим движком включена. Предыдущий запуск с успешным live update и ошибкой JIP (`network-20261116T151143025079Z`) был отвергнут из-за изменения исходников между процессами, что прямо зафиксировано как `RplConnection::ValidationError`; это не принято за дефект репликации продукта.

Harness использует native owned `SCR_PlayerController` для QA-ACK и явный observer/streaming у тестовой сцены. Он читает фактические notebook/scope компоненты через `Replication.FindItem`, без клиентского пересчёта или локальной копии state. Это доказывает сетевую передачу таблицы, страниц и ticks, live update и JIP. Он не проверяет допуск **production** zeroing RPC в ADS, перенос предмета, физические клавиши или попадания. Для этих границ подготовлен отдельный opt-in `--ads` и остаётся отдельный AC-11.

Для opt-in helper подтверждены установленные публичные API: `SCR_PlayerController.SetInitialMainEntity`, `CharacterInputContext.SetRaiseWeapon/SetWeaponADS`, `CharacterControllerComponent.SetWeaponADS/SetWeaponADSInput`, `ActionManager.ActivateAction/SetActionValue`, `MenuManager.CloseAllMenus`. По установленному `SCR_PlayerController.c:425–442` метод `SetInitialMainEntity` выполняет main-entity setup, `RplComponent.GiveExt`, отключает AI и вызывает `SetControlledEntity`. Один `SetControlledEntity` не передаёт владение; клиентский QA проверяет фактический `RplComponent.IsOwner`. ADS entry в fixture задаётся публичным `SetWeaponADS`, после чего обязательны реальная native camera activation на клиенте и серверное состояние ADS. Это не проверка физической клавиши входа в ADS. Прямой `SetDisableControls` недоступен из helper; он не используется. Нативный `SCR_PlayerController.OnUpdate` сам снимает блокировку после закрытия меню. Проверка ActionManager pulse отдельно считает DOWN-dispatch; это не заменяет проверку физического Shift/key mapping.

**Статус opt-in ADS:** полного PASS нет. В `network-20261116T151645407367Z` и `network-20261116T152116174829Z` пройдены две data snapshots и первые два этапа helper, после чего достигнут лимит ожидания входа в native ADS на этапе 3. Второй запуск подтвердил фактическое владение персонажем, raised/input flags и отсутствие открытого меню. Последняя revision со scripted `SetWeaponADS` подготовлена, но завершённого подтверждающего прогона для неё нет. Это ограничение проверки; оно не превращается в утверждение о пройденных ADS, клавишах, камере или AC-11.

## Граница подтверждения

Runtime-поворот камеры не равен доказанному прохождению AC-11. Для принятия нужна проверка в установленной игре с сервером и двумя клиентами: оба типа оптики, обе оси, перенос прицела, вход нового клиента, стандартный и изменённый zeroing. Нельзя объявлять попадания или совместимость с ACE проверенными только по чтению API.

Реализация не вмешивается в native projectile/muzzle callbacks и не применяет дополнительный поворот к снаряду: двойная коррекция была бы ошибкой. Проверяется штатная связь изменённой линии прицеливания, действий игрока и реплицированной ориентации игрового оружия. Если эта связь не проходит AC-11, релиз не считается принятым; произвольный неподтверждённый hook на выстрел не добавляется.

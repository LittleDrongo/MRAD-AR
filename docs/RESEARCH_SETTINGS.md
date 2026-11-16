# MRAD 2.0 — настройки, ввод и ресурсы демонстрации

Проверено 16 ноября 2026 на установленном **Arma Reforger / Workbench 1.9.0.37**. Финальный двухпроцессный GUI gate в 15:29 UTC прошёл: настоящее Settings/MRAD, девять native строк, все 13 runtime-переводов и сохранение/загрузка/точное восстановление native binding после перезапуска. [Результат и SHA-256 полных логов](qa/2026-11-16/final-gui-result.json) и [чистый снимок меню](qa/2026-11-16/settings-mrad.png) сохранены в репозитории. Захват физических клавиш и применение параметров через пользовательский ввод не проверялись; успешный native API round-trip их не подменяет.

## Первичные источники и R-8

Штатным `FileIO.CopyFile` из смонтированных ресурсов игры извлечены:

- `Configs/System/chimeraInputCommon.conf` и `Configs/System/keyBindingMenu.conf`;
- `Scripts/Game/UI/Menu/SubMenu/SCR_SettingsSuperMenu.c`;
- `Scripts/Game/UI/Menu/SettingsMenu/SCR_SettingsManager.c`, `SCR_SettingsManagerKeybindModule.c`, `SCR_SettingsSubMenuBase.c`, `SCR_KeybindSettingsSubMenu.c`, `SCR_KeybindRowComponent.c`;
- `UI/layouts/Menus/SettingsMenu/BindingMenu/KeybindRow.layout`, `KeybindSettings.layout`, `GameplaySettings.layout`.

Публичная Doxygen 1.9 подтверждает `SCR_TabViewComponent.AddTab`, `InputBinding`, `JsonLoadContext` и `JsonSaveContext`. Для сочетаний использована также первичная [документация Input Manager](https://community.bistudio.com/wiki/Arma_Reforger:Input_Manager). Порядок источников в `InputSourceCombo` существенен: сначала модификатор, затем основная клавиша.

В Controls категории представлены штатным переключателем категории. Реализация добавляет категорию **MRAD** в Controls и отдельную вкладку **MRAD** в общем Settings через `SCR_TabViewComponent.AddTab`. Вторая вкладка содержит параметры и те же девять штатных строк переназначения. Единственный `modded SCR_SettingsSuperMenu` вызывает `super.OnMenuOpen()`, проверяет отсутствие вкладки с идентификатором `MRAD` и добавляет её; существующие вкладки не заменяются.

## Регистрация и объединение конфигов

В моде находятся частичные расширения двух штатных конфигов. GUID сохранены по результату `Workbench.GetResourceName`, чтобы движок объединил ресурсы:

| Ресурс | GUID | Исходный массив | Массив после подключения MRAD |
|---|---|---:|---:|
| `Configs/System/chimeraInputCommon.conf`, Actions | `795184CF9AD764DB` | 307 | **316** |
| Тот же ресурс, Contexts | тот же | 100 | **101** |
| `Configs/System/keyBindingMenu.conf`, m_KeyBindingCategories | `4EE7794C9A3F11EF` | 9 | **10** |

Источник фактических counts: `E:\CodexWork\MRAD-QA-profile\export-20261116T141820172410Z\logs\script.log`, markers `MRAD_CONFIG_ARRAY`. Сравнивались явный vanilla alias и общий ресурс. Это подтверждает сохранение исходных массивов при добавлении MRAD; совместимость с каждым сторонним модом отдельно не проверялась.

## Действия ввода

Контекст `MRAD_OpticsContext` использует `Priority 40`, `Flags 0xa 0`, как штатный контекст увеличения оптики. Компонент оптики активирует его только у локального игрока при использовании поддерживаемого прицела в ADS; настройки сами контекст не активируют.

| Действие | Назначение по умолчанию |
|---|---|
| `MRAD_ElevationUpFine` | ↑ |
| `MRAD_ElevationDownFine` | ↓ |
| `MRAD_WindageLeftFine` | ← |
| `MRAD_WindageRightFine` | → |
| `MRAD_ElevationUpCoarse` | левый или правый Shift + ↑ |
| `MRAD_ElevationDownCoarse` | левый или правый Shift + ↓ |
| `MRAD_WindageLeftCoarse` | левый или правый Shift + ← |
| `MRAD_WindageRightCoarse` | левый или правый Shift + → |
| `MRAD_ResetZeroing` | не назначено; задаётся игроком |

Сброс оставлен без клавиши, чтобы не назначать его на Reload. Все девять действий имеют `click` preset и запись в штатном меню биндов. `InputSourceSum` позволяет хранить несколько сочетаний; оба Shift заданы отдельными `InputSourceCombo`. Coarse-действия перечислены перед Fine. Сценарий Shift+стрелка без двойного изменения ещё требует реальной проверки ввода.

Строки используют именно общий `InputBinding` из `SCR_SettingsManagerKeybindModule.GetInputBindings()`: этот же объект изменяет штатный диалог захвата. Для Reset, Unbind и Advanced подключены те же callback-пути, что в vanilla. После захвата или закрытия Advanced строка пересоздаётся отложенно, чтобы обновить текст сочетания и не удалить widget внутри его callback. Reset и Unbind обновляют строку и сохраняют binding штатными методами. Бинды сохраняет штатный `InputBinding`, отдельно от JSON параметров MRAD.

## Параметры профиля

Класс [MRAD_Settings](../MRAD/scripts/Game/MRAD/Settings/MRAD_Settings.c) предоставляет `MRAD_Settings.Get()`, getters и revision для обновления блокнота/оптики. Путь сохранения: `$profile:MRAD/settings.json`.

| Поле JSON | Default | Допустимые значения |
|---|---|---|
| `referenceWindSpeed` | 4.0 | 0–30, округление до 0.1 |
| `useActualWind` | false | false / true |
| `maxRange` | 1500 | 100–5000, округление вниз до сотен |
| `rowsPerPage` | 10 | 5–12 |
| `fineStepMrad` | 0.1 | 0.1–5, шаг 0.1 |
| `coarseStepMrad` | 1.0 | 0.1–10, шаг 0.1 |
| `elevationLimitMrad` | 50 | 1–50, шаг 0.1 |
| `windageLimitMrad` | 50 | 1–50, шаг 0.1 |
| `showHUD` | true | false / true |

`Apply` нормализует значения, сразу увеличивает revision и сохраняет профиль. Ошибка сохранения показывается отдельно: применённые значения продолжают действовать в текущем запуске. Отсутствующие поля JSON сохраняют defaults. Повреждённый файл не мешает запуску с defaults. Кнопка восстановления сбрасывает параметры MRAD; отдельные кнопки штатных строк сбрасывают бинды. Числовые поля принимают точку или запятую, отклоняют посторонние символы и значения вне подписанных границ.

Профиль dedicated server отдельный. Пользовательские пределы могут сузить серверные ограничения; клиентское значение не расширяет серверный предел. Подробности репликации находятся в [исследовании оптики](RESEARCH_OPTICS.md).

## Локализация и layout

[MradLang.st](../MRAD/UI/Lang/MradLang.st) содержит **50 ключей × 13 языков**: en_us, ru_ru, uk_ua, de_de, fr_fr, it_it, es_es, cs_cz, pl_pl, pt_br, ja_jp, ko_kr, zh_cn. Сохранены прежние GUID трёх исходных строк и GUID таблицы. Проверено совпадение `%1`/`%2` между переводами. Четыре собственных имени ART II / M21 одинаковы во всех языках. Для приблизительного результата блокнота предусмотрена отдельная строка `MRAD_EstimatedWind`.

Одной `.st` недостаточно для игры: нужны собранные `.conf` и регистрация в `WidgetManagerSettings.StringTables` проекта. Это требование подтверждено [официальным руководством локализации](https://community.bistudio.com/wiki/Arma_Reforger:Mod_Localisation) и проверкой реального клиента. В `addon.gproj` добавлена собственная `StringTableDefinition` с 13 ссылками на прежние runtime GUID; конфигурации остальных пяти платформ наследуют PC, как в установленном `ArmaReforger.gproj`. Для ja/ko/zh сохранены штатные EastAsian/font параметры. Сборка выполняется native `LocalizationEditor -run -load ... -build -saveDir ...`, документированным в [Startup Parameters](https://community.bistudio.com/wiki/Arma_Reforger:Startup_Parameters).

Расхождение с tutorial: свойство `TargetPrefix`, рекомендуемое веб-страницей, **не принимается парсером установленной 1.9.0.37** (`Unknown keyword/data 'TargetPrefix'`). В итоговую `.st` оно не включено; имена переводов остаются `Target_<language>`. Это выявлено запуском String Editor, а не предположением о формате.

Штатная сборка успешно выполнена в `E:\CodexWork\MRAD-QA-profile\localize-20261116T144906863757Z`. Все 13 результатов перенесены в исходные `.conf`, прежние `.meta` сохранены. Дополнительно сопоставлены все **650 пар Id/Text**: у каждого языка 50 уникальных ID, ровно 50 текстов, и каждый текст точно соответствует своему `Target_<language>` в `.st`, включая Unicode и placeholders. Разрешение этих строк в живом GUI проверяется отдельно.

`UI/layouts/MRADSettings/MRAD_Settings.layout` имеет GUID `7AF9376463AB7151`. Два scroll-контейнера разделяют параметры и штатные keybind rows; font использует дефолт UI движка. В исходниках проверены все имена, которые ищет скрипт. Отрисовка, gamepad navigation и ширина переведённых строк требуют визуальной проверки.

## Очистка существующих демонстрационных миров

US catalog теперь регистрирует существующий `RangeCard_Notepad_AUTO.et` вместо шести удалённых специализированных блокнотов. В шести файлах world layers заменены **18 деклараций ссылок** на отсутствующие старые ресурсы: блокноты → AUTO, старые винтовки/прицелы → примеры MRAD M21/ART II 2D и PIP, магазины/бинокли → доступные vanilla. Координаты оставшихся сущностей сохранены. Удалены старый `RHS_SlidingInventoryStorageSlot` override и восемь недоступных тестовых сущностей: три debug mesh, один GM94, четыре VSS magazine. Пустой `vss.layer` оставлен зарегистрированным, как другие уже пустые слои проекта. Три пути боеприпасов приведены к действующим vanilla-путям с сохранением GUID.

Все **34 уникальных ресурса**, на которые прямо ссылаются текущие `.ent`/`.layer`, сопоставлены: 27 GUID и путей присутствуют в установленной vanilla `resourceDatabase.rdb`, семь совпадают с локальными MRAD `.meta`. Несовпадений GUID/пути нет. Проверка каталога ресурсов подтверждает разрешимость прямых ссылок, но не заменяет загрузку миров и осмотр сцены.

## Проверено и осталось проверить

- Компиляция текущих Settings scripts с `JsonLoadContext` / `JsonSaveContext`, bridge для native callbacks и добавлением вкладки успешно прошла в интеграционной проверке 14:17 UTC; deprecated `SCR_Json*` aliases удалены.
- Объединение Actions, Contexts и категорий подтверждено runtime resource inspection указанным выше логом.
- Статически проверены девять action definitions / context refs / menu entries, обе Shift-комбинации для четырёх направлений, локализация и имена widgets.
- Реальный GUI executable открыл Settings/MRAD: параметры и все девять native rows видны при 1440×900; 122 проверки Settings/UI и затем 26 проверок настоящего меню и видимости его вкладки выполнены. Все 13 языков проверены через реальное переключение WidgetManager и сравнение переводов. Ошибки последующих вложенных probes перечислены отдельно ниже.
- **Ещё не подтверждены:** работа Apply/Defaults через пользовательский ввод, capture сочетаний, reset/unbind/advanced через UI, конфликты с существующими назначениями, восстановление настроек/биндов после пользовательского редактирования и перезапуска, gamepad navigation, ввод в игре и визуальная загрузка обновлённых миров. Отдельный native InputBinding restart gate подготовлен ниже. Эти пункты нельзя отмечать пройденными только по compile или counts.

## Подготовленная интеграционная проверка профиля и widgets

`tools/qa/MRAD_QA/Scripts/Game/MRAD_SettingsProbe.c` предоставляет `static int MRAD_SettingsProbe.Run(bool allowProfileWrite = false)`. Возвращается число failures. Для **изолированного** QA-профиля runner вызывает `Run(true)`. Без явного opt-in JSON-проверки дают `MRAD_SETTINGS_PROFILE_SKIP`.

Проверяется реальный код `MRAD_Settings`, без обращения к singleton: test subclass открывает protected `Load` и `Sanitize`. `Apply` сохраняет отличающиеся от defaults значения, новый экземпляр читает их из настоящего `$profile:MRAD/settings.json`, затем проверяются частичный старый JSON и `RestoreDefaults`. Исходный файл копируется в `qa-settings-backup.json` и восстанавливается после пробы; если его не было, fixture удаляется. Существующий backup блокирует новые записи, чтобы не затереть следы прерванной проверки. Ошибка восстановления считается failure, backup остаётся доступным.

При наличии GUI workspace проба создаёт настоящие notebook, HUD и Settings layouts, проверяет widget types/names и создание девяти штатных `SCR_KeybindRowComponent` с текущим `InputBinding`. Ни capture, ни изменение/сохранение биндов проба не запускает. Корни временно скрыты и удаляются в том же синхронном вызове. Это проверка загрузки ресурсов и их связи со скриптом, а не оценка внешнего вида.

`System.IsConsoleApp()` в установленной Doxygen определён как режим без rendering/audio/input. В этом случае UI получает **`SKIP_HEADLESS`**, даже если какой-либо workspace API существует. При отсутствии workspace или native input services также выводится явный SKIP/partial. Итоговый marker:

```text
MRAD_SETTINGS_RESULT checks=N failures=N profile=PASS|FAIL|SKIP_NOT_REQUESTED ui=LAYOUT_AND_ROWS_PASS|FAIL|PARTIAL_NO_NATIVE_INPUT|SKIP_HEADLESS|SKIP_NO_WORKSPACE|SKIP_NO_GAME
```

Даже `LAYOUT_AND_ROWS_PASS` сопровождается `MRAD_SETTINGS_VISUAL_SKIP`: визуальная проверка и capture остаются отдельными. После обнаружения проблемы локализации проба усилена: переключает 13 языков, проверяет по два известных перевода, возвращает исходный язык, затем проверяет разрешение восьми ключей текущего языка. Инициализация widgets сама по себе не доказывает доступность переводов.

## Сценарий GUI и сохранения после перезапуска

Оба запуска клиента должны использовать **один и тот же отдельный профиль**, например `E:\CodexWork\MRAD-QA-gui\profile`, и разные каталоги логов. Обычный `run_runtime.py` создаёт новый профиль на каждый запуск, поэтому два его запуска не проверяют persistence. Для ручного меню достаточно включённого MRAD (`61E9542E592789AF`); runtime QA world загружать не следует — его runner автоматически завершает процесс после проб.

1. В первом запуске открыть штатные Settings, затем MRAD. Снять экран вкладки и категории Controls → MRAD. Проверить все девять строк, оба scroll-контейнера, видимые Apply/Defaults и отсутствие обрезанного текста при выбранном языке/разрешении.
2. Ввести `6,7` для reference wind; включить actual wind; задать range 2300, rows 7, fine 0.3, coarse 2.2, limits 12.3/14.6, выключить HUD. Нажать Apply. Должны появиться нормализованные значения и успешный статус. Ввести `abc` в числовое поле: Apply должен показать ошибку без изменения ранее применённых значений; затем вернуть допустимое число.
3. Переназначить одну fine- и одну coarse-команду настоящим native capture. Для coarse проверить захват сочетания, например Right Ctrl + NumPad6, и фактическое отображение обеих клавиш. Записать выбранные сочетания и любые сообщения о конфликте; не считать молчание интерфейса доказательством отсутствия конфликтов.
4. На отдельной строке проверить Unbind, затем Reset; на ещё одной — Advanced. Убедиться, что отображение строки обновилось сразу. Оставить две выбранные в предыдущем пункте команды переназначенными для проверки persistence. Назначить отдельную клавишу `MRAD_ResetZeroing` и зафиксировать её.
5. Перейти в Controls → MRAD: должны отображаться те же значения биндов, что во вкладке MRAD. Несколько раз открыть/закрыть Settings; вкладка и строки не должны дублироваться. Сохранить скриншоты параметров, назначений и JSON этого изолированного профиля.
6. Нормально завершить первый клиент. Запустить второй клиент с **тем же `-profile`**, не очищая профиль и не вызывая Reset. Повторно снять параметры и бинды: значения должны совпасть с зафиксированными до выхода. Два разных экземпляра `MRAD_Settings` в одном процессе этого требования не закрывают.
7. В поддерживаемой MRAD-оптике проверить стрелки и оба Shift: один coarse input должен изменять состояние ровно на выбранный coarse step, без дополнительного fine step. Проверить свои новые сочетания и reset. Вне ADS и при вводе текста в меню оптика меняться не должна. Это проверяет события ввода, а не визуальный layout probe.
8. Проверить Defaults параметров и отдельный Reset биндов, затем повторное чтение после выхода/возврата. Для заявленной gamepad-поддержки отдельно пройти navigation, capture и reset с контроллером. Отсутствие контроллера фиксировать как непроведённый пункт.

К приёмке приложить build, параметры запуска, два независимых набора логов, профильные значения до/после перезапуска, скриншоты и наблюдения ввода. Не заменять эти свидетельства одним `failures=0` из headless runner.

## Фактическая GUI-попытка 16 ноября 2026

Отдельный **ArmaReforgerSteamDiag.exe 1.9.0.37** запущен с MRAD, фиксированным `-profile E:\CodexWork\MRAD-UI-profile`, `-window`, 1440×900 и `-language ru_ru`. Параметры сверены с первичной [документацией Startup Parameters](https://community.bistudio.com/wiki/Arma_Reforger:Startup_Parameters). Установленная Doxygen также подтверждает `MenuManager.OpenMenu(ScriptMenuPresetEnum preset, int userId=0, bool unique=false, bool hideParentMenu=true)`; автоматическая подмена Settings layout вместо настоящего меню не использовалась.

Подтверждены запуск клиента, компиляция Game без `SCRIPT(E)` и отрисовка штатного MainMenu. Снимки сделаны через `PrintWindow` только client area собственного HWND. Перед действиями проверялись PID, время создания процесса и путь exe. Действующий Workbench PID13912 не закрывался и не использовался для UI-теста.

**Блокер:** Windows не передала foreground собственному клиентскому окну. Защитная проверка не разрешила `SendInput` или клики; адресованный только клиентскому HWND Escape не изменил интерфейс. Повторный запуск без `-noFocus` не устранил ограничение. Один клиент завершился в 14:35:12 UTC с записью `Game destroyed`, без предшествующей script error; причину выхода установить нельзя, поэтому он не классифицирован как падение MRAD. После ограничения дальнейших попыток оставшийся собственный клиент закрыт штатным `WM_CLOSE`.

Доказательства сохранены отдельно, без включения снимков сторонних приложений в репозиторий:

- `E:\CodexWork\MRAD-UI-profile\ui_result.json` — итог `BLOCKED_INPUT`;
- `gui-20261116T143127449146Z`, `gui-20261116T143406945951Z`, `gui-20261116T143620532239Z` под тем же каталогом — команды запуска, собственные client screenshots и логи.

Эта ранняя попытка не подтвердила открытие **Settings → MRAD**. Последующая автоматическая проверка настоящего меню описана ниже; native capture, применение через кнопки и persistence после GUI-редактирования остаются отдельными пунктами. Успешная отрисовка MainMenu их не закрывает.

## Автоматический запуск GUI executable без ввода

Клиент `ArmaReforgerSteamDiag.exe` с `-server` QA-world, `-noFocus`, `-forceUpdate` и отдельным профилем нормально завершился после встроенных проверок. Доказательства: `E:\CodexWork\MRAD-UI-profile\auto-20261116T144121990148Z`; дочерний PID2116, exit0, 10.26s. Никаких кликов, переноса foreground или взаимодействия с существующим Workbench не было.

В `script.log` получены `consoleApp=0`, `MRAD_SETTINGS_RESULT checks=88 failures=0 profile=PASS ui=LAYOUT_AND_ROWS_PASS`, `MRAD_NOTEBOOK_RESULT checks=19 failures=0` и `MRAD_RUNTIME_DONE validRows=12 failures=0`. Созданы настоящие layouts и девять native binding rows; notebook создал render target и завершил 100 циклов открытия/закрытия. Для fine зарегистрировано по одному keyboard binding, для coarse — по два, для reset — placeholder (это не назначенная клавиша).

**Этот результат не был принят как чистая GUI-приёмка.** Последующий разбор `console.log` обнаружил ошибки, не охваченные первоначальными assertions: отсутствующие переводы `#MRAD_*`, неподдерживаемое `Text` у семи EditBox, `Font Size` у двух CheckBox, а также `Visible` и font fallback у notebook. Свойства Settings удалены; локализация подключается и собирается штатным способом; notebook исправляется отдельно. Требуется повторный запуск нового snapshot и анализ console, а не только `failures=0`. Старый `result.json` отражает исходные assertions и сам по себе не доказывает исправление этих дефектов.

## Настоящее Settings/MRAD: запуск 14:55 UTC

После исправлений выполнен один отдельный GUI-клиент PID25884, exit0 за 20.63s. Доказательства: `E:\CodexWork\MRAD-UI-profile\visual-20261116T145553826909Z`.

| Проверка | Фактический результат |
|---|---|
| Settings, JSON, layouts, native rows и 13 языков | `checks=122 failures=0 profile=PASS ui=LAYOUT_AND_ROWS_PASS` |
| Notebook, включая создание RT и 100 циклов | `checks=19 failures=0` |
| Настоящий SettingsSuperMenu → MRAD | `checks=24 failures=0 tab=MRAD rows=9 language=en_us` |
| Game scripts | `SCRIPT(E)` отсутствует |
| Снимки собственного HWND1510574 | `settings-mrad-1.png`, `settings-mrad-2.png`, 1440×900 |

`MenuManager.OpenMenu(ChimeraMenuPreset.SettingsSuperMenu, 0, true)` создаёт настоящий `SCR_SettingsSuperMenu`; выбранная вкладка содержит именно product `MRAD_SettingsMenu`, заполненный через реальные lifecycle callbacks. На снимках читаются девять параметров, Apply/Restore defaults, девять native строк и штатные отображения стрелок/Shift. Отсутствующие переводы и прежние ошибки свойств Settings layout в этом запуске устранены.

Снимки также показали, что прямой программный переход с первой вкладки на девятую оставляет её кнопку вне видимой полосы. Исходник `SCR_TabViewComponent` объясняет это: native overflow сдвигает диапазон при достижении граничной вкладки, а прямой jump пропускает такой шаг. QA-helper изменён на точный путь штатной правой кнопки — `GetNextValidItem(false)` → `ShowTab` после первого layout update. Добавлены отдельные assertions видимости кнопки MRAD и её координат внутри окна. Усиленный gate подтверждён следующим запуском; product TabView не подменяется и вкладки других модов не изменяются.

Анализ console отделён от assertions. Найдено старое неподдерживаемое `m_bIsSavingEnabled` в mission header MRAD — удалено только это поле. Сохранилось сообщение `RigidBodyComponent ... Notepad_LP.xob ... without geometry`: предмет действительно создаётся, но collision geometry исходной модели отсутствует; корректность падения/коллизии нельзя считать подтверждённой. Также зарегистрированы ошибки штатных IntroSplashScreen/Rally resources и диагностические UI-resource leaks при завершении; они не скрыты из evidence. QA-helper теперь явно закрывает своё меню перед завершением клиента.

Первоначальный wrapper ошибочно причислил восемь строк `MRAD_SETTINGS_PASS ... string resolves #MRAD_...` к missing strings. Фильтр исправлен: учитываются только строки уровней `(E)`/`(W)`. Исходный отчёт оставлен как история; подтверждение переводов основано на 26 сравнительных проверках языков, восьми разрешениях ключей и наблюдаемой отрисовке.

## Воспроизводимый GUI runner и native binding restart gate

`tools/qa/run_gui.py` требует Windows, Python 3.10+ и **Pillow только для `--visual`**. Оно должно быть доступно выбранному Python до запуска. Runner создаёт новый дочерний профиль под явно указанным task-каталогом, сохраняет команды/логи/результаты и ограничивает каждый свой процесс timeout. `PrintWindow` снимает только client area HWND, чей PID совпадает с дочерним процессом; дополнительно проверяется путь exe через удерживаемый process handle. Клавиатурный ввод, клики, перенос foreground и окна других программ не используются.

Установленный debug-вход `-server <world>` использует **UDP 0.0.0.0:2001** и игнорирует переданные `-bindIP`/`-port`. Поэтому runner не передаёт эти неподдерживаемые overrides, разрешает только `--port 2001` и перед каждым процессом проверяет свободный порт через `SO_EXCLUSIVEADDRUSE`. Он не завершает чужие процессы и не меняет firewall. Ранние команды в исторических логах содержали 28765, однако это не было фактическим портом игры.

Пример PowerShell, после синхронизации и успешной компиляции QA addon:

```powershell
python .\tools\qa\run_gui.py `
  --game "E:\Steam\steamapps\common\Arma Reforger\ArmaReforgerSteamDiag.exe" `
  --addons-dir "E:\Steam\steamapps\common\Arma Reforger\addons,E:\CodexWork\MRAD-AR,E:\CodexWork\MRAD-AR\tools\qa" `
  --profile "E:\CodexWork\MRAD-UI-profile" `
  --visual --binding-roundtrip --timeout 60
```

`--binding-roundtrip` выполняет ровно два последовательных клиента с одним новым профилем. Первый использует `-mradBindingWrite`: `MRAD_InputBindingProbe` требует исходное default-состояние `MRAD_ResetZeroing`, отказывается при существующем custom binding/backup, сохраняет snapshot исходных inputs и активного key stack, затем через штатные `CreateUserBinding`, `AddBinding`, `InsertCombo`, `Save` назначает временный Right Ctrl + NumPad6. Проверяются custom flag, количество сочетаний, новый экземпляр InputBinding и активная карта `InputManager.GetActionKeybinding`, включая оператор объединения двух клавиш.

Второй отдельный процесс с `-mradBindingRead` проверяет загруженное сочетание до любых изменений, затем `ResetDefault`/`Save` возвращают исходные raw inputs и default flag. Backup удаляется только после точного совпадения. Обязательны оба markers `MRAD_INPUT_RESULT phase=WRITE ... failures=0` и `phase=READ_RESTORE ... failures=0`; ни exit0, ни один только файл сохранения не считаются достаточными. При аварии первого процесса второй автоматически не запускается, а оставшийся backup сохраняется в изолированном профиле.

Опциональный `--native-fire` передаёт `-mradNativeFire` и требует отдельный строгий `MRAD_NATIVE_FIRE_RESULT originalShots=3 modifiedShots=3 ... failures=0`. По умолчанию этот сценарий не включается. Подтверждающий GUI-прогон этого режима не выполнен; результат остаётся непроверенным и не включается в итоговую приёмку Settings/InputBinding.

### Первый двухпроцессный результат и уточнение getter

Доказательства: `E:\CodexWork\MRAD-UI-profile\gui-20261116T150837660916Z`. Клиенты PID8256 и PID27488 нормально завершились, общий runtime35.45s. В первой фазе native paging прошёл восемь шагов; `MRAD_SETTINGS_VISUAL_RESULT checks=26 failures=0`. Кнопка MRAD видима и выделена; её прямоугольник x1143/y126.525/150×30 находится внутри 1440×900. В последней строке меню действительно отображается **RCTRL + NUM6**. Снимки остаются приватными доказательствами запуска: в верхней части присутствуют игровые уведомления с display name.

Сохранённый `written-binding-1.conf` содержит настоящий `InputSourceCombo`: Right Ctrl, NumPad6, preset click и InputFilterDown. В новом процессе **до восстановления** активная карта снова вернула `keyboard:KC_NUMPAD6`, `keyboard:KC_RCONTROL`, `+`. Затем default flag, количество и исходные inputs восстановлены, backup удалён. Это фактическое наблюдение сохранения через перезапуск.

Общий gate первого запуска всё же **FAIL**: дополнительный `InputBinding.GetBindings(..., "click", false)` вернул false/пустой flat list для этого combo, хотя native active map и меню прочли обе клавиши. Исходные результаты сохранены (`WRITE checks16 failures4`, `READ_RESTORE checks11 failures2`). Диагностика исправлена: этот flat getter теперь только логируется; обязательными остаются независимые custom flag/count, **активные две клавиши и оператор `+`**, отдельный перезапуск и точное восстановление активного stack из backupV2. Исправленный gate подтверждён новым финальным запуском ниже; старые markers не переименованы в PASS.

Дополнительно новый inventory probe сообщил `checks13 failures1 weaponChanges100 magazineChanges1` с ошибкой проверки изменённого drop после замены магазина; это отдельная проверка notebook, её результат не скрывается как ошибка Settings. В обеих GUI-фазах `unknown_keywords=[]` и `unresolved_mrad_strings=[]`; прежняя ошибка mission header устранена. Сообщение об отсутствии geometry у Notepad_LP и shutdown resource diagnostics остаются в логах.

Native binding storage и physical capture проверяются отдельно. `result.json` сообщает каждый результат, MRAD resource errors, parser failures, снимки с требованием визуального review и непроведённый physical capture. Ни положительный результат вложенной пробы, ни exit0 не превращают весь integration run в PASS при оставшейся ошибке.

## Финальный GUI gate: 15:29 UTC — PASS

Run `gui-20261116T152920832286Z` использовал неизменный snapshot с исправленной prefab-геометрией блокнота. Выполнены ровно два последовательных GUI-клиента, PID28400 и PID25504, на одном новом изолированном профиле. Оба завершились штатно с exit0; runner завершился за 31.40s. `--native-fire` не включался. Никаких действий с пользовательским профилем, клавиатурой, foreground или существующим Workbench не было.

| Проверка | Write | Отдельный процесс Read/Restore |
|---|---:|---:|
| Native InputBinding | 16 checks / 0 failures | 11 checks / 0 failures |
| Settings: JSON, layouts, девять строк, 13 языков | 122 / 0 | 122 / 0 |
| Notebook, создание RT и 100 циклов | 19 / 0 | 19 / 0 |
| Реальные изменения inventory | 16 / 0; 100 смен оружия и 1 замена магазина | 16 / 0; те же операции |
| Optics state checks | 49 / 0 | 49 / 0 |
| Runtime | 12 valid rows / 0 failures | 12 valid rows / 0 failures |
| Настоящее Settings/MRAD и native paging | 26 / 0; девять строк | меню повторно не открывалось |

Активная карта ввода вернула обе клавиши `KC_RCONTROL`, `KC_NUMPAD6` и оператор `+` в первом процессе, через новый объект InputBinding и **после отдельного перезапуска до восстановления**. Затем `ResetDefault`/`Save` восстановили default flag, количество bindings, исходные raw inputs и точный активный stack. Backup удалён только после совпадения. Итоговые `binding_roundtrip_pass=true` и `binding_backup_removed=true` подтверждены; сохранённый [native binding fixture](qa/2026-11-16/final-gui-binding.conf) содержит только это назначение.

В обоих процессах нет `SCRIPT(E)`, `Unknown keyword/data`, неразрешённых `MRAD_*` строк и ошибок ресурсов MRAD. Прежнее `without geometry` отсутствует: notebook имеет один физический collider, layers20480, mass1; его реальные размеры `0.11239 × 0.0138092 × 0.172461` совпадают с model bounds с учётом округления. Это подтверждает создание и размер prefab collider; отдельный ручной сценарий бросания и подбора предмета здесь не выполнялся.

Второй снимок собственного HWND снят спустя семь секунд после готовности меню, когда временные уведомления исчезли. Он осмотрен до публикации: имя аккаунта и Steam popup отсутствуют; вкладка MRAD видима и выделена, все параметры и девять строк читаются, временное назначение **RCTRL + NUM6** показано в последней строке. Первый снимок с уведомлением аккаунта в репозиторий не включён. Опубликованный PNG перенесён без редактирования; SHA-256 совпадает с исходным.

Сохранились сообщения установленной игры/QA-окружения: неизвестный класс у IntroSplashScreen, GUID/name mismatch штатных Rally-ресурсов, отсутствие AIWorld у минимальной QA-карты, штатное предупреждение EditBoxFilter и UI-resource leak diagnostics при завершении. В отдельном QA network probe также остаются deprecated observer warnings. Они перечислены в metadata; результат PASS относится к указанным MRAD gates, а не к отсутствию любых сообщений движка.

Доказательства без абсолютных launch commands и данных аккаунта: [write markers](qa/2026-11-16/final-gui-write.log), [read/restore markers](qa/2026-11-16/final-gui-read.log), [metadata и хеши десяти полных логов](qa/2026-11-16/final-gui-result.json), [снимок меню](qa/2026-11-16/settings-mrad.png). Физический capture, клики Apply/Defaults и ADS/попадания остаются за пределами этой проверки.

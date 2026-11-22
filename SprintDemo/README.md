# SprintDemo

Отдельный проект Unreal Engine 5.8 на основе исходных `SprintCharacter.h` и `SprintCharacter.cpp` из родительской папки.

## Запуск на этом Mac

1. Открой `Launch.command` двойным щелчком. Скрипт соберёт C++ и откроет редактор.
2. Дождись загрузки карты и компиляции шейдеров при первом открытии.
3. Нажми **Play** вверху редактора и щёлкни внутри игрового окна.

Также можно открыть `SprintDemo.uproject` в Unreal Engine 5.8.

**Управление:** WASD — движение, мышь — камера, Space — прыжок, Left Shift — спринт, E — взаимодействие, Esc — выход из Play, Shift+F1 — освободить курсор.

Скорость ходьбы — 500, спринта — 900. Выносливость — 100; при движении со спринтом расходуется 25/с, после отключения спринта восстанавливается 15/с. После истощения отпусти Shift и нажми снова. При удержании Shift без движения выносливость не меняется, как в исходном коде.

## Состав

- Рабочая версия персонажа: `Source/SprintDemo/SprintCharacter.*`. Исходные файлы в родительской папке сохранены.
- Карта, модель и анимации — из установленного Third Person Template UE 5.8.
- `BP_ThirdPersonCharacter` наследуется от `ASprintCharacter`; E вызывает C++ делегат `OnInteract`, на который подписан Blueprint с `Print String`.
- `IA_Sprint` и `IA_Interact` добавлены в `IMC_Default`.
- Анимация спринта настроена в `ABP_Unarmed` (название стандартного Animation Blueprint в UE 5.8). Флаг поступает из `SprintDemoAnimInstance`.
- Добавлены полоса выносливости и подсказки управления.
- Для совместимости с шаблоном 5.8 к исходному персонажу добавлены отдельное действие обзора мышью и функции ввода для виджетов шаблона.
- Вспомогательный генератор Blueprint и интеграционный тест адаптированы из локального проекта `hw_une`; сам `hw_une` не изменялся.

## Сборка и проверка из терминала на macOS и Linux

Из этой папки:

```bash
export UE_ROOT="/path/to/UE_5.8"
bash Scripts/build.sh
bash Scripts/setup_assets.sh -ValidateOnly
bash Scripts/test.sh
```

Команда `bash Scripts/setup_assets.sh` повторно настраивает Blueprint-ассеты ДЗ 1. Арена ДЗ 2 собирается при каждом Play на `Lvl_ThirdPerson`.

На Windows соберите конфигурацию **Development Editor** с целью `SprintDemoEditor` в Visual Studio. Для запуска обоих тестов в редакторе откройте вкладку **Automation** в **Session Frontend** и выберите фильтр `SprintDemo`.

## Интеграционные тесты

- `SprintDemo.Integration.InputBlueprintAndAnimation` проверяет ввод, движение, прыжок, выносливость, делегат взаимодействия и переходы Blueprint-анимации из ДЗ 1.
- `SprintDemo.Integration.PCGGuardBehavior` запускает Play на `Lvl_ThirdPerson`, проверяет количество экземпляров PCG и обход препятствия по NavMesh. Затем тест имитирует нажатия **W** и **Shift** и проверяет патруль, поиск по шуму за стеной, перехват поисковой ветки при обнаружении игрока и возврат к патрулю после потери цели.

`Scripts/test.sh` запускает оба теста в редакторе с NullRHI и проверяет результаты в новом `Saved/Automation/Run.*/index.json`. Для проверки внешнего вида арены используйте обычный Play.

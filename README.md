# ДЗ 1: Расширение Third Person Template

Проект на базе шаблона **Third Person (C++)** в Unreal Engine 5.

## Что сделано

1. **Наследование.** Создан C++ класс `ASprintCharacter` (наследник `ACharacter`) с камерой, движением и обработкой ввода. `BP_ThirdPersonCharacter` переназначен на него как на родительский класс.
2. **Enhanced Input.** Добавлены `IA_Sprint` (Left Shift) и `IA_Interact` (E), оба внесены в `IMC_Default`. Обработчики привязаны через `BindAction` в `SetupPlayerInputComponent`.
3. **Делегаты и логика.**
   - `FOnInteractSignature` — Dynamic Multicast Delegate, вызывается (`Broadcast`) по нажатию E. В `BP_ThirdPersonCharacter` на него подписан Print String «Interaction triggered!».
   - Спринт: при зажатом Shift `MaxWalkSpeed` повышается с 500 до 900, при отпускании возвращается. Состояние хранится в `UPROPERTY(BlueprintReadOnly) bool bIsSprinting`.
4. **Анимации.** В `ABP_Manny` (Event Graph) значение `bIsSprinting` читается из персонажа. В State Machine добавлено состояние `Sprint` с анимацией бега и переходы `Idle/Walk/Run ↔ Sprint` по `bIsSprinting`.
5. **Доп. задание — выносливость (Stamina).** Реализована на C++ в `Tick`: во время бега выносливость тратится (25 ед./с), без бега восстанавливается (15 ед./с). Когда она доходит до 0, бег прекращается, даже если Shift зажат. Текущее значение выводится на экран (`AddOnScreenDebugMessage`).

## Управление

| Клавиша | Действие |
|---|---|
| W A S D | Движение |
| Мышь | Камера |
| Space | Прыжок |
| Left Shift (зажать) | Спринт |
| E | Взаимодействие |

## Код

- `Source/<Project>/SprintCharacter.h`
- `Source/<Project>/SprintCharacter.cpp`

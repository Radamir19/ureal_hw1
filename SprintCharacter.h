#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SprintCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;

// Делегат взаимодействия: вызывается при нажатии E, на него подписывается BP_ThirdPersonCharacter
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractSignature);

/**
 * Базовый персонаж для Third Person Template:
 * движение/камера, спринт (Left Shift), взаимодействие (E), выносливость.
 */
UCLASS(config = Game)
class ASprintCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASprintCharacter();

	virtual void Tick(float DeltaSeconds) override;

	/** Событие взаимодействия (Broadcast при нажатии E) */
	UPROPERTY(BlueprintAssignable, Category = "Interaction")
	FOnInteractSignature OnInteract;

	/** Бежит ли персонаж сейчас (читается в Animation Blueprint) */
	UPROPERTY(BlueprintReadOnly, Category = "Sprint")
	bool bIsSprinting = false;

	/** Обычная скорость ходьбы */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprint")
	float WalkSpeed = 500.f;

	/** Скорость во время спринта */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sprint")
	float SprintSpeed = 900.f;

	/** Максимальная выносливость */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina")
	float MaxStamina = 100.f;

	/** Текущая выносливость */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Stamina")
	float Stamina = 100.f;

	/** Расход выносливости в секунду во время бега */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina")
	float StaminaDrainRate = 25.f;

	/** Восстановление выносливости в секунду, когда персонаж не бежит */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stamina")
	float StaminaRegenRate = 15.f;

protected:
	virtual void BeginPlay() override;
	virtual void NotifyControllerChanged() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	// --- Камера ---
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> FollowCamera;

	// --- Enhanced Input ---
	/** Необязательно: если задан, добавляется при получении контроллера */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> SprintAction;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	// --- Обработчики ввода ---
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartSprint();
	void StopSprint();
	void Interact();

private:
	void SetSprinting(bool bNewSprinting);
	void ShowStamina() const;

	/** Зажата ли сейчас клавиша спринта */
	bool bSprintKeyHeld = false;
};

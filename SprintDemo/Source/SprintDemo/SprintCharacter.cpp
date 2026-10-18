#include "SprintCharacter.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"

ASprintCharacter::ASprintCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.f);

	// Персонаж поворачивается по направлению движения, а не за камерой
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->bOrientRotationToMovement = true;
	Movement->RotationRate = FRotator(0.f, 500.f, 0.f);
	Movement->JumpZVelocity = 700.f;
	Movement->AirControl = 0.35f;
	Movement->MaxWalkSpeed = WalkSpeed;
	Movement->MinAnalogWalkSpeed = 20.f;
	Movement->BrakingDecelerationWalking = 2000.f;
	Movement->BrakingDecelerationFalling = 1500.f;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.f;
	CameraBoom->bUsePawnControlRotation = true;

	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	FollowCamera->bUsePawnControlRotation = false;
}

void ASprintCharacter::BeginPlay()
{
	Super::BeginPlay();

	Stamina = MaxStamina;
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed;
}

void ASprintCharacter::NotifyControllerChanged()
{
	Super::NotifyControllerChanged();

	// Добавляем Mapping Context (в шаблоне UE 5.4+ это делает и PlayerController — двойное добавление безопасно)
	if (APlayerController* PC = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
				ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			if (DefaultMappingContext)
			{
				Subsystem->AddMappingContext(DefaultMappingContext, 0);
			}
		}
	}
}

void ASprintCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent);
	if (!EIC)
	{
		UE_LOG(LogTemp, Error, TEXT("%s: Enhanced Input Component не найден"), *GetNameSafe(this));
		return;
	}

	// Базовые действия шаблона
	if (JumpAction)
	{
		EIC->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		EIC->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
	}
	if (MoveAction)
	{
		EIC->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ASprintCharacter::Move);
	}
	if (LookAction)
	{
		EIC->BindAction(LookAction, ETriggerEvent::Triggered, this, &ASprintCharacter::Look);
	}

	if (MouseLookAction)
    {
        EIC->BindAction(MouseLookAction, ETriggerEvent::Triggered, this, &ASprintCharacter::Look);
    }

    // Новые действия: спринт (зажатие/отпускание) и взаимодействие
	if (SprintAction)
	{
		EIC->BindAction(SprintAction, ETriggerEvent::Started, this, &ASprintCharacter::StartSprint);
		EIC->BindAction(SprintAction, ETriggerEvent::Completed, this, &ASprintCharacter::StopSprint);
		EIC->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ASprintCharacter::StopSprint);
	}
	if (InteractAction)
	{
		EIC->BindAction(InteractAction, ETriggerEvent::Started, this, &ASprintCharacter::Interact);
	}
}

void ASprintCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	if (!Controller)
	{
		return;
	}

	const FRotator YawRotation(0.f, Controller->GetControlRotation().Yaw, 0.f);
	const FVector Forward = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector Right = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	AddMovementInput(Forward, Input.Y);
	AddMovementInput(Right, Input.X);
}

void ASprintCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D Input = Value.Get<FVector2D>();
	AddControllerYawInput(Input.X);
	AddControllerPitchInput(Input.Y);
}

void ASprintCharacter::StartSprint()
{
	bSprintKeyHeld = true;
	if (Stamina > 0.f)
	{
		SetSprinting(true);
	}
}

void ASprintCharacter::StopSprint()
{
	bSprintKeyHeld = false;
	SetSprinting(false);
}

void ASprintCharacter::SetSprinting(bool bNewSprinting)
{
	bIsSprinting = bNewSprinting;
	GetCharacterMovement()->MaxWalkSpeed = bIsSprinting ? SprintSpeed : WalkSpeed;
}

void ASprintCharacter::Interact()
{
	OnInteract.Broadcast();
}

void ASprintCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Тратим выносливость только когда реально бежим (есть скорость), иначе восстанавливаем
	const bool bMoving = GetVelocity().SizeSquared2D() > 1.f;

	if (bIsSprinting && bMoving)
	{
		Stamina = FMath::Max(0.f, Stamina - StaminaDrainRate * DeltaSeconds);
		if (Stamina <= 0.f)
		{
			// Выносливость кончилась — прекращаем бег даже при зажатой клавише
			SetSprinting(false);
		}
	}
	else if (!bIsSprinting)
	{
		Stamina = FMath::Min(MaxStamina, Stamina + StaminaRegenRate * DeltaSeconds);
	}

	ShowStamina();
}

void ASprintCharacter::ShowStamina() const
{
	if (GEngine && IsLocallyControlled())
	{
		const FColor Color = Stamina > MaxStamina * 0.3f ? FColor::Green : FColor::Red;
		const FString Text = FString::Printf(TEXT("Stamina: %.0f / %.0f%s"), Stamina, MaxStamina,
			bIsSprinting ? TEXT("  [SPRINT]") : TEXT(""));
		// Ключ 1 — сообщение перезаписывается каждый кадр, а не копится
		GEngine->AddOnScreenDebugMessage(1, 0.f, Color, Text);
	}
}

void ASprintCharacter::DoMove(float Right, float Forward)
{
    Move(FInputActionValue(FVector2D(Right, Forward)));
}

void ASprintCharacter::DoLook(float Yaw, float Pitch)
{
    Look(FInputActionValue(FVector2D(Yaw, Pitch)));
}

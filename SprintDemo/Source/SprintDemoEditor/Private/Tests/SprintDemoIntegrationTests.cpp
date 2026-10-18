#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SprintDemoAnimInstance.h"
#include "SprintCharacter.h"
#include "Animation/AnimClassInterface.h"
#include "Animation/AnimStateMachineTypes.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "EnhancedPlayerInput.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputKeyEventArgs.h"
#include "InputMappingContext.h"
#include "Misc/OutputDevice.h"
#include "Misc/OutputDeviceRedirector.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
// Observe the actual Blueprint Print String output, without adding a second
// test listener to the character's gameplay delegate.
class FInteractionOutput final : public FOutputDevice
{
public:
    FInteractionOutput() { GLog->AddOutputDevice(this); }
    ~FInteractionOutput() override { GLog->RemoveOutputDevice(this); }

    void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity, const FName& Category) override
    {
        if (Category == TEXT("LogBlueprintUserMessages") &&
            FCString::Strstr(Message, TEXT("Interaction triggered!")))
        {
            ++MessageCount;
        }
    }

    int32 MessageCount = 0;
};

bool IsStateActive(const UAnimInstance* Instance, FName StateName)
{
    const IAnimClassInterface* AnimClass = Instance ? IAnimClassInterface::GetFromClass(Instance->GetClass()) : nullptr;
    if (!AnimClass)
    {
        return false;
    }
    for (const FBakedAnimationStateMachine& Machine : AnimClass->GetBakedStateMachines())
    {
        const int32 Index = Instance->GetStateMachineIndex(Machine.MachineName);
        if (Index != INDEX_NONE && Instance->GetCurrentStateName(Index) == StateName)
        {
            return true;
        }
    }
    return false;
}

// A real PIE session exercises the saved Blueprint defaults, local-player
// mapping contexts, Enhanced Input bindings, animation graphs and Blueprint VM.
// The test injects key events into the player controller, never calls gameplay
// handlers directly, and therefore also catches missing asset assignments.
class FExerciseSprintDemoInput final : public IAutomationLatentCommand
{
public:
    explicit FExerciseSprintDemoInput(FAutomationTestBase* InTest) : Test(InTest) {}

    bool Update() override
    {
        if (StartedAt == 0.0)
        {
            StartedAt = FPlatformTime::Seconds();
        }
        if (FPlatformTime::Seconds() - StartedAt > 60.0)
        {
            Test->AddError(FString::Printf(TEXT("PIE integration timed out at stage %d."), Stage));
            return true;
        }

        UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
        APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
        ASprintCharacter* Character = Controller ? Cast<ASprintCharacter>(Controller->GetPawn()) : nullptr;
        if (!Character || !Character->HasActorBegunPlay() || !Controller->PlayerInput)
        {
            return false;
        }

        if (Stage == 0)
        {
            // NullRHI has no rendered meshes; explicitly keep the real animation
            // graph evaluating so transitions can be checked in unattended runs.
            Character->GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
            Advance(World);
            return false;
        }

        const float Elapsed = World->GetTimeSeconds() - StageStartedAt;
        UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
        UAnimInstance* Animation = Character->GetMesh()->GetAnimInstance();
        switch (Stage)
        {
        case 1:
            if (Elapsed < 0.5f || Movement->IsFalling()) return false;
            Test->TestEqual(TEXT("The saved template Blueprint is the possessed pawn"),
                Character->GetClass()->GetName(), FString(TEXT("BP_ThirdPersonCharacter_C")));
            Test->TestTrue(TEXT("Input is handled by Enhanced Player Input"),
                IsValid(Cast<UEnhancedPlayerInput>(Controller->PlayerInput)));
            Test->TestTrue(TEXT("Pawn bindings use Enhanced Input"),
                IsValid(Cast<UEnhancedInputComponent>(Character->InputComponent)));
            {
                const UEnhancedInputLocalPlayerSubsystem* Subsystem =
                    ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(Controller->GetLocalPlayer());
                const UInputMappingContext* Context = LoadObject<UInputMappingContext>(nullptr,
                    TEXT("/Game/Input/IMC_Default.IMC_Default"));
                Test->TestTrue(TEXT("The template mapping context is active on the local player"),
                    Subsystem && Context && Subsystem->HasMappingContext(Context));
            }
            Test->TestTrue(TEXT("BeginPlay subscribed the Blueprint interaction event"),
                Character->OnInteract.Contains(Character, TEXT("HandleInteraction")));
            Test->TestNotNull(TEXT("Template camera exists"), Character->GetFollowCamera());
            Test->TestTrue(TEXT("Camera boom follows the controller rotation"),
                Character->GetCameraBoom() && Character->GetCameraBoom()->bUsePawnControlRotation);
            Test->TestTrue(TEXT("Animation Blueprint inherits the native sprint animation bridge"),
                IsValid(Cast<USprintDemoAnimInstance>(Animation)));
            InitialStamina = Character->Stamina;
            SendKey(Controller, EKeys::E, true);
            Advance(World);
            return false;

        case 2:
            if (Elapsed < 0.15f) return false;
            Test->TestEqual(TEXT("One E press executes Blueprint Print String exactly once"), Output.MessageCount, 1);
            SendKey(Controller, EKeys::E, false);
            InitialPosition = Character->GetActorLocation();
            SendKey(Controller, EKeys::W, true);
            SendKey(Controller, EKeys::LeftShift, true);
            Advance(World);
            return false;

        case 3:
            if (Elapsed < 0.35f) return false;
            Test->TestTrue(TEXT("Left Shift starts sprinting through the input mapping"), Character->bIsSprinting);
            Test->TestEqual(TEXT("Sprint input raises MaxWalkSpeed"), Movement->MaxWalkSpeed, Character->SprintSpeed);
            Test->TestTrue(TEXT("Sprint drains stamina during normal world ticks"), Character->Stamina < InitialStamina);
            Test->TestTrue(TEXT("W moves the possessed Blueprint character"),
                FVector::Dist2D(InitialPosition, Character->GetActorLocation()) > 10.0f);
            Test->TestTrue(TEXT("The real Animation Blueprint enters Sprint"), IsStateActive(Animation, TEXT("Sprint")));
            SendKey(Controller, EKeys::LeftShift, false);
            Advance(World);
            return false;

        case 4:
            if (Elapsed < 0.35f) return false;
            Test->TestFalse(TEXT("Releasing Left Shift stops sprinting"), Character->bIsSprinting);
            Test->TestEqual(TEXT("Releasing Left Shift restores walking speed"), Movement->MaxWalkSpeed, Character->WalkSpeed);
            Test->TestFalse(TEXT("The real Animation Blueprint leaves Sprint"), IsStateActive(Animation, TEXT("Sprint")));
            Test->TestEqual(TEXT("Interaction does not repeat while unrelated inputs run"), Output.MessageCount, 1);
            SendKey(Controller, EKeys::W, false);
            InitialYaw = Controller->GetControlRotation().Yaw;
            Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::MouseX, IE_Axis, 10.0f));
            Advance(World);
            return false;

        case 5:
            if (Elapsed < 0.1f) return false;
            Test->TestTrue(TEXT("Mouse input rotates the third-person camera"),
                FMath::Abs(FMath::FindDeltaAngleDegrees(InitialYaw, Controller->GetControlRotation().Yaw)) > 0.01f);
            SendKey(Controller, EKeys::SpaceBar, true);
            Advance(World);
            return false;

        case 6:
            if (Elapsed < 0.1f) return false;
            Test->TestTrue(TEXT("Space retains the template jump binding"), Movement->IsFalling());
            SendKey(Controller, EKeys::SpaceBar, false);
            return true;

        default:
            Test->AddError(TEXT("Unexpected integration test stage."));
            return true;
        }
    }

private:
    static void SendKey(APlayerController* Controller, FKey Key, bool bPressed)
    {
        Controller->InputKey(FInputKeyEventArgs::CreateSimulated(Key,
            bPressed ? IE_Pressed : IE_Released, bPressed ? 1.0f : 0.0f));
    }

    void Advance(UWorld* World)
    {
        ++Stage;
        StageStartedAt = World->GetTimeSeconds();
    }

    FAutomationTestBase* Test;
    FInteractionOutput Output;
    int32 Stage = 0;
    double StartedAt = 0.0;
    float StageStartedAt = 0.0f;
    float InitialStamina = 0.0f;
    float InitialYaw = 0.0f;
    FVector InitialPosition = FVector::ZeroVector;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSprintDemoInputAndBlueprintIntegration,
    "SprintDemo.Integration.InputBlueprintAndAnimation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSprintDemoInputAndBlueprintIntegration::RunTest(const FString& Parameters)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        AddError(TEXT("Run this integration test from the editor with no existing PIE session."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FExerciseSprintDemoInput(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif

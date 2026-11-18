#include "SprintGuard.h"
#include "SprintCharacter.h"
#include "SprintDemo.h"
#include "SprintGuardBehavior.h"
#include "Animation/AnimInstance.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "UObject/ConstructorHelpers.h"

ASprintGuard::ASprintGuard()
{
    AIControllerClass = ASprintGuardController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 96.f);
    bUseControllerRotationYaw = false;

    UCharacterMovementComponent* Movement = GetCharacterMovement();
    Movement->MaxWalkSpeed = SprintGuardBehavior::PatrolSpeed;
    Movement->bOrientRotationToMovement = true;
    Movement->RotationRate = FRotator(0.f, 540.f, 0.f);

    static const ConstructorHelpers::FObjectFinder<USkeletalMesh> MeshAsset(
        TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    static const ConstructorHelpers::FClassFinder<UAnimInstance> AnimationAsset(
        TEXT("/Game/Characters/Mannequins/Anims/Unarmed/ABP_Unarmed"));
    GetMesh()->SetSkeletalMeshAsset(MeshAsset.Object);
    GetMesh()->SetAnimInstanceClass(AnimationAsset.Class);
    GetMesh()->SetRelativeLocationAndRotation(FVector(0.f, 0.f, -96.f), FRotator(0.f, -90.f, 0.f));
}

ESprintGuardState ASprintGuard::GetGuardState() const
{
    const ASprintGuardController* GuardController = Cast<ASprintGuardController>(GetController());
    return GuardController ? GuardController->GetGuardState() : ESprintGuardState::Patrol;
}

ASprintGuardController::ASprintGuardController()
{
    UAIPerceptionComponent* Hearing = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("Perception"));
    SetPerceptionComponent(*Hearing);

    UAISenseConfig_Hearing* HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("Hearing"));
    HearingConfig->HearingRange = ASprintCharacter::SprintHearingRange;
    HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
    HearingConfig->DetectionByAffiliation.bDetectFriendlies = true;
    HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
    Hearing->ConfigureSense(*HearingConfig);
    Hearing->OnTargetPerceptionUpdated.AddDynamic(this, &ASprintGuardController::OnNoisePerceived);
}

void ASprintGuardController::OnPossess(APawn* InPawn)
{
    Super::OnPossess(InPawn);
    if (!GuardTree)
    {
        GuardTree = SprintGuardBehavior::CreateTree(this);
    }

    UBlackboardComponent* Board = nullptr;
    if (UseBlackboard(GuardTree->BlackboardAsset, Board))
    {
        Board->SetValueAsVector(SprintGuardBehavior::Home, GetNavAgentLocation());
        if (RunBehaviorTree(GuardTree))
        {
            return;
        }
    }

    UE_LOG(LogSprintDemo, Error, TEXT("Could not start the guard Behavior Tree."));
}

void ASprintGuardController::OnUnPossess()
{
    PendingSprintNoise.Reset();
    if (UBlackboardComponent* Board = GetBlackboardComponent())
    {
        Board->ClearValue(SprintGuardBehavior::Player);
        Board->ClearValue(SprintGuardBehavior::NoiseLocation);
        Board->ClearValue(SprintGuardBehavior::Destination);
    }
    Super::OnUnPossess();
}

void ASprintGuardController::OnNoisePerceived(AActor* Actor, FAIStimulus Stimulus)
{
    if (GetPawn() && IsValid(Actor) && Actor->IsA<ASprintCharacter>() &&
        Stimulus.WasSuccessfullySensed() && Stimulus.Tag == ASprintCharacter::SprintFootstepTag)
    {
        PendingSprintNoise = Stimulus.StimulusLocation;
    }
}

TOptional<FVector> ASprintGuardController::ConsumeSprintNoise()
{
    TOptional<FVector> Noise = PendingSprintNoise;
    PendingSprintNoise.Reset();
    return Noise;
}

ESprintGuardState ASprintGuardController::GetGuardState() const
{
    using namespace SprintGuardBehavior;
    const UBlackboardComponent* Board = GetBlackboardComponent();
    if (Board && IsValid(Board->GetValueAsObject(Player)))
    {
        return ESprintGuardState::Chase;
    }
    return Board && Board->IsVectorValueSet(NoiseLocation) ? ESprintGuardState::Investigate : ESprintGuardState::Patrol;
}

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "Perception/AIPerceptionTypes.h"
#include "SprintGuard.generated.h"

class UBehaviorTree;

enum class ESprintGuardState : uint8
{
    Patrol,
    Investigate,
    Chase
};

UCLASS()
class SPRINTDEMO_API ASprintGuard : public ACharacter
{
    GENERATED_BODY()

public:
    ASprintGuard();
    ESprintGuardState GetGuardState() const;
};

UCLASS()
class SPRINTDEMO_API ASprintGuardController : public AAIController
{
    GENERATED_BODY()

public:
    ASprintGuardController();
    TOptional<FVector> ConsumeSprintNoise();
    ESprintGuardState GetGuardState() const;

protected:
    virtual void OnPossess(APawn* InPawn) override;
    virtual void OnUnPossess() override;

private:
    UFUNCTION()
    void OnNoisePerceived(AActor* Actor, FAIStimulus Stimulus);

    UPROPERTY(Transient)
    TObjectPtr<UBehaviorTree> GuardTree;

    TOptional<FVector> PendingSprintNoise;
};

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BTService.h"
#include "BehaviorTree/BTTaskNode.h"
#include "BehaviorTree/Decorators/BTDecorator_Blackboard.h"
#include "BehaviorTree/Tasks/BTTask_MoveTo.h"
#include "SprintGuardBehavior.generated.h"

namespace SprintGuardBehavior
{
    inline const FName Home(TEXT("Home"));
    inline const FName Player(TEXT("VisiblePlayer"));
    inline const FName NoiseLocation(TEXT("NoiseLocation"));
    inline const FName NoiseExpiresAt(TEXT("NoiseExpiresAt"));
    inline const FName SearchStep(TEXT("SearchStep"));
    inline const FName Destination(TEXT("Destination"));

    constexpr float PatrolSpeed = 270.f;
    constexpr float InvestigateSpeed = 440.f;
    constexpr float ChaseSpeed = 700.f;
    constexpr float SightRadius = 1500.f;
    constexpr float LoseSightRadius = 1950.f;
    constexpr float PatrolRadius = 1400.f;

    UBehaviorTree* CreateTree(UObject* Outer);
}

UCLASS()
class UBTService_SprintGuardSenses : public UBTService
{
    GENERATED_BODY()

public:
    UBTService_SprintGuardSenses();

protected:
    virtual void TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds) override;
};

UCLASS()
class UBTTask_SprintGuardDestination : public UBTTaskNode
{
    GENERATED_BODY()

public:
    void Configure(bool bInInvestigate);

protected:
    virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

private:
    UPROPERTY()
    bool bInvestigate = false;
};

// Expose configuration for composing the standard UE nodes in C++.
UCLASS()
class UBTDecorator_SprintGuardKeySet : public UBTDecorator_Blackboard
{
    GENERATED_BODY()

public:
    void Configure(FName KeyName);
};

UCLASS()
class UBTTask_SprintGuardMoveTo : public UBTTask_MoveTo
{
    GENERATED_BODY()

public:
    void Configure(FName KeyName, float Radius, bool bFollowActor);
};

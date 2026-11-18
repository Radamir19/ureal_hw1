#include "SprintGuardBehavior.h"
#include "SprintCharacter.h"
#include "SprintGuard.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Float.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Vector.h"
#include "BehaviorTree/Composites/BTComposite_Selector.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "BehaviorTree/Tasks/BTTask_Wait.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationData.h"
#include "NavigationSystem.h"

namespace
{
    constexpr float InvestigationSeconds = 12.f;
    constexpr float NewNoiseDistance = 320.f;
    constexpr int32 InvestigationPoints = 4;

    UBTComposite_Sequence* AddBranch(UBehaviorTree* Tree, UBTComposite_Selector* Root, FName RequiredKey = NAME_None)
    {
        FBTCompositeChild& Child = Root->Children.AddDefaulted_GetRef();
        UBTComposite_Sequence* Sequence = NewObject<UBTComposite_Sequence>(Tree);
        Child.ChildComposite = Sequence;
        if (!RequiredKey.IsNone())
        {
            UBTDecorator_SprintGuardKeySet* Decorator = NewObject<UBTDecorator_SprintGuardKeySet>(Tree);
            Decorator->Configure(RequiredKey);
            Child.Decorators.Add(Decorator);
        }
        return Sequence;
    }

    void AddWait(UBehaviorTree* Tree, UBTCompositeNode* Parent, float Seconds)
    {
        UBTTask_Wait* Wait = NewObject<UBTTask_Wait>(Tree);
        Wait->WaitTime = FValueOrBBKey_Float(Seconds);
        Parent->Children.AddDefaulted_GetRef().ChildTask = Wait;
    }

    void AddMoveAndWait(UBehaviorTree* Tree, UBTComposite_Sequence* Sequence, FName KeyName, bool bFollowActor)
    {
        UBTTask_SprintGuardMoveTo* Move = NewObject<UBTTask_SprintGuardMoveTo>(Tree);
        Move->Configure(KeyName, bFollowActor ? 90.f : 65.f, bFollowActor);
        Sequence->Children.AddDefaulted_GetRef().ChildTask = Move;
        AddWait(Tree, Sequence, bFollowActor ? 0.2f : 0.55f);
    }
}

UBehaviorTree* SprintGuardBehavior::CreateTree(UObject* Outer)
{
    UBehaviorTree* Tree = NewObject<UBehaviorTree>(Outer, TEXT("BT_SprintGuard"), RF_Transient);
    UBlackboardData* Board = NewObject<UBlackboardData>(Tree, TEXT("BB_SprintGuard"));
    Board->UpdatePersistentKey<UBlackboardKeyType_Vector>(Home);
    Board->UpdatePersistentKey<UBlackboardKeyType_Object>(Player)->BaseClass = ASprintCharacter::StaticClass();
    Board->UpdatePersistentKey<UBlackboardKeyType_Vector>(NoiseLocation);
    Board->UpdatePersistentKey<UBlackboardKeyType_Float>(NoiseExpiresAt);
    Board->UpdatePersistentKey<UBlackboardKeyType_Int>(SearchStep);
    Board->UpdatePersistentKey<UBlackboardKeyType_Vector>(Destination);
    Tree->BlackboardAsset = Board;

    UBTComposite_Selector* Root = NewObject<UBTComposite_Selector>(Tree);
    Tree->RootNode = Root;
    Root->Services.Add(NewObject<UBTService_SprintGuardSenses>(Tree));

    AddMoveAndWait(Tree, AddBranch(Tree, Root, Player), Player, true);

    UBTComposite_Sequence* Investigate = AddBranch(Tree, Root, NoiseLocation);
    UBTTask_SprintGuardDestination* SearchPoint = NewObject<UBTTask_SprintGuardDestination>(Tree);
    SearchPoint->Configure(true);
    Investigate->Children.AddDefaulted_GetRef().ChildTask = SearchPoint;
    AddMoveAndWait(Tree, Investigate, Destination, false);

    UBTComposite_Sequence* Patrol = AddBranch(Tree, Root);
    UBTTask_SprintGuardDestination* PatrolPoint = NewObject<UBTTask_SprintGuardDestination>(Tree);
    PatrolPoint->Configure(false);
    Patrol->Children.AddDefaulted_GetRef().ChildTask = PatrolPoint;
    AddMoveAndWait(Tree, Patrol, Destination, false);

    AddWait(Tree, Root, 0.35f);
    return Tree;
}

void UBTDecorator_SprintGuardKeySet::Configure(FName KeyName)
{
    BlackboardKey.SelectedKeyName = KeyName;
    OperationType = EBasicKeyOperation::Set;
    NotifyObserver = KeyName == SprintGuardBehavior::NoiseLocation
        ? EBTBlackboardRestart::ValueChange : EBTBlackboardRestart::ResultChange;
    FlowAbortMode = EBTFlowAbortMode::Both;
}

void UBTTask_SprintGuardMoveTo::Configure(FName KeyName, float Radius, bool bFollowActor)
{
    BlackboardKey.SelectedKeyName = KeyName;
    AcceptableRadius = FValueOrBBKey_Float(Radius);
    bAllowPartialPath = static_cast<decltype(bAllowPartialPath)>(false);
    bTrackMovingGoal = static_cast<decltype(bTrackMovingGoal)>(bFollowActor);
}

UBTService_SprintGuardSenses::UBTService_SprintGuardSenses()
{
    NodeName = TEXT("Check sight and sprint noise");
    Interval = 0.15f;
    RandomDeviation = 0.f;
}

void UBTService_SprintGuardSenses::TickNode(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, float DeltaSeconds)
{
    Super::TickNode(OwnerComp, NodeMemory, DeltaSeconds);
    using namespace SprintGuardBehavior;

    ASprintGuardController* Controller = Cast<ASprintGuardController>(OwnerComp.GetAIOwner());
    ASprintGuard* Guard = Controller ? Cast<ASprintGuard>(Controller->GetPawn()) : nullptr;
    UBlackboardComponent* Board = OwnerComp.GetBlackboardComponent();
    if (!Guard || !Board)
    {
        return;
    }

    ASprintCharacter* Character = Cast<ASprintCharacter>(UGameplayStatics::GetPlayerPawn(Guard, 0));
    const bool bAlreadyChasing = Character && Board->GetValueAsObject(Player) == Character;
    const float Radius = bAlreadyChasing ? LoseSightRadius : SightRadius;
    const bool bVisible = IsValid(Character) &&
        FVector::DistSquared(Guard->GetActorLocation(), Character->GetActorLocation()) <= FMath::Square(Radius) &&
        Controller->LineOfSightTo(Character);

    const TOptional<FVector> Noise = Controller->ConsumeSprintNoise();
    UCharacterMovementComponent* Movement = Guard->GetCharacterMovement();
    if (bVisible)
    {
        Movement->MaxWalkSpeed = ChaseSpeed;
        Board->SetValueAsObject(Player, Character);
        Board->ClearValue(NoiseLocation);
        return;
    }

    const float Now = Guard->GetWorld()->GetTimeSeconds();
    const bool bActiveSearch = Board->IsVectorValueSet(NoiseLocation) && Now < Board->GetValueAsFloat(NoiseExpiresAt);
    if (Noise.IsSet() && (!bActiveSearch ||
        FVector::DistSquared2D(Noise.GetValue(), Board->GetValueAsVector(NoiseLocation)) >= FMath::Square(NewNoiseDistance)))
    {
        Board->SetValueAsInt(SearchStep, 0);
        Board->SetValueAsFloat(NoiseExpiresAt, Now + InvestigationSeconds);
        Board->SetValueAsVector(NoiseLocation, Noise.GetValue());
    }
    else if (!bActiveSearch)
    {
        Board->ClearValue(NoiseLocation);
    }

    Movement->MaxWalkSpeed = Board->IsVectorValueSet(NoiseLocation) ? InvestigateSpeed : PatrolSpeed;
    Board->ClearValue(Player);
}

void UBTTask_SprintGuardDestination::Configure(bool bInInvestigate)
{
    bInvestigate = bInInvestigate;
    NodeName = bInvestigate ? TEXT("Choose next point around the noise") : TEXT("Choose reachable patrol point");
}

EBTNodeResult::Type UBTTask_SprintGuardDestination::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
    using namespace SprintGuardBehavior;
    AAIController* Controller = OwnerComp.GetAIOwner();
    UBlackboardComponent* Board = OwnerComp.GetBlackboardComponent();
    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(OwnerComp.GetWorld());
    if (!Controller || !Controller->GetPawn() || !Board || !Nav)
    {
        return EBTNodeResult::Failed;
    }

    const FVector Start = Controller->GetNavAgentLocation();
    ANavigationData* NavData = Nav->GetNavDataForProps(Controller->GetNavAgentPropertiesRef(), Start);
    const FName AnchorKey = bInvestigate ? NoiseLocation : Home;
    if (!NavData || !Board->IsVectorValueSet(AnchorKey))
    {
        return EBTNodeResult::Failed;
    }

    const int32 Step = bInvestigate ? Board->GetValueAsInt(SearchStep) : 0;
    if (bInvestigate && Step >= InvestigationPoints)
    {
        Board->ClearValue(NoiseLocation);
        return EBTNodeResult::Failed;
    }

    FNavLocation Anchor;
    if (!Nav->ProjectPointToNavigation(Board->GetValueAsVector(AnchorKey), Anchor,
        FVector(250.f, 250.f, 120.f), NavData))
    {
        if (bInvestigate)
        {
            Board->ClearValue(NoiseLocation);
        }
        return EBTNodeResult::Failed;
    }

    // The first move goes to the sound itself. Subsequent moves sample larger
    // rings around it and reject candidates that would require a partial path.
    const float Radius = bInvestigate ? 300.f + 220.f * Step : PatrolRadius;
    const bool bHasPrevious = Board->IsVectorValueSet(Destination);
    const FVector Previous = Board->GetValueAsVector(Destination);
    for (int32 Attempt = 0; Attempt < 10; ++Attempt)
    {
        const bool bCheckAnchor = bInvestigate && Step == 0 && Attempt == 0;
        FNavLocation Candidate = Anchor;
        if (!bCheckAnchor &&
            (!Nav->GetRandomReachablePointInRadius(Anchor.Location, Radius, Candidate, NavData) ||
                FVector::DistSquared2D(Start, Candidate.Location) < FMath::Square(160.f) ||
                FVector::DistSquared2D(Anchor.Location, Candidate.Location) > FMath::Square(Radius) ||
                (bHasPrevious && FVector::DistSquared2D(Previous, Candidate.Location) < FMath::Square(200.f)) ||
                (bInvestigate && FVector::DistSquared2D(Anchor.Location, Candidate.Location) < FMath::Square(Radius * 0.4f))))
        {
            continue;
        }

        FPathFindingQuery Path(Controller, *NavData, Start, Candidate.Location);
        Path.SetAllowPartialPaths(false);
        if (Nav->TestPathSync(Path))
        {
            Board->SetValueAsVector(Destination, Candidate.Location);
            if (bInvestigate)
            {
                Board->SetValueAsInt(SearchStep, Step + 1);
            }
            return EBTNodeResult::Succeeded;
        }
    }

    if (bInvestigate)
    {
        Board->ClearValue(NoiseLocation);
    }
    return EBTNodeResult::Failed;
}

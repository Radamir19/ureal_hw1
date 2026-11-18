#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "SprintArena.h"
#include "SprintCharacter.h"
#include "SprintDemoGameMode.h"
#include "SprintGuard.h"
#include "SprintGuardBehavior.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationData.h"
#include "NavigationSystem.h"
#include "Tests/AutomationEditorCommon.h"

namespace
{
    void SendKey(APlayerController& Controller, FKey Key, bool bPressed)
    {
        Controller.InputKey(FInputKeyEventArgs::CreateSimulated(Key,
            bPressed ? IE_Pressed : IE_Released, bPressed ? 1.f : 0.f));
    }

    void PlaceCharacter(ACharacter& Character, const FVector& Location)
    {
        Character.GetCharacterMovement()->StopMovementImmediately();
        Character.SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
    }

    bool HasPathAroundProp(UWorld& World, const UInstancedStaticMeshComponent& Props)
    {
        UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(&World);
        const ANavigationData* NavData = Nav ? Nav->GetDefaultNavDataInstance(FNavigationSystem::DontCreate) : nullptr;
        if (!NavData)
        {
            return false;
        }

        for (int32 Index = 0; Index < Props.GetInstanceCount(); ++Index)
        {
            FTransform Instance;
            if (!Props.GetInstanceTransform(Index, Instance, true))
            {
                continue;
            }

            const FVector Left = Instance.GetLocation() - FVector(185, 0, 0);
            const FVector Right = Instance.GetLocation() + FVector(185, 0, 0);
            FHitResult Hit;
            FNavLocation From, To;
            const FVector Projection(45, 45, 130);
            if (World.LineTraceSingleByChannel(Hit, Left, Right, ECC_Visibility) && Hit.GetComponent() == &Props &&
                Nav->ProjectPointToNavigation(Left, From, Projection, NavData) &&
                Nav->ProjectPointToNavigation(Right, To, Projection, NavData))
            {
                FPathFindingQuery Request(&Props, *NavData, From.Location, To.Location);
                Request.SetAllowPartialPaths(false);
                const FPathFindingResult Result = Nav->FindPathSync(Request);
                if (Result.IsSuccessful() && Result.Path.IsValid() && Result.Path->GetPathPoints().Num() > 2)
                {
                    return true;
                }
            }
        }
        return false;
    }

    class FExerciseSprintArena final : public IAutomationLatentCommand
    {
    public:
        explicit FExerciseSprintArena(FAutomationTestBase& InTest) : Test(InTest) {}

        bool Update() override
        {
            using namespace SprintGuardBehavior;
            if (StartedAt == 0.0)
            {
                StartedAt = FPlatformTime::Seconds();
            }
            if (FPlatformTime::Seconds() - StartedAt > 75.0)
            {
                Test.AddError(FString::Printf(TEXT("Arena integration test timed out at stage %d."), static_cast<int32>(Stage)));
                return true;
            }

            UWorld* World = GEditor ? GEditor->PlayWorld : nullptr;
            ASprintDemoGameMode* Mode = World ? World->GetAuthGameMode<ASprintDemoGameMode>() : nullptr;
            ASprintArena* Arena = Mode ? Mode->GetSprintArena() : nullptr;
            ASprintGuard* Guard = Arena ? Arena->GetGuard() : nullptr;
            ASprintGuardController* AI = Guard ? Cast<ASprintGuardController>(Guard->GetController()) : nullptr;
            const UBlackboardComponent* Board = AI ? AI->GetBlackboardComponent() : nullptr;
            APlayerController* Controller = World ? World->GetFirstPlayerController() : nullptr;
            ASprintCharacter* Character = Controller ? Cast<ASprintCharacter>(Controller->GetPawn()) : nullptr;
            if (!Board || !Character || !Controller->PlayerInput || !Character->HasActorBegunPlay())
            {
                return false;
            }

            const FVector Origin = Arena->GetActorLocation();
            const FVector QuietSpot = Origin + FVector(-2470, -1440, 102);
            const ESprintGuardState State = Guard->GetGuardState();
            switch (Stage)
            {
            case EStage::Setup:
            {
                int32 Instances = 0;
                bool bDetour = false;
                const TInlineComponentArray<UInstancedStaticMeshComponent*> Meshes(Arena);
                for (const UInstancedStaticMeshComponent* Mesh : Meshes)
                {
                    Instances += Mesh->GetInstanceCount();
                    bDetour = bDetour || HasPathAroundProp(*World, *Mesh);
                }
                bool bVolumeContainsArena = false;
                for (TActorIterator<ANavMeshBoundsVolume> It(World); It; ++It)
                {
                    bVolumeContainsArena |= It->GetComponentsBoundingBox(true).IsInside(Origin);
                }
                Test.TestEqual(TEXT("PCG spawned all 40 arena instances"), Instances, 40);
                Test.TestTrue(TEXT("NavMeshBoundsVolume covers the arena"), bVolumeContainsArena);
                Test.TestTrue(TEXT("Navigation finds a complete detour around a collidable PCG instance"), bDetour);
                Test.TestTrue(TEXT("Play starts the original player in the extended level"),
                    FVector::Dist2D(Character->GetActorLocation(), Origin) < 2800);
                PlaceCharacter(*Character, QuietSpot);
                Checkpoint = Guard->GetActorLocation();
                Advance(*World, EStage::Patrol);
                break;
            }
            case EStage::Patrol:
                if (FVector::Dist2D(Guard->GetActorLocation(), Checkpoint) < 150) return false;
                Test.TestTrue(TEXT("Guard moves while patrolling"), State == ESprintGuardState::Patrol);
                Test.TestTrue(TEXT("Patrol destination is within the home radius"),
                    Board->IsVectorValueSet(Destination) &&
                    FVector::Dist2D(Board->GetValueAsVector(Home), Board->GetValueAsVector(Destination)) <= PatrolRadius + 1.f);
                AI->StopMovement();
                PlaceCharacter(*Guard, Origin + FVector(300, 0, 102));
                PlaceCharacter(*Character, Origin + FVector(-1050, 0, 102));
                Controller->SetControlRotation(FRotator::ZeroRotator);
                SendKey(*Controller, EKeys::LeftShift, true);
                Advance(*World, EStage::SilentSprint);
                break;

            case EStage::SilentSprint:
                if (World->GetTimeSeconds() - StageStartedAt < 0.35) return false;
                Test.TestTrue(TEXT("Held Shift alone is silent"), Character->bIsSprinting && State == ESprintGuardState::Patrol);
                Test.TestFalse(TEXT("Cover blocks sight"), AI->LineOfSightTo(Character));
                SendKey(*Controller, EKeys::W, true);
                Advance(*World, EStage::HearSprint);
                break;

            case EStage::HearSprint:
                if (State != ESprintGuardState::Investigate) return false;
                Test.TestFalse(TEXT("Sprint is heard with no line of sight"), AI->LineOfSightTo(Character));
                Test.TestTrue(TEXT("The remembered noise position comes from the real input-driven sprint"),
                    FVector::Dist2D(Board->GetValueAsVector(NoiseLocation), Character->GetActorLocation()) < 250);
                SendKey(*Controller, EKeys::W, false);
                SendKey(*Controller, EKeys::LeftShift, false);
                PlaceCharacter(*Character, QuietSpot);
                Advance(*World, EStage::Sweep);
                break;

            case EStage::Sweep:
                if (State != ESprintGuardState::Investigate || Board->GetValueAsInt(SearchStep) < 2) return false;
                Test.TestTrue(TEXT("After reaching the noise the tree selects another search point"),
                    FVector::Dist2D(Board->GetValueAsVector(Destination), Board->GetValueAsVector(NoiseLocation)) >= 200);
                Checkpoint = Guard->GetActorLocation();
                PlaceCharacter(*Character, FVector(Checkpoint.X - 450, 0, Origin.Z + 102));
                Advance(*World, EStage::Chase);
                break;

            case EStage::Chase:
                if (State != ESprintGuardState::Chase || FVector::Dist2D(Guard->GetActorLocation(), Checkpoint) < 70) return false;
                Test.TestTrue(TEXT("Sight interrupts search and pursues the player at run speed"),
                    Board->GetValueAsObject(Player) == Character && Guard->GetCharacterMovement()->MaxWalkSpeed == ChaseSpeed);
                Test.TestFalse(TEXT("Chase discards the previously heard noise"), Board->IsVectorValueSet(NoiseLocation));
                PlaceCharacter(*Character, Origin + FVector(2470, 0, 102));
                Advance(*World, EStage::Escape);
                break;

            case EStage::Escape:
                if (State != ESprintGuardState::Patrol || AI->GetMoveStatus() != EPathFollowingStatus::Moving) return false;
                Test.TestNull(TEXT("Out of sight and hearing range, the guard resumes its patrol"), Board->GetValueAsObject(Player));
                return true;
            }
            return false;
        }

    private:
        enum class EStage { Setup, Patrol, SilentSprint, HearSprint, Sweep, Chase, Escape };

        void Advance(UWorld& World, EStage Next)
        {
            Stage = Next;
            StageStartedAt = World.GetTimeSeconds();
        }

        FAutomationTestBase& Test;
        double StartedAt = 0.0;
        EStage Stage = EStage::Setup;
        double StageStartedAt = 0;
        FVector Checkpoint = FVector::ZeroVector;
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSprintArenaIntegrationTest, "SprintDemo.Integration.PCGGuardBehavior",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FSprintArenaIntegrationTest::RunTest(const FString& Parameters)
{
    if (!GEditor || GEditor->PlayWorld)
    {
        AddError(TEXT("Run the arena test from the editor with no existing PIE session."));
        return false;
    }
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/ThirdPerson/Lvl_ThirdPerson")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FExerciseSprintArena(*this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif

#include "SprintArena.h"
#include "SprintArenaPCG.h"
#include "SprintDemo.h"
#include "SprintGuard.h"
#include "AI/NavigationSystemBase.h"
#include "Components/BoxComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Elements/PCGStaticMeshSpawner.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Materials/MaterialInterface.h"
#include "MeshSelectors/PCGMeshSelectorWeighted.h"
#include "NavMesh/NavMeshBoundsVolume.h"
#include "NavigationSystem.h"
#include "PCGComponent.h"
#include "PCGGraph.h"
#include "PCGPin.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
    constexpr double ArenaHalfLength = 2800;
    constexpr double ArenaHalfWidth = 1700;
    const FVector StartOffset(-2240, 0, 102);
    const FVector GuardOffset(300, 0, 0);
}

ASprintArena::ASprintArena()
{
    USceneComponent* Scene = CreateDefaultSubobject<USceneComponent>(TEXT("ArenaOrigin"));
    Scene->SetMobility(EComponentMobility::Static);
    SetRootComponent(Scene);

    static const ConstructorHelpers::FObjectFinder<UStaticMesh> CubeAsset(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static const ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderAsset(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static const ConstructorHelpers::FObjectFinder<UMaterialInterface> FloorAsset(TEXT(
        "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_Gray.MI_PrototypeGrid_Gray"));
    static const ConstructorHelpers::FObjectFinder<UMaterialInterface> WallAsset(TEXT(
        "/Game/LevelPrototyping/Materials/MI_PrototypeGrid_TopDark.MI_PrototypeGrid_TopDark"));
    static const ConstructorHelpers::FObjectFinder<UMaterialInterface> PropAsset(TEXT(
        "/Game/LevelPrototyping/Materials/MI_DefaultColorway.MI_DefaultColorway"));
    Cube = CubeAsset.Object;
    Cylinder = CylinderAsset.Object;
    PropMaterial = PropAsset.Object;

    CreateBlock(TEXT("Floor"), FloorAsset.Object, FVector(0, 0, -35), FVector(ArenaHalfLength, ArenaHalfWidth, 35));
    CreateBlock(TEXT("NorthWall"), WallAsset.Object, FVector(0, ArenaHalfWidth - 35, 155), FVector(ArenaHalfLength, 35, 155));
    CreateBlock(TEXT("SouthWall"), WallAsset.Object, FVector(0, -ArenaHalfWidth + 35, 155), FVector(ArenaHalfLength, 35, 155));
    CreateBlock(TEXT("EastWall"), WallAsset.Object, FVector(ArenaHalfLength - 35, 0, 155), FVector(35, ArenaHalfWidth - 70, 155));
    CreateBlock(TEXT("WestWall"), WallAsset.Object, FVector(-ArenaHalfLength + 35, 0, 155), FVector(35, ArenaHalfWidth - 70, 155));
    CreateBlock(TEXT("NoiseCover"), WallAsset.Object, FVector(-520, 0, 160), FVector(70, 510, 160));
    CreateBlock(TEXT("FarCover"), WallAsset.Object, FVector(1190, -105, 140), FVector(85, 325, 140));

    PCG = CreateDefaultSubobject<UPCGComponent>(TEXT("ArenaPCG"));
    PCG->bIsComponentPartitioned = false;
    PCG->GenerationTrigger = EPCGComponentGenerationTrigger::GenerateOnDemand;
}

void ASprintArena::CreateBlock(FName Name, UMaterialInterface* Material, const FVector& Center, const FVector& HalfExtent)
{
    UStaticMeshComponent* Block = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Block->SetupAttachment(RootComponent);
    Block->SetMobility(EComponentMobility::Static);
    Block->SetStaticMesh(Cube);
    Block->SetMaterial(0, Material);
    Block->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
    Block->SetRelativeLocation(Center);
    Block->SetRelativeScale3D(HalfExtent / 50.0);
}

void ASprintArena::PrepareForPlay()
{
    // GameMode calls this before it chooses a player start and before the
    // world's initial navigation build can gather the bounds.
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    UWorld* World = GetWorld();
    ANavMeshBoundsVolume* Volume = World->SpawnActor<ANavMeshBoundsVolume>(GetActorLocation(), FRotator::ZeroRotator, Params);
    if (Volume)
    {
        // Recast reads the volume's registered component bounds. Give it a
        // box without relying on an editor-only brush builder.
        UBoxComponent* Bounds = NewObject<UBoxComponent>(Volume, TEXT("NavigationBounds"));
        Bounds->SetupAttachment(Volume->GetRootComponent());
        Bounds->InitBoxExtent(FVector(ArenaHalfLength, ArenaHalfWidth, 450));
        Bounds->SetMobility(EComponentMobility::Static);
        Bounds->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
        Bounds->SetCanEverAffectNavigation(false);
        Volume->AddInstanceComponent(Bounds);
        Bounds->RegisterComponent();
        if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
        {
            Nav->OnNavigationBoundsUpdated(Volume);
        }
    }
    PlayerStart = World->SpawnActor<APlayerStart>(GetActorLocation() + StartOffset, FRotator::ZeroRotator, Params);
}

void ASprintArena::BeginPlay()
{
    Super::BeginPlay();
    GenerateObstacles();
    StartupDeadline = GetWorld()->GetTimeSeconds() + 30.f;
    GetWorldTimerManager().SetTimer(StartupTimer, this, &ASprintArena::TrySpawnGuard, 0.2f, true);
}

void ASprintArena::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    GetWorldTimerManager().ClearTimer(StartupTimer);
    Super::EndPlay(EndPlayReason);
}

void ASprintArena::GenerateObstacles()
{
    UPCGGraph* Graph = NewObject<UPCGGraph>(this, TEXT("PCG_SprintArena"), RF_Transient);

    USprintArenaPointsSettings* PointsSettings = nullptr;
    UPCGNode* Points = Graph->AddNodeOfType(PointsSettings);
    UPCGStaticMeshSpawnerSettings* SpawnSettings = nullptr;
    UPCGNode* Spawner = Graph->AddNodeOfType(SpawnSettings);
    SpawnSettings->SetMeshSelectorType(UPCGMeshSelectorWeighted::StaticClass());
    UPCGMeshSelectorWeighted* Selector = CastChecked<UPCGMeshSelectorWeighted>(SpawnSettings->MeshSelectorParameters);
    for (UStaticMesh* Mesh : {Cube.Get(), Cylinder.Get()})
    {
        FPCGMeshSelectorWeightedEntry& Entry = Selector->MeshEntries.AddDefaulted_GetRef();
        Entry.Descriptor.StaticMesh = Mesh;
        Entry.Descriptor.OverrideMaterials.Emplace(PropMaterial.Get());
        Entry.Descriptor.Mobility = EComponentMobility::Static;
    }

    Graph->AddEdge(Points, PCGPinConstants::DefaultOutputLabel, Spawner, PCGPinConstants::DefaultInputLabel);
    Graph->AddEdge(Spawner, PCGPinConstants::DefaultOutputLabel, Graph->GetOutputNode(), PCGPinConstants::DefaultOutputLabel);
    PCG->Seed = PointsSettings->LayoutSeed;
    PCG->SetGraphLocal(Graph);
    PCG->GenerateLocal(true);
}

void ASprintArena::TrySpawnGuard()
{
    if (GetWorld()->GetTimeSeconds() >= StartupDeadline)
    {
        UE_LOG(LogSprintDemo, Error, TEXT("Arena startup timed out while waiting for %s."),
            bObstaclesRegistered ? TEXT("Recast navigation") : TEXT("PCG obstacles"));
        GetWorldTimerManager().ClearTimer(StartupTimer);
        return;
    }

    if (!bObstaclesRegistered)
    {
        if (PCG->IsGenerating())
        {
            return;
        }

        const TInlineComponentArray<UInstancedStaticMeshComponent*> Obstacles(this);
        // Generation adds instances asynchronously. Update Recast with the
        // resulting components before starting a tree that issues Move To.
        for (UInstancedStaticMeshComponent* Obstacle : Obstacles)
        {
            if (Obstacle->GetInstanceCount() > 0)
            {
                Obstacle->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
                Obstacle->SetCanEverAffectNavigation(true);
                FNavigationSystem::UpdateComponentData(*Obstacle);
                bObstaclesRegistered = true;
            }
        }
        return;
    }

    UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
    FNavLocation Spawn;
    if (!Nav || UNavigationSystemV1::IsNavigationBeingBuilt(this) ||
        !Nav->ProjectPointToNavigation(GetActorLocation() + GuardOffset, Spawn, FVector(100, 100, 100)))
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(StartupTimer);
    Guard = GetWorld()->SpawnActor<ASprintGuard>(Spawn.Location + FVector(0, 0, 100), FRotator(0, 180, 0));
    if (!Guard)
    {
        UE_LOG(LogSprintDemo, Error, TEXT("Could not spawn the guard on the arena's NavMesh."));
    }
}

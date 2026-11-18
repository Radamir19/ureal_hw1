#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "SprintArena.generated.h"

class APlayerStart;
class ASprintGuard;
class UMaterialInterface;
class UPCGComponent;
class UStaticMesh;

UCLASS()
class SPRINTDEMO_API ASprintArena : public AActor
{
    GENERATED_BODY()

public:
    ASprintArena();

    void PrepareForPlay();
    APlayerStart* GetArenaPlayerStart() const { return PlayerStart; }
    ASprintGuard* GetGuard() const { return Guard; }

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void CreateBlock(FName Name, UMaterialInterface* Material, const FVector& Center, const FVector& HalfExtent);
    void GenerateObstacles();
    void TrySpawnGuard();

    UPROPERTY(VisibleAnywhere, Category="PCG")
    TObjectPtr<UPCGComponent> PCG;

    UPROPERTY()
    TObjectPtr<UStaticMesh> Cube;

    UPROPERTY()
    TObjectPtr<UStaticMesh> Cylinder;

    UPROPERTY()
    TObjectPtr<UMaterialInterface> PropMaterial;

    UPROPERTY(Transient)
    TObjectPtr<APlayerStart> PlayerStart;

    UPROPERTY(Transient)
    TObjectPtr<ASprintGuard> Guard;

    FTimerHandle StartupTimer;
    bool bObstaclesRegistered = false;
    float StartupDeadline = 0.f;
};

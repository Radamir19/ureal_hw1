#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SprintDemoGameMode.generated.h"

class ASprintArena;

UCLASS()
class SPRINTDEMO_API ASprintDemoGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ASprintDemoGameMode();
    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual AActor* ChoosePlayerStart_Implementation(AController* Player) override;

    ASprintArena* GetSprintArena() const { return Arena; }

private:
    UPROPERTY(Transient)
    TObjectPtr<ASprintArena> Arena;
};

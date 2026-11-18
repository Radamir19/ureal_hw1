#include "SprintDemoGameMode.h"
#include "SprintArena.h"
#include "SprintCharacter.h"
#include "SprintDemoHUD.h"
#include "SprintDemoPlayerController.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Kismet/GameplayStatics.h"

ASprintDemoGameMode::ASprintDemoGameMode()
{
    DefaultPawnClass = ASprintCharacter::StaticClass();
    PlayerControllerClass = ASprintDemoPlayerController::StaticClass();
    HUDClass = ASprintDemoHUD::StaticClass();
}

void ASprintDemoGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    if (UGameplayStatics::GetCurrentLevelName(this) == TEXT("Lvl_ThirdPerson"))
    {
        Arena = GetWorld()->SpawnActor<ASprintArena>(FVector(9600, 0, 0), FRotator::ZeroRotator);
        if (Arena)
        {
            Arena->PrepareForPlay();
        }
    }
}

AActor* ASprintDemoGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
    APlayerStart* Start = Arena ? Arena->GetArenaPlayerStart() : nullptr;
    return IsValid(Start) ? Start : Super::ChoosePlayerStart_Implementation(Player);
}

#include "SprintDemoGameMode.h"
#include "SprintCharacter.h"
#include "SprintDemoHUD.h"
#include "SprintDemoPlayerController.h"

ASprintDemoGameMode::ASprintDemoGameMode()
{
    DefaultPawnClass = ASprintCharacter::StaticClass();
    PlayerControllerClass = ASprintDemoPlayerController::StaticClass();
    HUDClass = ASprintDemoHUD::StaticClass();
}

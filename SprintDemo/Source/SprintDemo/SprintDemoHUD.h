#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SprintDemoHUD.generated.h"

UCLASS()
class SPRINTDEMO_API ASprintDemoHUD : public AHUD
{
    GENERATED_BODY()

public:
    virtual void DrawHUD() override;
};

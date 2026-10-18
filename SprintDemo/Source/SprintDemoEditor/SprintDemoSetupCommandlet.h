#pragma once

#include "Commandlets/Commandlet.h"
#include "SprintDemoSetupCommandlet.generated.h"

/** Rebuilds the assignment's editable assets with Unreal's own graph and package APIs. */
UCLASS()
class USprintDemoSetupCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    USprintDemoSetupCommandlet();
    virtual int32 Main(const FString& Params) override;
};

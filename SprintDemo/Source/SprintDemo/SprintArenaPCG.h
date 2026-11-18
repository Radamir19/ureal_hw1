#pragma once

#include "PCGElement.h"
#include "PCGSettings.h"
#include "SprintArenaPCG.generated.h"

UCLASS(ClassGroup=(Procedural))
class USprintArenaPointsSettings : public UPCGSettings
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, Category="Placement")
    int32 LayoutSeed = 4621;

#if WITH_EDITOR
    virtual FName GetDefaultNodeName() const override { return TEXT("SprintArenaPoints"); }
    virtual FText GetDefaultNodeTitle() const override
    {
        return NSLOCTEXT("SprintDemo", "ArenaPoints", "Obstacle points in the arena");
    }
    virtual EPCGSettingsType GetType() const override { return EPCGSettingsType::Spatial; }
#endif

protected:
    virtual TArray<FPCGPinProperties> InputPinProperties() const override { return {}; }
    virtual TArray<FPCGPinProperties> OutputPinProperties() const override { return DefaultPointOutputPinProperties(); }
    virtual FPCGElementPtr CreateElement() const override;
};

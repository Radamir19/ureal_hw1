#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "SprintDemoAnimInstance.generated.h"

/** Native bridge to the locomotion animation Blueprint. */
UCLASS(Transient, Blueprintable)
class SPRINTDEMO_API USprintDemoAnimInstance : public UAnimInstance
{
    GENERATED_BODY()

public:
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category="Locomotion")
    bool bIsSprinting = false;

    virtual void NativeUpdateAnimation(float DeltaSeconds) override;
};

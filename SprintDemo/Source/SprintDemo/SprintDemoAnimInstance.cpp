#include "SprintDemoAnimInstance.h"
#include "SprintCharacter.h"

void USprintDemoAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const ASprintCharacter* Character = Cast<ASprintCharacter>(TryGetPawnOwner());
    bIsSprinting = Character && Character->bIsSprinting;
}

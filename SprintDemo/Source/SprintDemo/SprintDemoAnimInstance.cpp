#include "SprintDemoAnimInstance.h"
#include "SprintCharacter.h"
#include "SprintGuard.h"
#include "SprintGuardBehavior.h"

void USprintDemoAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
    Super::NativeUpdateAnimation(DeltaSeconds);
    const APawn* Pawn = TryGetPawnOwner();
    if (const ASprintCharacter* Character = Cast<ASprintCharacter>(Pawn))
    {
        bIsSprinting = Character->bIsSprinting;
        return;
    }

    const ASprintGuard* Guard = Cast<ASprintGuard>(Pawn);
    bIsSprinting = Guard && Guard->GetGuardState() == ESprintGuardState::Chase &&
        Guard->GetVelocity().SizeSquared2D() > FMath::Square(SprintGuardBehavior::InvestigateSpeed);
}

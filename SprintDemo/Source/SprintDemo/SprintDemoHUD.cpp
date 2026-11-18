#include "SprintDemoHUD.h"
#include "SprintArena.h"
#include "SprintCharacter.h"
#include "SprintDemoGameMode.h"
#include "SprintGuard.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void ASprintDemoHUD::DrawHUD()
{
    Super::DrawHUD();
    const ASprintCharacter* Character = Cast<ASprintCharacter>(GetOwningPawn());
    if (!Canvas || !Character)
    {
        return;
    }

    const float Scale = FMath::Clamp(Canvas->ClipY / 720.0f, 0.7f, 2.0f);
    const float X = 28.0f * Scale;
    const float Y = Canvas->ClipY - 136.0f * Scale;
    const float Width = 296.0f * Scale;
    const float Height = 16.0f * Scale;
    const float Fraction = FMath::Clamp(Character->Stamina / FMath::Max(1.f, Character->MaxStamina), 0.f, 1.f);
    const FLinearColor FillColor = Fraction < 0.25f
        ? FLinearColor(1.0f, 0.35f, 0.2f)
        : FLinearColor(0.15f, 0.85f, 0.65f);

    DrawRect(FLinearColor(0.025f, 0.035f, 0.05f, 0.88f), X - 12.0f * Scale,
        Y - 12.0f * Scale, Width + 24.0f * Scale, 112.0f * Scale);
    DrawText(FString::Printf(TEXT("STAMINA  %.0f / %.0f%s"), Character->Stamina,
        Character->MaxStamina, Character->bIsSprinting ? TEXT("  |  SPRINT") : TEXT("")),
        FLinearColor::White, X, Y, GEngine->GetSmallFont(), Scale);
    DrawRect(FLinearColor(0.16f, 0.19f, 0.23f), X, Y + 26.0f * Scale, Width, Height);
    DrawRect(FillColor, X + 2.0f * Scale, Y + 28.0f * Scale,
        (Width - 4.0f * Scale) * Fraction, Height - 4.0f * Scale);
    DrawText(TEXT("WASD Move   Mouse Look   Space Jump"), FLinearColor(0.78f, 0.84f, 0.9f),
        X, Y + 52.0f * Scale, GEngine->GetSmallFont(), Scale);
    DrawText(TEXT("Shift Sprint (hold)   E Interact"), FLinearColor(0.78f, 0.84f, 0.9f),
        X, Y + 72.0f * Scale, GEngine->GetSmallFont(), Scale);

    const ASprintDemoGameMode* GameMode = GetWorld()->GetAuthGameMode<ASprintDemoGameMode>();
    const ASprintArena* Arena = GameMode ? GameMode->GetSprintArena() : nullptr;
    const ASprintGuard* Guard = Arena ? Arena->GetGuard() : nullptr;
    if (IsValid(Guard))
    {
        const ESprintGuardState State = Guard->GetGuardState();
        const float Right = Canvas->ClipX - Width - X;
        const TCHAR* Status = TEXT("GUARD   |   PATROL");
        FLinearColor Accent(0.52f, 0.84f, 0.92f);
        if (State == ESprintGuardState::Investigate)
        {
            Status = TEXT("GUARD   |   INVESTIGATING NOISE");
            Accent = FLinearColor(1.f, 0.73f, 0.23f);
        }
        else if (State == ESprintGuardState::Chase)
        {
            Status = TEXT("GUARD   |   CHASE");
            Accent = FLinearColor(1.f, 0.37f, 0.28f);
        }
        DrawRect(FLinearColor(0.025f, 0.035f, 0.05f, 0.88f), Right - 12.f * Scale, 16.f * Scale,
            Width + 24.f * Scale, 58.f * Scale);
        DrawText(Status, Accent, Right, 24.f * Scale, GEngine->GetSmallFont(), Scale);
        DrawText(TEXT("Sprint behind cover to make noise"), FLinearColor(0.78f, 0.84f, 0.9f),
            Right, 48.f * Scale, GEngine->GetSmallFont(), Scale);
    }
}

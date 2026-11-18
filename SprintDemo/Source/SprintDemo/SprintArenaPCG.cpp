#include "SprintArenaPCG.h"
#include "SprintArena.h"
#include "Data/PCGBasePointData.h"
#include "Metadata/PCGMetadata.h"
#include "PCGContext.h"
#include "PCGPin.h"

namespace
{
    class FSprintArenaPoints : public IPCGElement
    {
        // The arena transform is outside the graph's inputs and can change between runs.
        virtual bool IsCacheable(const UPCGSettings* Settings) const override { return false; }
        virtual bool CanExecuteOnlyOnMainThread(FPCGContext* Context) const override { return true; }

        virtual bool ExecuteInternal(FPCGContext* Context) const override
        {
            const USprintArenaPointsSettings* Settings = Context->GetInputSettings<USprintArenaPointsSettings>();
            const ASprintArena* Arena = Cast<ASprintArena>(Context->GetTargetActor(nullptr));
            if (!Settings || !Arena)
            {
                return true;
            }

            // Four zones leave the middle and two flanking lanes clear. One
            // jittered point per cell keeps the props far enough apart for AI.
            constexpr int32 Columns = 5;
            constexpr int32 Rows = 2;
            const FVector2D Zones[] = {{-1450, -1120}, {-1450, 1120}, {1450, -1120}, {1450, 1120}};
            FRandomStream Random(Settings->LayoutSeed);
            const FTransform ArenaTransform = Arena->GetActorTransform();

            UPCGBasePointData* Points = FPCGContext::NewPointData_AnyThread(Context);
            Points->SetNumPoints(UE_ARRAY_COUNT(Zones) * Columns * Rows, false);
            Points->AllocateProperties(EPCGPointNativeProperties::All);
            FPCGPointValueRanges Values(Points, false);

            int32 Index = 0;
            for (const FVector2D& Zone : Zones)
            {
                for (int32 Row = 0; Row < Rows; ++Row)
                {
                    for (int32 Column = 0; Column < Columns; ++Column, ++Index)
                    {
                        const float Width = Random.FRandRange(0.95f, 1.45f);
                        const float Height = Random.FRandRange(1.35f, 2.3f);
                        const double X = Zone.X + (Column - (Columns - 1) * 0.5) * 390 + Random.FRandRange(-85.f, 85.f);
                        const double Y = Zone.Y + (Row - (Rows - 1) * 0.5) * 385 + Random.FRandRange(-65.f, 65.f);
                        const FVector Center(X, Y, 50.f * Height - 1.f);
                        const FRotator Rotation(0.f, Random.FRandRange(-180.f, 180.f), 0.f);
                        Values.TransformRange[Index] = FTransform(Rotation, Center, FVector(Width, Width, Height)) * ArenaTransform;
                        Values.DensityRange[Index] = 1.f;
                        Values.SteepnessRange[Index] = 1.f;
                        Values.BoundsMinRange[Index] = FVector(-50.f);
                        Values.BoundsMaxRange[Index] = FVector(50.f);
                        Values.ColorRange[Index] = FVector4::One();
                        Values.SeedRange[Index] = static_cast<int32>(Random.GetUnsignedInt() & 0x7fffffff);
                        Values.MetadataEntryRange[Index] = PCGInvalidEntryKey;
                    }
                }
            }

            FPCGTaggedData& Output = Context->OutputData.TaggedData.Emplace_GetRef();
            Output.Data = Points;
            Output.Pin = PCGPinConstants::DefaultOutputLabel;
            return true;
        }
    };
}

FPCGElementPtr USprintArenaPointsSettings::CreateElement() const
{
    return MakeShared<FSprintArenaPoints>();
}

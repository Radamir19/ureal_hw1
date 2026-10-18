using UnrealBuildTool;

public class SprintDemoEditor : ModuleRules
{
    public SprintDemoEditor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PrivateDependencyModuleNames.AddRange(new[]
        {
            "Core", "CoreUObject", "Engine", "SprintDemo", "EnhancedInput", "InputCore",
            "UnrealEd", "BlueprintGraph", "Kismet", "KismetCompiler", "AnimGraph",
            "AnimGraphRuntime", "AssetRegistry"
        });
    }
}

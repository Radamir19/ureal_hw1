using UnrealBuildTool;

public class SprintDemo : ModuleRules
{
    public SprintDemo(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new[] {
            "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG"
        });
        PrivateDependencyModuleNames.AddRange(new[] { "Slate", "SlateCore" });
        PublicIncludePaths.Add("SprintDemo");
    }
}

using UnrealBuildTool;

public class SprintDemoEditorTarget : TargetRules
{
    public SprintDemoEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V7;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
        ExtraModuleNames.Add("SprintDemo");
        ExtraModuleNames.Add("SprintDemoEditor");
    }
}

using UnrealBuildTool;

public class WowUnrealTarget : TargetRules
{
    public WowUnrealTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Game;
        DefaultBuildSettings = BuildSettingsVersion.V5;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_5;
        ExtraModuleNames.AddRange(new string[] { "WowUnreal", "WowData", "WowAssets", "WowWorld", "WowUI", "WowNetwork", "WowClient" });
    }
}

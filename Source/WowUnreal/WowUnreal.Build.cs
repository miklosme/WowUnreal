using UnrealBuildTool;

public class WowUnreal : ModuleRules
{
    public WowUnreal(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "UMG", "WowData", "WowAssets", "WowWorld", "WowNetwork", "WowUI", "WowClient" });
    }
}

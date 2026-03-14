using UnrealBuildTool;
public class WowAssets : ModuleRules
{
    public WowAssets(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "RenderCore", "RHI", "WowData", "MeshDescription", "StaticMeshDescription", "SkeletalMeshDescription", "AnimationCore" });
    }
}

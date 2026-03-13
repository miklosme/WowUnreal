using UnrealBuildTool;
public class WowWorld : ModuleRules
{
    public WowWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "WowData", "WowAssets", "MeshDescription", "StaticMeshDescription", "ProceduralMeshComponent" });
    }
}

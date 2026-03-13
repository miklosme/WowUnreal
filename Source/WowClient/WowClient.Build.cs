using UnrealBuildTool;
public class WowClient : ModuleRules
{
    public WowClient(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "WowNetwork", "Json", "JsonUtilities" });
    }
}

using UnrealBuildTool;
public class WowNetwork : ModuleRules
{
    public WowNetwork(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "Sockets", "Networking" });
        AddEngineThirdPartyPrivateStaticDependencies(Target, "OpenSSL");
    }
}

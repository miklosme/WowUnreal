using UnrealBuildTool;
using System.IO;

public class WowData : ModuleRules
{
    public WowData(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });
        string StormLibPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "StormLib", "src");
        if (Directory.Exists(StormLibPath))
        {
            PublicIncludePaths.Add(StormLibPath);
            PublicDefinitions.Add("STORMLIB_NO_AUTO_LINK=1");
            AddEngineThirdPartyPrivateStaticDependencies(Target, "zlib");
        }
    }
}

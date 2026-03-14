using UnrealBuildTool;
using System.IO;

public class WowData : ModuleRules
{
    public WowData(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject" });

        string StormLibPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "StormLib", "src");
        string StormLibBuild = Path.Combine(ModuleDirectory, "..", "ThirdParty", "StormLib", "build");

        if (Directory.Exists(StormLibPath))
        {
            PublicIncludePaths.Add(StormLibPath);
            PublicDefinitions.Add("STORMLIB_NO_AUTO_LINK=1");

            // Link pre-built StormLib static library
            string LibPath = Path.Combine(StormLibBuild, "libstorm.a");
            if (File.Exists(LibPath))
            {
                PublicAdditionalLibraries.Add(LibPath);
                // StormLib depends on zlib and bzip2
                PublicAdditionalLibraries.Add("bz2");
                PublicAdditionalLibraries.Add("z");
            }
        }
    }
}

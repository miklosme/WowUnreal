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

        if (!Directory.Exists(StormLibPath))
        {
            throw new BuildException($"Vendored StormLib source is missing: {StormLibPath}");
        }

        PublicIncludePaths.Add(StormLibPath);
        PublicDefinitions.Add("STORMLIB_NO_AUTO_LINK=1");

        string LibPath = Path.Combine(StormLibBuild, "libstorm.a");
        if (!File.Exists(LibPath))
        {
            throw new BuildException("StormLib is not built. Run Scripts/bootstrap_linux_dependencies.sh from the project root.");
        }

        // The bootstrap compiles StormLib's vendored zlib and bzip2 sources into this archive.
        PublicAdditionalLibraries.Add(LibPath);
    }
}

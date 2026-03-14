using UnrealBuildTool;
using System.IO;
public class WowUI : ModuleRules
{
    public WowUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "WowData", "WowAssets", "WowNetwork", "XmlParser" });
        string LuaPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "lua");
        if (Directory.Exists(LuaPath))
        {
            PublicIncludePaths.Add(LuaPath);
            string LuaLib = Path.Combine(LuaPath, "liblua.a");
            if (File.Exists(LuaLib))
            {
                PublicAdditionalLibraries.Add(LuaLib);
            }
        }
    }
}

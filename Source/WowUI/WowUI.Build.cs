using UnrealBuildTool;
using System.IO;
public class WowUI : ModuleRules
{
    public WowUI(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "UMG", "Slate", "SlateCore", "WowData", "WowAssets", "WowNetwork", "XmlParser", "InputCore" });
        // pugixml for robust XML parsing (WoW FrameXML)
        string PugiPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "pugixml");
        if (Directory.Exists(PugiPath))
        {
            PublicIncludePaths.Add(PugiPath);
            PrivateDefinitions.Add("HAS_PUGIXML=1");
        }

        string LuaPath = Path.Combine(ModuleDirectory, "..", "ThirdParty", "lua");
        if (!Directory.Exists(LuaPath))
        {
            throw new BuildException($"Vendored Lua source is missing: {LuaPath}");
        }

        PublicIncludePaths.Add(LuaPath);
        string LuaLib = Path.Combine(LuaPath, "liblua.a");
        if (!File.Exists(LuaLib))
        {
            throw new BuildException("Lua is not built. Run Scripts/bootstrap_linux_dependencies.sh from the project root.");
        }

        PublicAdditionalLibraries.Add(LuaLib);
    }
}

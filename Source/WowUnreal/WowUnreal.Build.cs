// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class WowUnreal : ModuleRules
{
	public WowUnreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"WowUnreal",
			"WowUnreal/Variant_Platforming",
			"WowUnreal/Variant_Platforming/Animation",
			"WowUnreal/Variant_Combat",
			"WowUnreal/Variant_Combat/AI",
			"WowUnreal/Variant_Combat/Animation",
			"WowUnreal/Variant_Combat/Gameplay",
			"WowUnreal/Variant_Combat/Interfaces",
			"WowUnreal/Variant_Combat/UI",
			"WowUnreal/Variant_SideScrolling",
			"WowUnreal/Variant_SideScrolling/AI",
			"WowUnreal/Variant_SideScrolling/Gameplay",
			"WowUnreal/Variant_SideScrolling/Interfaces",
			"WowUnreal/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}

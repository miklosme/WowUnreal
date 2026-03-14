#pragma once
#include "CoreMinimal.h"

class FMpqManager;
class FWowLuaVM;
class FWowFrameXmlParser;

/** TOC file metadata */
struct FWowTocData
{
	FString Title;
	FString Notes;
	FString Author;
	FString Version;
	int32 Interface = 0;
	TArray<FString> RequiredDeps;
	TArray<FString> OptionalDeps;
	TArray<FString> SavedVariables;
	TArray<FString> SavedVariablesPerCharacter;
	TArray<FString> Files; // .lua and .xml files in load order
	bool bLoadOnDemand = false;
	bool bDisabled = false;
};

class WOWUI_API FWowAddonLoader
{
public:
	/** Parse a .toc file */
	static FWowTocData ParseToc(const TArray<uint8>& Data);

	/** Discover addons in Interface/AddOns/ */
	static TArray<FString> DiscoverAddons(FMpqManager* Mpq);

	/** Load an addon's files (XML and Lua) */
	static bool LoadAddon(const FString& AddonName, FMpqManager* Mpq, FWowLuaVM* LuaVM);
};

#pragma once
#include "CoreMinimal.h"

struct lua_State;
struct FWowTocData;

/**
 * Manages WoW SavedVariables persistence.
 * Serializes/deserializes Lua globals to disk in WoW's standard Lua table format.
 * Storage: Saved/WTF/Account/{Account}/SavedVariables/{AddonName}.lua
 * Per-character: Saved/WTF/Account/{Account}/{Server}/{Character}/SavedVariables/{AddonName}.lua
 */
class WOWUI_API FWowSavedVariables
{
public:
	/** Set account/server/character for path resolution */
	static void SetIdentity(const FString& Account, const FString& Server = TEXT(""), const FString& Character = TEXT(""));

	/** Load saved variables for an addon before its code runs */
	static void LoadForAddon(lua_State* L, const FString& AddonName, const FWowTocData& Toc);

	/** Save variables for an addon (called on shutdown/logout) */
	static void SaveForAddon(lua_State* L, const FString& AddonName, const FWowTocData& Toc);

	/** Save all registered addon variables */
	static void SaveAll(lua_State* L);

	/** Register an addon's TOC so we can save on shutdown */
	static void RegisterAddon(const FString& AddonName, const FWowTocData& Toc);

private:
	/** Serialize a Lua value at the given stack index to a string */
	static void SerializeLuaValue(lua_State* L, int Idx, FString& Out, int Indent);

	/** Write indentation */
	static void AppendIndent(FString& Out, int Indent);

	/** Get the base path for account-wide saved variables */
	static FString GetAccountSVPath();

	/** Get the base path for per-character saved variables */
	static FString GetCharacterSVPath();

	static FString AccountName;
	static FString ServerName;
	static FString CharacterName;

	/** Registered addon TOCs for SaveAll */
	static TMap<FString, FWowTocData> RegisteredAddons;
};

#include "WowSavedVariables.h"
#include "WowAddonLoader.h"
#include "WowLuaVM.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"

#if __has_include("lua.h")
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}
#define HAS_LUA 1
#else
#define HAS_LUA 0
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWowSV, Log, All);

FString FWowSavedVariables::AccountName = TEXT("WowTestUser");
FString FWowSavedVariables::ServerName = TEXT("WowUnreal");
FString FWowSavedVariables::CharacterName = TEXT("Player");
TMap<FString, FWowTocData> FWowSavedVariables::RegisteredAddons;

void FWowSavedVariables::SetIdentity(const FString& Account, const FString& Server, const FString& Character)
{
	AccountName = Account.IsEmpty() ? TEXT("WowTestUser") : Account;
	ServerName = Server.IsEmpty() ? TEXT("WowUnreal") : Server;
	CharacterName = Character.IsEmpty() ? TEXT("Player") : Character;
}

FString FWowSavedVariables::GetAccountSVPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WTF"), TEXT("Account"), AccountName, TEXT("SavedVariables"));
}

FString FWowSavedVariables::GetCharacterSVPath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("WTF"), TEXT("Account"), AccountName, ServerName, CharacterName, TEXT("SavedVariables"));
}

void FWowSavedVariables::RegisterAddon(const FString& AddonName, const FWowTocData& Toc)
{
	if (Toc.SavedVariables.Num() > 0 || Toc.SavedVariablesPerCharacter.Num() > 0)
	{
		RegisteredAddons.Add(AddonName, Toc);
	}
}

void FWowSavedVariables::LoadForAddon(lua_State* L, const FString& AddonName, const FWowTocData& Toc)
{
#if HAS_LUA
	if (!L) return;

	// Load account-wide saved variables
	if (Toc.SavedVariables.Num() > 0)
	{
		FString FilePath = FPaths::Combine(GetAccountSVPath(), AddonName + TEXT(".lua"));
		if (FPaths::FileExists(FilePath))
		{
			TArray<uint8> Data;
			if (FFileHelper::LoadFileToArray(Data, *FilePath))
			{
				FTCHARToUTF8 ChunkName(*FString::Printf(TEXT("@SavedVariables/%s"), *AddonName));
				if (luaL_loadbuffer(L, (const char*)Data.GetData(), Data.Num(), ChunkName.Get()) == 0)
				{
					if (lua_pcall(L, 0, 0, 0) == 0)
					{
						UE_LOG(LogWowSV, Log, TEXT("Loaded SavedVariables for %s"), *AddonName);
					}
					else
					{
						UE_LOG(LogWowSV, Warning, TEXT("Error executing SavedVariables for %s: %s"),
							*AddonName, UTF8_TO_TCHAR(lua_tostring(L, -1)));
						lua_pop(L, 1);
					}
				}
				else
				{
					UE_LOG(LogWowSV, Warning, TEXT("Error parsing SavedVariables for %s: %s"),
						*AddonName, UTF8_TO_TCHAR(lua_tostring(L, -1)));
					lua_pop(L, 1);
				}
			}
		}
	}

	// Load per-character saved variables
	if (Toc.SavedVariablesPerCharacter.Num() > 0)
	{
		FString FilePath = FPaths::Combine(GetCharacterSVPath(), AddonName + TEXT(".lua"));
		if (FPaths::FileExists(FilePath))
		{
			TArray<uint8> Data;
			if (FFileHelper::LoadFileToArray(Data, *FilePath))
			{
				FTCHARToUTF8 ChunkName(*FString::Printf(TEXT("@SavedVariablesPerCharacter/%s"), *AddonName));
				if (luaL_loadbuffer(L, (const char*)Data.GetData(), Data.Num(), ChunkName.Get()) == 0)
				{
					if (lua_pcall(L, 0, 0, 0) == 0)
					{
						UE_LOG(LogWowSV, Log, TEXT("Loaded SavedVariablesPerCharacter for %s"), *AddonName);
					}
					else
					{
						UE_LOG(LogWowSV, Warning, TEXT("Error executing SavedVariablesPerCharacter for %s: %s"),
							*AddonName, UTF8_TO_TCHAR(lua_tostring(L, -1)));
						lua_pop(L, 1);
					}
				}
				else
				{
					UE_LOG(LogWowSV, Warning, TEXT("Error parsing SavedVariablesPerCharacter for %s: %s"),
						*AddonName, UTF8_TO_TCHAR(lua_tostring(L, -1)));
					lua_pop(L, 1);
				}
			}
		}
	}
#endif
}

void FWowSavedVariables::SaveForAddon(lua_State* L, const FString& AddonName, const FWowTocData& Toc)
{
#if HAS_LUA
	if (!L) return;

	// Save account-wide variables
	if (Toc.SavedVariables.Num() > 0)
	{
		FString Output;
		for (const FString& VarName : Toc.SavedVariables)
		{
			FTCHARToUTF8 VarNameUtf8(*VarName);
			lua_getglobal(L, VarNameUtf8.Get());
			if (!lua_isnil(L, -1))
			{
				Output += VarName + TEXT(" = ");
				SerializeLuaValue(L, lua_gettop(L), Output, 0);
				Output += TEXT("\n");
			}
			lua_pop(L, 1);
		}

		if (!Output.IsEmpty())
		{
			FString DirPath = GetAccountSVPath();
			IFileManager::Get().MakeDirectory(*DirPath, true);
			FString FilePath = FPaths::Combine(DirPath, AddonName + TEXT(".lua"));
			FFileHelper::SaveStringToFile(Output, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			UE_LOG(LogWowSV, Log, TEXT("Saved %d SavedVariables for %s"), Toc.SavedVariables.Num(), *AddonName);
		}
	}

	// Save per-character variables
	if (Toc.SavedVariablesPerCharacter.Num() > 0)
	{
		FString Output;
		for (const FString& VarName : Toc.SavedVariablesPerCharacter)
		{
			FTCHARToUTF8 VarNameUtf8(*VarName);
			lua_getglobal(L, VarNameUtf8.Get());
			if (!lua_isnil(L, -1))
			{
				Output += VarName + TEXT(" = ");
				SerializeLuaValue(L, lua_gettop(L), Output, 0);
				Output += TEXT("\n");
			}
			lua_pop(L, 1);
		}

		if (!Output.IsEmpty())
		{
			FString DirPath = GetCharacterSVPath();
			IFileManager::Get().MakeDirectory(*DirPath, true);
			FString FilePath = FPaths::Combine(DirPath, AddonName + TEXT(".lua"));
			FFileHelper::SaveStringToFile(Output, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
			UE_LOG(LogWowSV, Log, TEXT("Saved %d SavedVariablesPerCharacter for %s"),
				Toc.SavedVariablesPerCharacter.Num(), *AddonName);
		}
	}
#endif
}

void FWowSavedVariables::SaveAll(lua_State* L)
{
#if HAS_LUA
	for (const auto& Pair : RegisteredAddons)
	{
		SaveForAddon(L, Pair.Key, Pair.Value);
	}
	UE_LOG(LogWowSV, Log, TEXT("SaveAll: saved variables for %d addons"), RegisteredAddons.Num());
#endif
}

void FWowSavedVariables::AppendIndent(FString& Out, int Indent)
{
	for (int i = 0; i < Indent; i++)
	{
		Out += TEXT("\t");
	}
}

void FWowSavedVariables::SerializeLuaValue(lua_State* L, int Idx, FString& Out, int Indent)
{
#if HAS_LUA
	// Prevent excessive recursion
	if (Indent > 20)
	{
		Out += TEXT("nil -- max depth exceeded");
		return;
	}

	int Type = lua_type(L, Idx);
	switch (Type)
	{
	case LUA_TNIL:
		Out += TEXT("nil");
		break;

	case LUA_TBOOLEAN:
		Out += lua_toboolean(L, Idx) ? TEXT("true") : TEXT("false");
		break;

	case LUA_TNUMBER:
	{
		double Val = lua_tonumber(L, Idx);
		// Use integer format if it's a whole number
		if (Val == FMath::FloorToDouble(Val) && FMath::Abs(Val) < 1e15)
		{
			Out += FString::Printf(TEXT("%lld"), (int64)Val);
		}
		else
		{
			Out += FString::Printf(TEXT("%.17g"), Val);
		}
		break;
	}

	case LUA_TSTRING:
	{
		const char* Str = lua_tostring(L, Idx);
		Out += TEXT("\"");
		// Escape special characters
		for (const char* p = Str; *p; p++)
		{
			switch (*p)
			{
			case '\\': Out += TEXT("\\\\"); break;
			case '"':  Out += TEXT("\\\""); break;
			case '\n': Out += TEXT("\\n"); break;
			case '\r': Out += TEXT("\\r"); break;
			case '\t': Out += TEXT("\\t"); break;
			case '\0': Out += TEXT("\\0"); break;
			default:
				if ((uint8)*p < 32)
				{
					Out += FString::Printf(TEXT("\\%d"), (int)(uint8)*p);
				}
				else
				{
					Out += (TCHAR)*p;
				}
				break;
			}
		}
		Out += TEXT("\"");
		break;
	}

	case LUA_TTABLE:
	{
		Out += TEXT("{\n");
		int NewIndent = Indent + 1;

		// Check if table is array-like (sequential integer keys starting at 1)
		int ArrayLen = 0;
#if LUA_VERSION_NUM >= 502
		ArrayLen = (int)lua_rawlen(L, Idx);
#else
		ArrayLen = (int)lua_objlen(L, Idx);
#endif

		// Serialize array part
		for (int i = 1; i <= ArrayLen; i++)
		{
			lua_rawgeti(L, Idx, i);
			if (lua_isnil(L, -1))
			{
				lua_pop(L, 1);
				break;
			}
			AppendIndent(Out, NewIndent);
			SerializeLuaValue(L, lua_gettop(L), Out, NewIndent);
			Out += TEXT(", -- [");
			Out += FString::FromInt(i);
			Out += TEXT("]\n");
			lua_pop(L, 1);
		}

		// Serialize hash part (non-integer or out-of-range keys)
		lua_pushnil(L);
		while (lua_next(L, Idx) != 0)
		{
			// Skip array keys we already serialized
			if (lua_type(L, -2) == LUA_TNUMBER)
			{
				double Key = lua_tonumber(L, -2);
				if (Key == FMath::FloorToDouble(Key) && Key >= 1 && Key <= ArrayLen)
				{
					lua_pop(L, 1);
					continue;
				}
			}

			AppendIndent(Out, NewIndent);

			// Key
			if (lua_type(L, -2) == LUA_TSTRING)
			{
				const char* KeyStr = lua_tostring(L, -2);
				// Check if key is a valid Lua identifier
				bool bValidIdent = true;
				if (KeyStr[0] == '\0' || (KeyStr[0] != '_' && !FChar::IsAlpha(KeyStr[0])))
					bValidIdent = false;
				for (const char* p = KeyStr + 1; *p && bValidIdent; p++)
				{
					if (*p != '_' && !FChar::IsAlnum(*p))
						bValidIdent = false;
				}

				if (bValidIdent)
				{
					Out += TEXT("[\"");
					Out += UTF8_TO_TCHAR(KeyStr);
					Out += TEXT("\"]");
				}
				else
				{
					Out += TEXT("[\"");
					// Escape the key string
					for (const char* p = KeyStr; *p; p++)
					{
						switch (*p)
						{
						case '\\': Out += TEXT("\\\\"); break;
						case '"':  Out += TEXT("\\\""); break;
						case '\n': Out += TEXT("\\n"); break;
						default: Out += (TCHAR)*p; break;
						}
					}
					Out += TEXT("\"]");
				}
			}
			else if (lua_type(L, -2) == LUA_TNUMBER)
			{
				Out += TEXT("[");
				double Key = lua_tonumber(L, -2);
				if (Key == FMath::FloorToDouble(Key))
					Out += FString::Printf(TEXT("%lld"), (int64)Key);
				else
					Out += FString::Printf(TEXT("%.17g"), Key);
				Out += TEXT("]");
			}
			else
			{
				// Skip non-string/number keys (can't be serialized)
				lua_pop(L, 1);
				continue;
			}

			Out += TEXT(" = ");
			SerializeLuaValue(L, lua_gettop(L), Out, NewIndent);
			Out += TEXT(",\n");
			lua_pop(L, 1); // pop value, keep key for next iteration
		}

		AppendIndent(Out, Indent);
		Out += TEXT("}");
		break;
	}

	default:
		// Functions, userdata, threads — skip with nil
		Out += TEXT("nil");
		break;
	}
#endif
}

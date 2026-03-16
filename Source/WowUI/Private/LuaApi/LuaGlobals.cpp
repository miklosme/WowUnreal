#include "LuaApiRegistry.h"

#if __has_include("lua.h")
extern "C" {
#include "lua.h"
#include "lauxlib.h"
#include "lualib.h"
}

DEFINE_LOG_CATEGORY_STATIC(LogWowLuaApi, Log, All);

// ─── print(...) — output to UE log ──────────────────────────────────────────────
static int L_print(lua_State* L)
{
    int n = lua_gettop(L);
    FString Out;
    for (int i = 1; i <= n; i++)
    {
        if (i > 1) Out += TEXT("\t");
        if (lua_isstring(L, i))
            Out += UTF8_TO_TCHAR(lua_tostring(L, i));
        else if (lua_isnil(L, i))
            Out += TEXT("nil");
        else if (lua_isboolean(L, i))
            Out += lua_toboolean(L, i) ? TEXT("true") : TEXT("false");
        else
            Out += FString::Printf(TEXT("%s: %p"), UTF8_TO_TCHAR(luaL_typename(L, i)), lua_topointer(L, i));
    }
    UE_LOG(LogWowLuaApi, Log, TEXT("[Lua] %s"), *Out);
    return 0;
}

// ─── format(fmt, ...) — alias for string.format ────────────────────────────────
static int L_format(lua_State* L)
{
    lua_getglobal(L, "string");
    lua_getfield(L, -1, "format");
    lua_remove(L, -2);
    // Push all arguments
    int n = lua_gettop(L) - 1; // -1 for the function we just pushed
    for (int i = 1; i <= n; i++)
    {
        lua_pushvalue(L, i);
    }
    lua_call(L, n, 1);
    return 1;
}

// ─── strsplit(delimiter, str, pieces) ───────────────────────────────────────────
static int L_strsplit(lua_State* L)
{
    const char* delim = luaL_checkstring(L, 1);
    const char* str = luaL_checkstring(L, 2);
    int maxPieces = luaL_optinteger(L, 3, 0);

    FString Input = UTF8_TO_TCHAR(str);
    FString Delimiter = UTF8_TO_TCHAR(delim);

    TArray<FString> Parts;
    if (Delimiter.Len() == 1)
    {
        Input.ParseIntoArray(Parts, *Delimiter);
    }
    else
    {
        Parts.Add(Input);
    }

    if (maxPieces > 0 && Parts.Num() > maxPieces)
    {
        // Rejoin excess parts into last element
        FString Last;
        for (int32 i = maxPieces - 1; i < Parts.Num(); i++)
        {
            if (i > maxPieces - 1) Last += Delimiter;
            Last += Parts[i];
        }
        Parts.SetNum(maxPieces);
        Parts.Last() = Last;
    }

    for (const FString& Part : Parts)
    {
        FTCHARToUTF8 Conv(*Part);
        lua_pushstring(L, Conv.Get());
    }
    return Parts.Num();
}

// ─── strtrim(str) ───────────────────────────────────────────────────────────────
static int L_strtrim(lua_State* L)
{
    const char* str = luaL_checkstring(L, 1);
    FString S = UTF8_TO_TCHAR(str);
    S = S.TrimStartAndEnd();
    FTCHARToUTF8 Conv(*S);
    lua_pushstring(L, Conv.Get());
    return 1;
}

// ─── strbyte, strchar etc — aliases for string.* ────────────────────────────────
static int L_string_alias(lua_State* L, const char* method)
{
    lua_getglobal(L, "string");
    lua_getfield(L, -1, method);
    lua_remove(L, -2);
    int n = lua_gettop(L) - 1;
    for (int i = 1; i <= n; i++) lua_pushvalue(L, i);
    lua_call(L, n, LUA_MULTRET);
    return lua_gettop(L) - n;
}

#define DEF_STRING_ALIAS(name, method) \
    static int L_##name(lua_State* L) { return L_string_alias(L, #method); }

DEF_STRING_ALIAS(strbyte, byte)
DEF_STRING_ALIAS(strchar, char)
DEF_STRING_ALIAS(strfind, find)
DEF_STRING_ALIAS(strlen, len)
DEF_STRING_ALIAS(strlower, lower)
DEF_STRING_ALIAS(strupper, upper)
DEF_STRING_ALIAS(strsub, sub)
DEF_STRING_ALIAS(strrep, rep)
DEF_STRING_ALIAS(gsub, gsub)
DEF_STRING_ALIAS(gmatch, gmatch)
DEF_STRING_ALIAS(strmatch, match)
DEF_STRING_ALIAS(strrev, reverse)

// ─── Table functions ────────────────────────────────────────────────────────────

// wipe(table) — clear all keys from table, return it
static int L_wipe(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnil(L);
    while (lua_next(L, 1) != 0)
    {
        lua_pop(L, 1); // pop value
        lua_pushvalue(L, -1); // copy key
        lua_pushnil(L);
        lua_rawset(L, 1); // table[key] = nil
    }
    lua_settop(L, 1);
    return 1;
}

// tinsert — alias for table.insert
static int L_tinsert(lua_State* L)
{
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "insert");
    lua_remove(L, -2);
    int n = lua_gettop(L) - 1;
    for (int i = 1; i <= n; i++) lua_pushvalue(L, i);
    lua_call(L, n, 0);
    return 0;
}

// tremove — alias for table.remove
static int L_tremove(lua_State* L)
{
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "remove");
    lua_remove(L, -2);
    int n = lua_gettop(L) - 1;
    for (int i = 1; i <= n; i++) lua_pushvalue(L, i);
    lua_call(L, n, LUA_MULTRET);
    return lua_gettop(L) - n;
}

// sort — alias for table.sort
static int L_sort(lua_State* L)
{
    lua_getglobal(L, "table");
    lua_getfield(L, -1, "sort");
    lua_remove(L, -2);
    int n = lua_gettop(L) - 1;
    for (int i = 1; i <= n; i++) lua_pushvalue(L, i);
    lua_call(L, n, 0);
    return 0;
}

// getn — returns table size (for indexed arrays)
static int L_getn(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushnumber(L, luaL_getn(L, 1));
    return 1;
}

// foreach — iterate over table, calling function for each key-value pair
static int L_foreach(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    lua_pushnil(L); // first key
    while (lua_next(L, 1) != 0) // table is at index 1
    {
        lua_pushvalue(L, 2); // function
        lua_pushvalue(L, -3); // key
        lua_pushvalue(L, -3); // value
        lua_call(L, 2, 1); // call function(key, value)

        if (!lua_isnil(L, -1))
        {
            // If function returns non-nil, stop iteration
            lua_pop(L, 2); // remove value and key
            return 1;
        }
        lua_pop(L, 2); // remove result and value, keep key for next iteration
    }
    return 0;
}

// foreachi — iterate over indexed array, calling function for each index-value pair
static int L_foreachi(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    luaL_checktype(L, 2, LUA_TFUNCTION);

    int n = luaL_getn(L, 1);
    for (int i = 1; i <= n; i++)
    {
        lua_pushvalue(L, 2); // function
        lua_pushnumber(L, i); // index
        lua_rawgeti(L, 1, i); // value
        lua_call(L, 2, 1); // call function(index, value)

        if (!lua_isnil(L, -1))
        {
            // If function returns non-nil, return it and stop
            return 1;
        }
        lua_pop(L, 1); // remove result
    }
    return 0;
}

// unpack — unpack array elements onto the stack
static int L_unpack(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    int n = luaL_getn(L, 1);
    luaL_checkstack(L, n, "table too big to unpack");
    for (int i = 1; i <= n; i++)
    {
        lua_rawgeti(L, 1, i);
    }
    return n;
}

// select — return arguments starting from index (WoW-specific function)
static int L_select(lua_State* L)
{
    int n = lua_gettop(L);
    if (n == 0)
    {
        lua_pushnumber(L, 0);
        return 1;
    }

    if (lua_isstring(L, 1) && strcmp(lua_tostring(L, 1), "#") == 0)
    {
        lua_pushnumber(L, n - 1);
        return 1;
    }

    int start = static_cast<int>(luaL_checknumber(L, 1));
    if (start < 0) start = n + start + 1; // negative indices count from end

    if (start <= 0) start = 1;
    if (start > n) return 0;

    for (int i = start + 1; i <= n; i++)
    {
        lua_pushvalue(L, i);
    }
    return n - start;
}

// pairs — return iterator for table
static int L_pairs(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_getglobal(L, "next");
    lua_pushvalue(L, 1);
    lua_pushnil(L);
    return 3;
}

// ipairs — return iterator for indexed array
static int L_ipairs(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_pushcfunction(L, [](lua_State* L2) -> int {
        int i = static_cast<int>(luaL_checknumber(L2, 2)) + 1;
        lua_pushnumber(L2, i);
        lua_rawgeti(L2, 1, i);
        return lua_isnil(L2, -1) ? 0 : 2;
    });
    lua_pushvalue(L, 1);
    lua_pushnumber(L, 0);
    return 3;
}

// next — basic table iterator
static int L_next(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_settop(L, 2);
    if (lua_next(L, 1))
        return 2;
    else
    {
        lua_pushnil(L);
        return 1;
    }
}

// rawget — get table value without invoking metamethods
static int L_rawget(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_rawget(L, 1);
    return 1;
}

// rawset — set table value without invoking metamethods
static int L_rawset(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TTABLE);
    lua_rawset(L, 1);
    lua_pushvalue(L, 1);
    return 1;
}

// ─── Time functions ─────────────────────────────────────────────────────────────

// GetTime() — returns game time in seconds as a float
static int L_GetTime(lua_State* L)
{
    lua_pushnumber(L, FPlatformTime::Seconds());
    return 1;
}

// debugprofilestop() — returns milliseconds since last call
static int L_debugprofilestop(lua_State* L)
{
    static double LastTime = FPlatformTime::Seconds();
    double Now = FPlatformTime::Seconds();
    lua_pushnumber(L, (Now - LastTime) * 1000.0);
    LastTime = Now;
    return 1;
}

// ─── Error handling ─────────────────────────────────────────────────────────────

static int L_geterrorhandler(lua_State* L)
{
    lua_getglobal(L, "_ERRORHANDLER");
    if (lua_isnil(L, -1))
    {
        lua_pop(L, 1);
        lua_pushcfunction(L, [](lua_State* L2) -> int {
            const char* msg = lua_tostring(L2, 1);
            UE_LOG(LogWowLuaApi, Error, TEXT("[Lua Error] %s"), msg ? UTF8_TO_TCHAR(msg) : TEXT("(nil)"));
            return 0;
        });
    }
    return 1;
}

static int L_seterrorhandler(lua_State* L)
{
    luaL_checktype(L, 1, LUA_TFUNCTION);
    lua_pushvalue(L, 1);
    lua_setglobal(L, "_ERRORHANDLER");
    return 0;
}

// securecall — call function in secure environment
static int L_securecall(lua_State* L)
{
    if (lua_gettop(L) == 0) return 0;
    luaL_checktype(L, 1, LUA_TFUNCTION);

    int nargs = lua_gettop(L) - 1;
    lua_call(L, nargs, LUA_MULTRET);
    return lua_gettop(L);
}

// hooksecurefunc — hook a secure function
static int L_hooksecurefunc(lua_State* L)
{
    // For now, just store the hook but don't actually implement secure call hooking
    return 0;
}

// issecurevariable — check if a variable is secure
static int L_issecurevariable(lua_State* L)
{
    lua_pushboolean(L, 1); // Everything is secure for now
    return 1;
}

// forceinsecure — force insecure context
static int L_forceinsecure(lua_State* L)
{
    return 0;
}

// tostringall — convert all arguments to strings and return them
static int L_tostringall(lua_State* L)
{
    int n = lua_gettop(L);
    for (int i = 1; i <= n; i++)
    {
        const char* s = lua_tolstring(L, i, nullptr);
        lua_remove(L, i);
        lua_insert(L, i);
    }
    return n;
}

// ─── Math aliases ───────────────────────────────────────────────────────────────
static void RegisterMathAliases(lua_State* L)
{
    // WoW provides these as global aliases for math.* functions
    const char* aliases[] = {
        "abs", "ceil", "floor", "max", "min", "mod", "fmod", "random",
        "sqrt", "sin", "cos", "tan", "atan", "atan2", "pow", "log", "exp",
        "deg", "rad", nullptr
    };

    for (int i = 0; aliases[i]; i++)
    {
        lua_getglobal(L, "math");
        const char* name = aliases[i];
        // "mod" maps to math.fmod in Lua 5.1, but also add "fmod" alias
        if (strcmp(name, "mod") == 0)
            lua_getfield(L, -1, "fmod");
        else
            lua_getfield(L, -1, name);
        lua_remove(L, -2); // remove math table
        if (!lua_isnil(L, -1))
            lua_setglobal(L, name);
        else
            lua_pop(L, 1);
    }

    // Add math constants
    lua_getglobal(L, "math");
    lua_getfield(L, -1, "pi");
    if (!lua_isnil(L, -1))
        lua_setglobal(L, "pi");
    else
        lua_pop(L, 1);

    lua_getfield(L, -1, "huge");
    if (!lua_isnil(L, -1))
        lua_setglobal(L, "huge");
    else
        lua_pop(L, 1);

    lua_pop(L, 1); // pop math table
}

// ─── Registration ───────────────────────────────────────────────────────────────

void WowLuaApi::RegisterGlobals(lua_State* L)
{
    lua_register(L, "print", L_print);
    lua_register(L, "format", L_format);
    lua_register(L, "strsplit", L_strsplit);
    lua_register(L, "strtrim", L_strtrim);

    // String aliases
    lua_register(L, "strbyte", L_strbyte);
    lua_register(L, "strchar", L_strchar);
    lua_register(L, "strfind", L_strfind);
    lua_register(L, "strlen", L_strlen);
    lua_register(L, "strlower", L_strlower);
    lua_register(L, "strupper", L_strupper);
    lua_register(L, "strsub", L_strsub);
    lua_register(L, "strrep", L_strrep);
    lua_register(L, "gsub", L_gsub);
    lua_register(L, "gmatch", L_gmatch);
    lua_register(L, "strmatch", L_strmatch);
    lua_register(L, "strrev", L_strrev);

    // Table functions
    lua_register(L, "wipe", L_wipe);
    lua_register(L, "tinsert", L_tinsert);
    lua_register(L, "tremove", L_tremove);
    lua_register(L, "sort", L_sort);
    lua_register(L, "getn", L_getn);
    lua_register(L, "foreach", L_foreach);
    lua_register(L, "foreachi", L_foreachi);
    lua_register(L, "unpack", L_unpack);
    lua_register(L, "select", L_select);
    lua_register(L, "pairs", L_pairs);
    lua_register(L, "ipairs", L_ipairs);
    lua_register(L, "next", L_next);
    lua_register(L, "rawget", L_rawget);
    lua_register(L, "rawset", L_rawset);

    // Time
    lua_register(L, "GetTime", L_GetTime);
    lua_register(L, "debugprofilestop", L_debugprofilestop);

    // Error handling
    lua_register(L, "geterrorhandler", L_geterrorhandler);
    lua_register(L, "seterrorhandler", L_seterrorhandler);

    // Security functions
    lua_register(L, "securecall", L_securecall);
    lua_register(L, "hooksecurefunc", L_hooksecurefunc);
    lua_register(L, "issecurevariable", L_issecurevariable);
    lua_register(L, "forceinsecure", L_forceinsecure);
    lua_register(L, "tostringall", L_tostringall);

    // Math aliases
    RegisterMathAliases(L);

    UE_LOG(LogWowLuaApi, Log, TEXT("Registered WoW Lua globals (Phase 1 bootstrap)"));
}

#else
void WowLuaApi::RegisterGlobals(lua_State*) {}
#endif

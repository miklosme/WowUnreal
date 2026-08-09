# Lua API Bindings

## Goal
Implement the WoW Lua API functions needed for the default Blizzard UI and popular addons to run.

## Context
- Lua 5.1.5 VM embedded in `Source/WowUI/Public/WowLuaVM.h`
- No API functions bound yet
- ~1200 API functions total, prioritized by dependency
- Reference: WoW 3.3.5 API documentation (wowprogramming.com / wowwiki archives)
- The default UI (`FrameXML/` from MPQ) is the ultimate test — if it loads, most addons work

## Requirements

### Binding Pattern
1. Each API function registered as C function in Lua via `lua_register` or `luaL_register`
2. Functions read args from Lua stack, perform action, push results to stack
3. Group related functions into registration modules
4. Error handling: invalid args → `luaL_argerror`, not crash

### Phase 1 — Bootstrap (needed for FrameXML to parse)
These must work before any XML/addon loading:

**Global Functions:**
- `print(...)` — output to chat/log
- `format(fmt, ...)` — alias for string.format
- `strsplit(delimiter, str, pieces)` — WoW-specific string split
- `strtrim(str)` — trim whitespace
- `strbyte`, `strchar`, `strfind`, `strlen`, `strlower`, `strupper`, `strsub`, `strrep`
- `gsub`, `gmatch`, `match` — pattern matching
- `tinsert`, `tremove`, `wipe`, `sort` — table functions
- `pairs`, `ipairs`, `next`, `select`, `unpack`, `type`, `tostring`, `tonumber`
- `pcall`, `xpcall`, `error`, `assert`
- `date`, `time`, `difftime`, `GetTime()` — returns float seconds
- `debugprofilestop()` — performance timing
- `geterrorhandler`, `seterrorhandler`

**Math:** (most already in Lua stdlib, add WoW aliases)
- `abs`, `ceil`, `floor`, `max`, `min`, `mod`, `random`, `sqrt`, `sin`, `cos`, `atan2`, `pow`, `log`, `exp`

### Phase 2 — Frame System (needed for UI creation)
- `CreateFrame(type, name, parent, template)` — creates UI frame
- Frame methods: `SetPoint`, `SetSize`, `SetWidth`, `SetHeight`, `Show`, `Hide`, `SetAlpha`, `SetScale`, `GetName`, `GetParent`, `GetFrameType`, `SetFrameStrata`, `SetFrameLevel`, `IsVisible`, `IsShown`, `GetWidth`, `GetHeight`, `GetScale`, `GetAlpha`, `EnableMouse`, `EnableKeyboard`, `SetID`, `GetID`
- `GetParent`, `GetChildren`, `GetRegions`, `GetNumChildren`, `GetNumRegions`
- `SetScript(handler, func)`, `GetScript(handler)`, `HookScript(handler, func)`
- `RegisterEvent(event)`, `UnregisterEvent(event)`, `RegisterAllEvents`, `UnregisterAllEvents`, `IsEventRegistered`

**Texture/FontString:**
- Frame:CreateTexture, Frame:CreateFontString
- Texture: `SetTexture`, `SetTexCoord`, `SetVertexColor`, `SetBlendMode`, `GetTexture`
- FontString: `SetText`, `GetText`, `SetFont`, `SetTextColor`, `SetJustifyH`, `SetJustifyV`, `SetWordWrap`

### Phase 3 — Game State (needed for gameplay UI)
- Unit API: `UnitName`, `UnitLevel`, `UnitHealth`, `UnitHealthMax`, `UnitPower`, `UnitPowerMax`, `UnitClass`, `UnitRace`, `UnitExists`, `UnitIsPlayer`, `UnitIsDead`, `UnitGUID`
- Target: `TargetUnit`, `ClearTarget`
- Spell: `GetSpellInfo`, `GetSpellCooldown`, `CastSpellByName`, `CastSpellByID`
- Item: `GetItemInfo`, `GetContainerItemInfo`, `GetContainerNumSlots`
- Chat: `SendChatMessage`, `DEFAULT_CHAT_FRAME:AddMessage`
- Action bar: `GetActionInfo`, `GetActionTexture`, `HasAction`, `UseAction`

### Phase 4 — Stub API
Functions that can return safe defaults initially:
- `IsInInstance() → false`
- `GetNumPartyMembers() → 0`
- `GetNumRaidMembers() → 0`
- `IsInGuild() → false`
- `GetMoney() → 0`
- `GetPlayerMapPosition() → 0, 0`
- Stub returning nil/0/false is fine for initial load — the UI degrades gracefully

### Sandbox
1. Remove or restrict: `os`, `io`, `debug`, `loadfile`, `dofile`
2. Allow: `loadstring` (addons use it)
3. Memory limit per addon environment
4. Execution timeout (prevent infinite loops)

## Architecture

### File Structure
```
WowUI/Private/LuaApi/
├── LuaGlobals.cpp       — print, format, strsplit, GetTime, etc.
├── LuaFrame.cpp         — CreateFrame, frame methods
├── LuaTexture.cpp       — Texture/FontString methods
├── LuaUnit.cpp          — Unit* functions
├── LuaSpell.cpp         — Spell functions
├── LuaItem.cpp          — Item functions
├── LuaChat.cpp          — Chat functions
├── LuaActionBar.cpp     — Action bar functions
├── LuaStubs.cpp         — Stub implementations returning defaults
└── LuaApiRegistry.cpp   — Master registration function
```

## Acceptance Criteria
- [ ] Builds without errors
- [ ] `print("Hello from Lua")` outputs to UE log
- [ ] `CreateFrame("Frame", "TestFrame")` creates a frame object
- [ ] `strsplit`, `strtrim`, `format` work correctly
- [ ] `GetTime()` returns increasing float values
- [ ] Phase 1 functions all registered (verify by calling from Lua console)
- [ ] FrameXML `UIParent.lua` begins loading without immediate errors
- [ ] Screenshot shows game running (Lua errors in log are OK at this stage, crashes are not)

## Verification Steps
1. Build the editor target using [development setup](../setup/development.md)
2. Launch the relevant map using [development setup](../setup/development.md)
3. Check log for Lua initialization messages
4. Add test: execute `print(strsplit(",", "a,b,c"))` from C++ on VM startup
5. Attempt to load first few FrameXML files — log shows progress before errors

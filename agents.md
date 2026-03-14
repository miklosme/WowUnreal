# AGENTS.md — WowUnreal Autonomous Agent Operating Manual

## Core Principle

**One task at a time.** Pick ONE incomplete `[ ]` task from `IMPLEMENTATION_PLAN.md`, complete it, verify it compiles and runs, then commit. Loop until ALL tasks are `[x]`.

- Mark completed work as `[?]` (pending verification). The **next iteration** verifies it works before marking `[x]`.
- **ALL tasks must be completed** — not just high-priority ones. Work through the plan systematically.
- **Commit after every successful change.** Small, atomic commits.
- **Never skip verification.** Every change must compile, run, and produce a screenshot.

---

## Build Commands

```bash
# UE5 engine root
UE="/Users/Shared/Epic Games/UE_5.7"

# Build (Development)
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" WowUnreal Mac Development \
  -project="/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" -waitmutex

# Build (Editor — for PIE testing)
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" WowUnrealEditor Mac Development \
  -project="/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" -waitmutex

# Quick compile check (no linking)
"$UE/Engine/Build/BatchFiles/Mac/Build.sh" WowUnrealEditor Mac Development \
  -project="/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" -waitmutex -NoLink
```

## Run & Screenshot Verification

```bash
# Launch editor in unattended mode, load default map, take screenshot, exit
"$UE/Engine/Binaries/Mac/UnrealEditor" \
  "/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" \
  -game -unattended -nosplash -nullrhi -log \
  -ExecCmds="HighResShot 1920x1080" -AutomationExitOnFinish

# Or launch PIE from editor CLI
"$UE/Engine/Binaries/Mac/UnrealEditor" \
  "/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" \
  -game -windowed -resx=1920 -resy=1080 -nosplash -log
```

**Every task completion MUST include:**
1. Build succeeds (zero errors)
2. Launch the game/editor
3. Take a screenshot of the rendered world to visually confirm the change works
4. Save screenshot to `Saved/Screenshots/` with descriptive name

---

## Test Commands

```bash
# UE5 Automation tests (if any exist)
"$UE/Engine/Binaries/Mac/UnrealEditor" \
  "/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" \
  -ExecCmds="Automation RunTests WowUnreal" -unattended -nosplash -nullrhi -log

# Quick smoke test: does it compile and launch without crashing?
"$UE/Engine/Binaries/Mac/UnrealEditor" \
  "/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" \
  -game -nullrhi -unattended -nosplash -log -ExecCmds="quit"
```

---

## Error Handling

- **Build errors:** Read the error, fix it. If stuck after 3 attempts on the same error, mark the task as `[B]` (blocked) in the plan with a note explaining why, and move to the next task.
- **Test/runtime failures:** Fix before proceeding. Do not move on with a broken build.
- **File not found:** Use `Glob` or `ls` to locate the file. WoW data files are case-insensitive — normalize paths to backslash, lowercase when searching MPQ.
- **Link errors (undefined symbols):** Check `Build.cs` module dependencies. Common fix: add missing module to `PublicDependencyModuleNames`.
- **UE macro errors:** Ensure `GENERATED_BODY()`, proper `#include` order (`.generated.h` last), and correct `UCLASS`/`UPROPERTY`/`UFUNCTION` syntax.

---

## Task Selection Workflow

```
1. Read IMPLEMENTATION_PLAN.md
2. Find any [?] tasks — verify they work, mark [x] if good or [ ] if broken
3. Find the first incomplete [ ] task (respect phase ordering)
4. Check if it has blocking dependencies — complete those first
5. Execute the task
6. Build → Run → Screenshot → Verify
7. Mark as [?], commit with descriptive message
8. Update IMPLEMENTATION_PLAN.md
9. Loop until ALL tasks are [x]
```

---

## Project-Specific Rules

### Code Structure
- **7 modules:** WowUnreal, WowData, WowAssets, WowWorld, WowUI, WowNetwork, WowClient
- Keep format parsing in `WowData`, UE asset conversion in `WowAssets`, rendering in `WowWorld`
- All coordinates must go through the WoW→UE conversion utilities (WoW: Y-north, Z-up → UE: X-forward, Z-up, 100x scale)

### Data & Assets
- **Never ship Blizzard assets.** All data read from MPQ at runtime from `~/World of Warcraft 3.3.5a/Data`
- MPQ file paths are case-insensitive with backslash separators — normalize when constructing paths
- BLP textures: use DXT passthrough to GPU (no CPU decompression for DXT1/3/5)
- Currently using `ProceduralMeshComponent` — migrate to `UStaticMesh`/`USkeletalMesh` when the plan says to

### Networking
- Protocol version: **12340** (3.3.5a)
- Test server: `127.0.0.1` (auth:3724, world:8085) — Account: `WowTestUser` / `WowTestPass`
- Server is AzerothCore — reference `~/projects/azerothcore-wotlk` for opcodes and packet structures

### Reference Projects
- `~/projects/noggit3` — ADT/terrain structs, lighting
- `~/projects/pywowlib` — Most readable format definitions
- `~/projects/wowmodelviewer` — M2 animation, character rendering
- `~/projects/azerothcore-wotlk` — Network protocol, opcodes
- **Do NOT use `~/projects/WowGodot` as reference** — it has known issues

### Lua / UI
- Lua version: **5.1.5** (not 5.2+)
- Sandbox the Lua VM — restrict `os`, `io`, `debug` modules
- Target compatibility with real WoW 3.3.5 addons

### Performance
- Target: **60+ FPS** on mid-range GPU
- Memory budget: **<2GB** for world data
- Prefer instanced rendering for repeated meshes
- No main-thread stalls during streaming — use async loading

### Commit Style
- One logical change per commit
- Prefix: `Add:`, `Fix:`, `Update:`, `Refactor:`, `WIP:` as appropriate
- Include what was verified (e.g., "verified: builds, terrain renders correctly")

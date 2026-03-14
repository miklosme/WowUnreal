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

**IMPORTANT: Do NOT use OS-level screenshot tools (`screencapture`, `osascript`, etc.). Always use the Unreal Engine screenshot API.**

```bash
# Launch the game with rendering enabled (NO -nullrhi — that disables the GPU)
"$UE/Engine/Binaries/Mac/UnrealEditor" \
  "/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" \
  -game -windowed -resx=1920 -resy=1080 -nosplash -log
```

Screenshots are taken via `UWowScreenshotManager::TakeScreenshot()` which calls `FScreenshotRequest::RequestScreenshot()` — the correct UE API that captures the rendered viewport. The game also supports a delayed auto-screenshot on launch (see `AWowWorldManager`).

To request a screenshot from the console or exec command:
```bash
# Use UE console command to capture viewport
-ExecCmds="HighResShot 1920x1080"
```

**Every task completion MUST include:**
1. Build succeeds (zero errors)
2. Launch the game with rendering enabled (never use `-nullrhi` for visual verification)
3. Take a screenshot using the UE screenshot API (NOT OS-level `screencapture`)
4. Screenshot saves to `Saved/Screenshots/` with descriptive name
5. **VALIDATE the screenshot is not black/empty** — if it is, the task FAILED. Do not commit.

### Screenshot Validation — MANDATORY

After capturing a screenshot, you MUST validate it is not a black/empty image:
```bash
# Check if screenshot has actual content (not all black)
python3 -c "
from PIL import Image; import sys
img = Image.open(sys.argv[1])
pixels = list(img.getdata())
non_black = sum(1 for p in pixels if max(p[:3]) > 10)
pct = non_black / len(pixels) * 100
print(f'Non-black pixels: {pct:.1f}%')
if pct < 1.0:
    print('FAIL: Screenshot is black/empty — game is not rendering')
    sys.exit(1)
print('PASS: Screenshot has content')
" Saved/Screenshots/your_screenshot.png
```

**If the screenshot is black:**
- Do NOT commit the change
- Do NOT mark the task as `[?]`
- Investigate WHY the game isn't rendering
- Check the log for `FinalPreExposure`, `Error`, `Fatal`, or crash messages
- Fix the rendering issue BEFORE proceeding

### Memory & Performance — MANDATORY

- **Never load more than 1 tile synchronously per tick** — use `LoadTileAsync`
- **Never spawn LOD1/WDL tiles all at once** — throttle to a few per tick
- **LOD1 tiles must NOT load per-chunk textures** — use simple solid materials
- **If the game hangs, freezes, or locks up the machine, the task FAILED**
- The game should remain responsive — no multi-second stalls on the game thread

---

## Test Commands

```bash
# UE5 Automation tests (if any exist) — nullrhi is OK here since these are logic tests
"$UE/Engine/Binaries/Mac/UnrealEditor" \
  "/Users/clancey/Projects/WowUnreal/WowUnreal.uproject" \
  -ExecCmds="Automation RunTests WowUnreal" -unattended -nosplash -nullrhi -log

# Quick smoke test: does it compile and launch without crashing?
# NOTE: Use -nullrhi ONLY for non-visual smoke tests. For screenshot verification, always launch with rendering enabled.
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
- Prefer instanced rendering for repeated meshes
- No main-thread stalls during streaming — use async loading
- Throttle tile/object spawning — never spawn dozens of heavy objects in a single frame

### Commit Style
- One logical change per commit
- Prefix: `Add:`, `Fix:`, `Update:`, `Refactor:`, `WIP:` as appropriate
- Include what was verified (e.g., "verified: builds, terrain renders correctly")

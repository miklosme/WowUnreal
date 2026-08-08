# WowUnreal Specs

Specs are the **source of truth** for what needs to be built. Each spec file describes a feature or system in enough detail that an autonomous agent can implement it without ambiguity.

## Principles

- **Specs + stdlib = generate** — A spec combined with UE5/C++ standard knowledge should be enough to produce working code.
- **Concise but complete** — Detailed enough to implement from, short enough to fit in agent context.
- **Verifiable** — Every spec has acceptance criteria that can be checked by building, running, and screenshotting.

## Spec File Format

```markdown
# Feature Name

## Goal
One-sentence description of what this feature does and why.

## Context
- What already exists (modules, files, classes)
- What this depends on
- Reference projects to consult (noggit3, pywowlib, wowmodelviewer, azerothcore)

## Requirements
Numbered list of what must be implemented. Be specific about:
- File formats and binary layouts
- UE5 classes/APIs to use
- Data flow (input → processing → output)

## Architecture
- New files/classes to create
- Which module they belong to (WowData, WowAssets, WowWorld, etc.)
- Key interfaces and data structures

## Acceptance Criteria
Checklist that an agent uses to verify the work is done:
- [ ] Builds without errors (`run_test.sh build`)
- [ ] Runs without crashes (`run_test.sh`)
- [ ] Screenshot shows expected visual result
- [ ] Specific functional checks (e.g., "water renders at correct height")

## Verification Steps
Exact commands or steps to verify this spec is implemented:
1. Build: `./run_test.sh build`
2. Run: `./run_test.sh`
3. Screenshot: Use `UWowScreenshotManager::TakeScreenshot()` or check `~/Library/Logs/WowUnreal/`
4. Visual check: describe what should be visible in the screenshot
```

## Agent Workflow

When an agent picks up a spec:

1. **Read the spec** — understand requirements and acceptance criteria
2. **Check context** — read existing code mentioned in the spec
3. **Implement** — write the code
4. **Build** — run `./run_test.sh build` and fix any compile errors
5. **Run** — launch with `./run_test.sh` and check logs for errors
6. **Screenshot** — take a screenshot to visually verify the result
7. **Report** — confirm all acceptance criteria are met

## File Organization

```
specs/
├── README.md          — This file
├── overview.md        — Project goals, architecture, current status
├── terrain-lod.md     — Terrain LOD and WDL distant terrain
├── water.md           — Water/lava/slime rendering (MH2O)
├── sky-atmosphere.md  — Sky, lighting, fog, day/night cycle
├── m2-animation.md    — Skeletal animation pipeline for M2 models
├── character.md       — Character models, equipment, customization
├── static-mesh.md     — ProceduralMesh → UStaticMesh migration
├── dbc-wrappers.md    — Typed DBC table wrappers
├── networking.md      — Packet handlers and entity system
├── movement.md        — Player movement and camera
├── lua-api.md         — Lua VM API bindings
├── ui-framexml.md     — FrameXML and widget system
└── audio.md           — Music, ambience, sound effects
```

## References

Do NOT use WowGodot as a primary reference (it has issues). Use these instead:
- **noggit3** (`~/projects/noggit3`) — ADT/terrain, Lua scripting, lighting
- **pywowlib** (`~/projects/pywowlib`) — Most readable format definitions
- **wowmodelviewer** (`~/projects/wowmodelviewer`) — M2 animation, character rendering
- **WMVx** (`~/projects/WMVx`) — Modern model viewer
- **azerothcore-wotlk** (`~/projects/azerothcore-wotlk`) — Network protocol, opcodes

## Build & Test

```bash
# Build and run
./run_test.sh build

# Run only (skip build)
./run_test.sh

# UE5 location
/Users/Shared/Epic Games/UE_5.7

# WoW data
~/Downloads/World of Warcraft 3.3.5a/Data

# Logs
~/Library/Logs/WowUnreal/WowUnreal.log
```

## Technical Constraints

- UE5 5.7, C++ (no Blueprints for core logic)
- Protocol version 12340 (WoW 3.3.5a)
- Lua 5.1.5 (not 5.2+)
- Target: 60+ FPS, < 4GB RAM
- Platforms: macOS (primary dev), Windows
- Data: user-supplied MPQ files, never distributed

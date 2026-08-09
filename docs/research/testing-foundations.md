# Testing foundations

Research date: 2026-08-09

This note records the testing facilities that exist in the current checkout and the gaps exposed while bringing the fork up on Linux and Unreal Engine 5.8.1. Its purpose is to ground the next development task: designing a robust suite that a coding agent can run autonomously while developing the client. It deliberately states requirements and open questions rather than selecting the final test architecture.

## Executive summary

- The project has **66 registered Unreal Automation tests**, all in [`WowParserTests.cpp`](../../Source/WowTests/Private/WowParserTests.cpp). Every test uses `IMPLEMENT_SIMPLE_AUTOMATION_TEST` with `EditorContext | ProductFilter`.
- A useful working classification is 12 in-memory unit tests, 41 in-memory component tests, and 13 real-data integration tests. The boundary between “unit” and “component” is descriptive, not encoded in Unreal metadata.
- The 13 data integration tests silently return success with a warning when their MPQ helper cannot find the original author's expected directories. The helper does not consume `-wowdata` or `WOW_DATA`, so an apparently green run can omit every real-data check.
- [`PacketHandlerTest.cpp`](../../Source/WowTests/PacketHandlerTest.cpp) is an ad-hoc compiled probe, not one of the 66 tests: it is not registered with Unreal Automation, contains no assertions, and emits no structured result.
- The repository also contains runtime scenes, timed screenshot hooks, shell launchers, and a server-backed `-testscene=network` smoke harness. These are valuable raw materials. At the research baseline the launchers were macOS- and author-specific, and most observations remain logs or screenshots without a machine-enforced oracle.
- There is currently no checked-in Linux command that runs the `WowUnreal.*` automation namespace unattended, exports a machine-readable report, and returns a trustworthy pass/fail status.

Epic's [Unreal Engine 5.8 Automation Test Framework](https://dev.epicgames.com/documentation/unreal-engine/automation-test-framework-in-unreal-engine) distinguishes API-level unit tests, system-level feature tests, fast smoke tests, content-stress tests, and screenshot comparisons. It also cautions that this framework relies on engine systems and is not ideal for pure unit testing. Those terms should not be collapsed into the project's historical use of “test scene” or “smoke test.”

## Test-level vocabulary for this project

| Level | Meaning here | Current example |
|---|---|---|
| Unit | Deterministic logic exercised in memory with a narrow subject and no MPQ, world, renderer, audio device, or server. | Coordinate and movement conversion tests in [`WowParserTests.cpp`](../../Source/WowTests/Private/WowParserTests.cpp). |
| Component | Several client classes exercised together in an Unreal Editor process, but without entering a rendered world or using a live server. | Lua/UI routing and packet-handler/entity tests in [`WowParserTests.cpp`](../../Source/WowTests/Private/WowParserTests.cpp). |
| Data integration | Real user-supplied WoW 3.3.5a archives are opened and production parsers/stores consume known files. | DBC, BLP, ADT, M2, WMO, audio DBC, character lookup, and MPQ tests. |
| Runtime smoke | A real game/editor process loads a scene and proves a bounded set of runtime invariants without attempting a complete user journey. | Offline `character`, `terrain`, `wmo`, and `ui` test scenes; the server-backed `network` harness. |
| End-to-end server | The production login flow connects to a controlled AzerothCore instance, selects a controlled character, loads the correct world, renders gameplay, accepts input, and exits with assertions and artifacts. | No autonomous test currently covers this entire path. A human successfully entering and playing the world is a manual end-to-end check, not a repeatable automated result. |

Running inside Unreal Editor does not by itself make a test an integration test. Conversely, reading a real MPQ is an integration boundary even when no viewport is created.

## The 66 registered Unreal Automation tests

The current source contains exactly 66 `IMPLEMENT_SIMPLE_AUTOMATION_TEST` registrations under the `WowUnreal.*` namespace. The namespace counts are:

| Namespace | Count | Predominant level |
|---|---:|---|
| `UI` | 24 | Component |
| `Network` | 17 | Component |
| `Entity` | 7 | Unit |
| `Parser` | 6 | Data integration |
| `Coord` | 3 | Unit |
| `Audio` | 3 | Data integration |
| `Character` | 2 | Data integration |
| `Movement` | 2 | Unit |
| `Mpq` | 1 | Data integration |
| `World` | 1 | Data integration |
| **Total** | **66** | |

This gives a practical dependency grouping:

- **12 unit tests:** 3 coordinate, 7 entity, and 2 movement tests.
- **41 component tests:** 24 UI/Lua tests and 17 network/packet tests. These create combinations of Lua VM, event/frame/inventory, connection, socket, entity, or packet-handler objects in memory.
- **13 data integration tests:** the tests listed in the next section.

All 66 are synchronous simple tests: the suite contains no latent commands, map opening, spawned `UWorld`, or Play-In-Editor flow. All use `EditorContext | ProductFilter`; none is tagged as Unreal's `SmokeFilter`, and there are no registered functional, Gauntlet, Automation Driver, or screenshot-comparison tests in the project. The [`WowTests` module](../../Source/WowTests/WowTests.Build.cs) is declared as a default-loaded runtime module in [`WowUnreal.uproject`](../../WowUnreal.uproject), rather than being isolated as a test-only plugin or target.

### Authorship and history

Current `git blame` attributes the complete test file to original author James Clancey. The suite began with [commit `bf4497f`, “Phase 13 automated tests — 13 tests”](https://github.com/miklosme/WowUnreal/commit/bf4497fb3ede062d1a26535a64d8487b3f9ad298) on 2026-03-14, gained 11 audio/network/character/movement/world tests in [commit `9638c35`](https://github.com/miklosme/WowUnreal/commit/9638c357b05589297e58d4eb00e7911773580cbd) later that day, and then grew alongside networking and UI work through 2026-04-08. The commit subjects often record editor builds, automation runs, rendered runs, and screenshots as separate verification activities; that history is evidence that the original workflow did not treat the registered automation suite as sufficient runtime coverage.

The count and classification above describe the current checkout, not the historical “all passing” statements in commit messages. A fresh baseline still needs to be run after the MPQ configuration and unattended runner are made trustworthy.

## The 13 real-MPQ-dependent tests

These tests call the shared `WowTestUtils::GetMpq()` helper at the top of [`WowParserTests.cpp`](../../Source/WowTests/Private/WowParserTests.cpp):

| Test | Production boundary exercised |
|---|---|
| `WowUnreal.Parser.DBC.MapDbc` | Reads and parses `Map.dbc`. |
| `WowUnreal.Parser.DBC.AreaTableDbc` | Reads and parses `AreaTable.dbc`. |
| `WowUnreal.Parser.BLP.DxtTexture` | Reads and parses a known terrain BLP. |
| `WowUnreal.Parser.ADT.Elwynn32_48` | Reads and parses a known Eastern Kingdoms ADT. |
| `WowUnreal.Parser.M2.TreeModel` | Reads and parses a known tree M2 and skin. |
| `WowUnreal.Parser.WMO.GoldshireInn` | Reads and parses a known WMO root. |
| `WowUnreal.Mpq.InitAndRead` | Initializes the archive chain and reads a known DBC. |
| `WowUnreal.Audio.ZoneMusicDbc` | Loads production DBC stores and checks zone-music mappings. |
| `WowUnreal.Audio.SoundEntriesDbc` | Loads production DBC stores and checks sound-entry mappings. |
| `WowUnreal.Audio.SoundAmbienceDbc` | Loads production DBC stores and checks ambience mappings. |
| `WowUnreal.Character.ModelPath` | Uses race/model DBC data and verifies the referenced M2 exists. |
| `WowUnreal.Character.CreatureDisplayLookup` | Resolves creature display/model DBC relationships. |
| `WowUnreal.World.AdtAreaIds` | Parses a known ADT and validates area identifiers. |

`GetMpq()` tries only these locations, once per process:

1. `${UserHome}/Downloads/World of Warcraft 3.3.5a/Data`
2. `${UserHome}/World of Warcraft 3.3.5a/Data`
3. `/Users/clancey/Downloads/World of Warcraft 3.3.5a/Data`

It does **not** parse the project's supported `-wowdata` argument and does not read the globally configured `WOW_DATA` value. If none exists, every dependent test calls `AddWarning(...)` and returns `true`. Some tests also warning-skip when a particular BLP, M2, or WMO path is absent. This behavior makes “not executed” indistinguishable from “passed” to a caller that considers only failure status. If the archive manager does initialize but a required core file such as `Map.dbc` or the known ADT is absent, those particular tests add an error instead.

The next design must decide whether missing external data is a suite-level prerequisite failure, an explicit skip with machine-readable accounting, or a reason not to schedule that test tier. It must not silently turn 13 omitted checks into 13 passes.

## `PacketHandlerTest.cpp` is not an Automation test

[`PacketHandlerTest.cpp`](../../Source/WowTests/PacketHandlerTest.cpp) constructs three packet byte arrays and calls `FWowPacketHandler::HandlePacket` for spell-start, power-update, and aura-update opcodes. It then writes `"Packet handler tests completed"` to the log.

Important limitations:

- It has no `IMPLEMENT_*_AUTOMATION_TEST` registration, `TestTrue`/`TestEqual` checks, or failure result.
- Its static initializer schedules the probe only if `GEngine` and `GWorld` already exist at static-initialization time; otherwise it silently does nothing.
- Even when it runs, “did not crash immediately” and one log line are its only observable outcomes.
- It is not counted among the 66 tests. The registered `WowUnreal.Network.*` tests in `WowParserTests.cpp` are the real automation coverage for packet behavior.

This file should therefore be treated as historical diagnostic scaffolding until the next design decides whether to convert, replace, or remove it.

## Runtime scenes and smoke-test raw materials

The checkout contains ten map assets under [`Content/Maps`](../../Content/Maps): `AnimationTest`, `CharacterTest`, `MobTest`, `ModelViewer`, `NetworkTest`, `StreamingTest`, `TerrainTest`, `UITest`, `WmoTest`, and `WowWorld`. Separately, [`AWowViewerGameMode`](../../Source/WowUnreal/WowViewerGameMode.cpp) recognizes the command-line selectors `character`, `terrain`, `wmo`, `ui`, `login`, and `network`; an absent or unrecognized selector falls back to login. A map named `CharacterTest` and a selector named `-testscene=character` are therefore related concepts, but not interchangeable names.

The map inventory also overstates the implemented harness inventory. [`UWowMapCreatorCommandlet`](../../Source/WowUnreal/WowMapCreatorCommandlet.cpp) assigns `AWowTestGameMode` to all eight `*Test` maps, but the current base [`SetupTestScene()`](../../Source/WowUnreal/WowTestGameMode.cpp) contains named setup only for `CharacterTest`; the other seven fall through to “Base test scene — no additional setup.” `ModelViewer` has its own game-mode subclass. In practice, implemented terrain/WMO/UI/network behavior lives in the command-line selectors on `WowWorld`, not in equivalently named `TerrainTest`, `WmoTest`, `UITest`, or `NetworkTest` maps. Future design must inventory executable behavior rather than inferring coverage from asset filenames.

The offline scene selectors exercise real engine/world behavior and MPQ-backed assets, but they currently rely primarily on human observation and logs. [`AWowWorldManager`](../../Source/WowWorld/Private/WowWorldManager.cpp), [`AWowFlyCamera`](../../Source/WowUnreal/WowFlyCamera.cpp), and [`AWowOrbitCamera`](../../Source/WowUnreal/WowOrbitCamera.cpp) support delayed screenshots; the world manager also supports delayed exit. Screenshot existence proves that a frame was captured, not that terrain, materials, models, UI, or lighting are correct. Unreal 5.8 has a dedicated screenshot-comparison facility, as described by Epic's [Automation Test Framework](https://dev.epicgames.com/documentation/unreal-engine/automation-test-framework-in-unreal-engine), but this project does not currently register a visual comparison test or maintain approved baselines.

### The `network` selector is a partial server integration smoke harness

`-testscene=network` is stronger than the offline scenes in one respect: [`SetupNetworkTestScene`](../../Source/WowUnreal/WowViewerGameMode.cpp) loads saved credentials, authenticates against a real server, selects realm index 0 and character index 0, enters `WorldInGame`, checks for entities and a local player, prints an `ALL PASS`/`SOME FAILED` summary, applies a timeout, and exits automatically.

It is not yet an agent-trustworthy end-to-end test:

- Every completion path uses `RequestExit(false)`, so `SOME FAILED`, missing credentials, and success are not deliberately mapped to distinct process exit codes.
- It depends on saved private credentials, a reachable mutable server, realm 0, and the first character's current state.
- It exercises `UWowConnectionManager` and entity updates directly. It does not go through `AWowLoginController`, initialize terrain for the server-provided map, render gameplay, simulate movement, or verify audio/UI behavior.
- Its results are log conventions rather than structured Unreal Automation results.

It should be classified as a useful **server-backed integration smoke harness**, not dismissed as having no assertions and not promoted to full end-to-end coverage.

## Legacy macOS launch and verification scripts

This section records the state found during the testing inventory. The macOS
launchers have since been replaced by the Linux entry points documented in
[Build and launch commands](../setup/launching.md). The new launchers remove the
hardcoded paths, fixed sleeps, process killing, and informational log greps, but
they intentionally do not claim to be automated tests.

The original scripts encoded useful knowledge about scenes and diagnostic logs,
but were not reproducible Linux automated tests.

| Files | Baseline behavior | Why the result was not autonomous/reproducible |
|---|---|---|
| [`build.sh`](../../build.sh) | Built the editor. | Hardcoded `/Users/clancey/Projects/WowUnreal`, `/Users/Shared/Epic Games/UE_5.7`, Mac target/platform, and destructive clean paths. |
| [`run_game.sh`](../../run_game.sh), [`run_map.sh`](../../run_map.sh) | Launched the game or a named map. | Hardcoded author paths/Mac binaries; process killing and manual lifetime; the root map launcher did not supply the configured WoW-data path. |
| [`run_terrain.sh`](../../run_terrain.sh), [`run_test.sh`](../../run_test.sh) | Slept for a fixed interval, grepped a macOS log, optionally requested a screenshot, then left the game running. | Fixed sleeps raced asynchronous loading; stderr was discarded; grep output was informational and generally did not drive failure status; errors were filtered; screenshots had no visual oracle. |
| [`Scripts/run_map.sh`](../../Scripts/run_map.sh), [`Scripts/run_model_viewer.sh`](../../Scripts/run_model_viewer.sh) | Launched a map, waited, showed selected logs, and checked screenshot existence. | Same author/Mac paths and fixed-delay/log-oracle problems; macOS `stat -f` and `~/Library/Logs` assumptions. |
| [`Scripts/run_character_test.sh`](../../Scripts/run_character_test.sh), [`Scripts/run_mob_test.sh`](../../Scripts/run_mob_test.sh), [`Scripts/run_world.sh`](../../Scripts/run_world.sh) | Were thin wrappers around `Scripts/run_map.sh`. | Inherited all limitations of that launcher. |
| [`test_implementation.sh`](../../test_implementation.sh) | Greps source for body-armor symbol names and prints a historical summary. | Prints checkmarks unconditionally, has no `set -e` or behavioral assertions, and can report a persuasive-looking success even when a grep fails. |

No checked-in script invokes `Automation RunTests WowUnreal`, uses `-ReportExportPath`, or turns Unreal's structured results into a reliable shell exit status. The portable runtime launcher returns the editor process status, but that alone is not an automation-test result. Epic documents both command-line selection and JSON/HTML report export in [Run Automation Tests in Unreal Engine 5.8](https://dev.epicgames.com/documentation/unreal-engine/run-automation-tests-in-unreal-engine). That capability is available, but its exact use and process-result handling for this project remain design work.

The existing [development setup guide](../setup/development.md) already distinguishes build, Unreal automation, offline runtime, server entry, and visual screenshot evidence. The next design should preserve that separation while making the outcomes executable and machine-verifiable.

## Runtime bugs the existing tests did not catch

The Linux/UE 5.8.1 bring-up exposed three instructive failures. Each compiled, coexisted with the existing test coverage, and required a real user-flow or runtime thread to trigger.

### Server map initialization ordering

World entry could reach `WorldInGame` before the login controller had processed the server's `LOGIN_VERIFY_WORLD` map ID. Terrain then initialized with the world manager's default map name (`Azeroth`), even for a Kalimdor character, queued zero matching tiles, and left the loading screen at 10 percent. The fix in [`AWowLoginController`](../../Source/WowUnreal/WowLoginController.cpp) makes world-system initialization wait until both session state and map configuration are ready.

Why coverage missed it: parser tests prove that `Map.dbc` can be read, and network tests prove pieces of session/packet behavior, but no registered test covers the ordering of production login delegates through terrain initialization. The `network` scene bypasses the login controller and terrain entirely.

### Deferred actor destruction and fixed preview names

Pressing Back from character creation destroyed the current preview and immediately returned to character selection. Unreal defers actor destruction until the end of the frame, so spawning another actor with the required fixed name `WowCharacterPreview` could fatally fail. The current code in [`ShowCharacterSelectScreen`](../../Source/WowUnreal/WowLoginController.cpp) requests the name while allowing Unreal to generate a unique replacement.

Why coverage missed it: existing UI/component tests call Lua and widget logic in memory; they do not drive the actual character-creation-to-selection transition in a `UWorld` across frame boundaries.

### Runtime WAV playback on the audio thread

MPQ-backed WAV ambience was constructed as a transient regular `USoundWave`. Under UE 5.8, the audio mixer attempted asset-style streamed platform-data work from its audio task and hit `IsInGameThread()` in `USoundWave::CacheInheritedLoadingBehavior`. [`WowAudioManager`](../../Source/WowWorld/Private/WowAudioManager.cpp) now validates PCM WAV input and queues it through `USoundWaveProcedural`.

Why coverage missed it: the three audio tests validate DBC relationships only. They do not create a sound, start an audio device/mixer source, wait for audio-thread work, or assert crash-free playback.

These incidents argue for coverage at multiple levels. Adding more packet or parser assertions alone cannot detect lifecycle ordering, rendered-world initialization, or thread-affinity failures.

## Requirements for an agent-operable test suite

The next design should produce test evidence a coding agent can interpret without watching a viewport or guessing from a log. At minimum, the design needs to account for:

1. **One documented entry point per test tier.** An agent must be able to discover, build, select, run, and time out tests without author-specific paths or interactive editor steps.
2. **Truthful outcomes.** Assertion failure, crash, timeout, missing prerequisite, explicit skip, and pass must be distinguishable in exit status and machine-readable artifacts. A skipped MPQ tier must never look like 13 passes.
3. **Declared dependencies.** Each test must state whether it needs only compiled code, user-supplied 3.3.5a data, rendering/GPU/audio, or a pinned AzerothCore instance.
4. **Determinism and isolation.** Tests must not depend on execution order, stale `Saved/` state, a developer's first realm/character, shared mutable credentials, wall-clock game state, or fixed sleeps when a readiness condition is available. This also follows Epic's guidance that Automation tests must not assume editor/game state or leave persistent state behind.
5. **Bounded execution.** Every process and asynchronous wait needs a meaningful timeout, cleanup behavior, and crash/hang detection.
6. **Useful failure artifacts.** Preserve focused logs, structured results, crash context, relevant screenshots, server logs/state, and reproduction commands without leaking credentials or Blizzard assets.
7. **Production-path relevance.** Component seams are useful, but at least one tier must exercise production orchestration: login controller, map selection, terrain readiness, entity/player spawn, rendered frames, input/movement, and audio work.
8. **Stable external fixtures.** Real-MPQ expectations need an explicit supported build/locale manifest. Server tests need a pinned AzerothCore revision, reproducible account/character/world state, and a reset strategy.
9. **Cost-aware selection.** Fast in-memory checks should remain cheap enough for every edit; data, rendered, and server tiers need explicit expected duration and scheduling policy.
10. **Regression expressiveness.** The suite must make it practical to encode failures such as callback ordering, deferred UObject/actor lifetimes, and game-thread/audio-thread boundaries—not only parser happy paths.
11. **Agent-readable maintenance rules.** A future agent needs to know which tier to extend for a change, how to avoid brittle log/string assertions, and what evidence is required before calling a gameplay feature complete.

## Research questions for the design task

The next task should answer these before implementation commits to a framework or directory layout:

- Which checks belong in Unreal Simple Automation tests, Automation Specs/CQTest, Functional Testing, Automation Driver, screenshot comparison, Gauntlet, or a small external process supervisor? Epic exposes all of these at different layers; choosing one tool for every level would erase important boundaries. See the [UE 5.8 framework overview](https://dev.epicgames.com/documentation/unreal-engine/automation-test-framework-in-unreal-engine), [Automation Driver](https://dev.epicgames.com/documentation/unreal-engine/automation-driver-in-unreal-engine), and [Gauntlet runner](https://dev.epicgames.com/documentation/unreal-engine/running-gauntlet-tests-in-unreal-engine).
- Should pure coordinate/entity/packet logic be separable from the engine process, or is the current Editor-hosted cost acceptable?
- Can small legally distributable synthetic binary fixtures cover malformed-file and boundary cases while real MPQs remain an explicit local integration tier? Which parser facts require canonical build-12340 data?
- What constitutes a complete and supported `WOW_DATA` fixture, and how should locale/build mismatches be reported before tests start?
- How will a test provision and reset an AzerothCore account, known characters in multiple starting maps, realm configuration, and database state without storing secrets in Git?
- Which world-entry checkpoints are authoritative: protocol state, selected `Map.dbc` internal name, tile queue/load completion, local-player actor spawn, frame presentation, successful movement acknowledgement, or some combination?
- How should input be driven and observed so character creation/back, character selection, movement, targeting, and UI behavior exercise production code without pixel-coordinate brittleness?
- Which visual properties can use deterministic screenshot baselines, which require semantic scene assertions, and how will GPU/driver variance be tolerated?
- Which audio checks can run without physical output, and what is the smallest test that actually schedules mixer/audio-thread work and catches thread-affinity assertions?
- Should server/runtime tests use an uncooked editor game, a packaged Development client, or both? What defects does each environment expose?
- How will expected engine warnings be scoped without broad filters that can hide new failures?
- What runtime and resource budgets allow an autonomous coding loop to run fast checks frequently while still requiring deeper evidence before a task is complete?

The desired outcome of the next task is a written testing design and staged adoption plan grounded in answers to these questions. It should preserve useful existing tests and scenes, but should not assume their current organization, scripts, or pass semantics are the final structure.

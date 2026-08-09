# Build and launch commands

The checked-in shell launchers support the Linux development path. They derive
the repository location from their own files and use the globally configured
`UE_ROOT` and `WOW_DATA` values; no user-specific paths are embedded in them.

Before using them, verify that both variables are available in the current shell:

```bash
test -x "${UE_ROOT}/Engine/Binaries/Linux/UnrealEditor"
test -f "${WOW_DATA}/common.MPQ"
```

## Build

Build the editor target:

```bash
./build.sh
```

Ask UnrealBuildTool to clean the target before rebuilding:

```bash
./build.sh clean
```

The build launcher checks the generated StormLib and Lua archives before invoking
the UE 5.8.1 Linux `WowUnrealEditor` Development target.

## Launch the client

Open the normal login flow:

```bash
./run_game.sh
```

Build first and then open the login flow:

```bash
./run_game.sh --build
```

Use the default saved credential and enter the world with the first character:

```bash
./run_game.sh --autologin
```

Select a saved credential alias explicitly:

```bash
./run_game.sh --autologin --account local
```

The launcher runs UnrealEditor in the foreground. `Ctrl+C` stops that process,
and the editor's exit status is returned to the shell. It does not kill other
Unreal processes or use a repository lock file.

## Offline development scenes

The implemented scene selectors run through the `WowWorld` map and production
viewer game mode. For example:

```bash
./run_terrain.sh
Scripts/run_character_test.sh
Scripts/run_game.sh --scene wmo
Scripts/run_game.sh --scene ui
```

The `character`, `terrain`, `wmo`, and `ui` scenes need `WOW_DATA` but do not
need an AzerothCore connection. The `network` scene and the normal login flow
need a reachable server and saved credentials for unattended login.

Map assets can also be opened directly:

```bash
./run_map.sh ModelViewer
Scripts/run_game.sh --list-maps
```

Map names and `--scene` selectors are different mechanisms. Several historical
`*Test.umap` assets contain only a base scene; prefer the named selectors above
when one exists. `Scripts/run_model_viewer.sh` opens `ModelViewer` at 1600x900.

## Options and Unreal arguments

Use `Scripts/run_game.sh --help` for the complete launcher interface. Common
options include `--resolution 1920x1080`, `--map NAME`, `--scene NAME`, and
`--log PATH`. Unrecognized arguments are forwarded to Unreal. A `--` separator
can make that intent explicit:

```bash
./run_terrain.sh -- -autoscreenshot=terrain.png -autoscreenshotdelay=15
```

The compatibility spelling `build` is accepted by run wrappers, so older commands
such as `Scripts/run_model_viewer.sh build` continue to work.

## Logs

Every launch supplies an absolute Unreal log path. The default is:

```text
Saved/Logs/WowUnreal.log
```

Follow it from another terminal with:

```bash
tail -f Saved/Logs/WowUnreal.log
```

Choose a separate log when comparing sessions:

```bash
./run_game.sh --log Saved/Logs/human-start.log --autologin
```

`run_test.sh` remains only as a compatibility alias for an autologin game launch.
It is not an automated test and does not infer success from log text. See
[Testing foundations](../research/testing-foundations.md) for the current test
inventory and the planned autonomous-suite work.

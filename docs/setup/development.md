# Development prerequisites

## Host tools

The initial supported development path is Linux. Install the host tools used by the vendored dependency builds:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build vulkan-tools
```

Unreal Engine's installed build supplies its own clang and .NET toolchains. An IDE is optional; Git and GitHub CLI are required for this repository's issue workflow.

## Vendored dependencies

The repository contains source for:

- StormLib 9.31 under `Source/ThirdParty/StormLib`
- Lua 5.1.5 under `Source/ThirdParty/lua`
- pugixml 1.15 under `Source/ThirdParty/pugixml`

Pugixml is compiled with the WowUI module. The current Unreal build rules expect these generated archives:

```text
Source/ThirdParty/StormLib/build/libstorm.a
Source/ThirdParty/lua/liblua.a
```

They are intentionally ignored as build artifacts and are not present in a clean clone. Their absence means the local bootstrap must be run; do not download unrelated prebuilt archives.

Build both archives with the compiler and libc++ shipped in the selected Unreal installation:

```bash
Scripts/bootstrap_linux_dependencies.sh
```

The script requires `UE_ROOT`, discovers the matching x86_64 Linux toolchain, uses position-independent code, and compiles StormLib's vendored zlib and bzip2 implementations into `libstorm.a`. It builds only the static Lua library, not the readline-dependent interpreter. It is safe to rerun after dependency or engine-toolchain changes. To validate existing outputs without rebuilding them:

```bash
Scripts/bootstrap_linux_dependencies.sh --check
```

## Build the editor target

After generating both static archives, build the verified Linux editor target with
the repository launcher:

```bash
./build.sh
```

This validates the vendored archives and invokes the equivalent Unreal command:

```bash
"${UE_ROOT}/Engine/Build/BatchFiles/Linux/Build.sh" \
  WowUnrealEditor Linux Development \
  -Project="$(pwd)/WowUnreal.uproject" \
  -WaitMutex
```

Use `./build.sh clean` to ask UnrealBuildTool to clean this target before rebuilding.
The script does not remove `Binaries/` or `Intermediate/` itself. Continue with the
[launch guide](launching.md) after a successful build.

## Verification levels

Use the smallest relevant level and record the command and result:

1. UnrealBuildTool help starts successfully.
2. `WowUnrealEditor` builds without errors.
3. Unreal automation tests pass.
4. The relevant offline map or viewer loads with explicit WoW data.
5. Network changes authenticate and enter the world against the pinned AzerothCore environment.

Visual changes require a rendered screenshot from the Unreal viewport; `-nullrhi` is suitable only for non-visual tests.

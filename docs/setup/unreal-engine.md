# Unreal Engine 5.8.1 on Linux

The fork's target development baseline is the precompiled Unreal Engine 5.8.1 Linux installed build. The project metadata still declares 5.7 until the engine migration is completed; avoid resaving project assets before that migration lands.

## Requirements

Epic's UE 5.8 Linux guidance recommends Ubuntu 22.04 or Rocky Linux 8 for development, 32 GB RAM, a dedicated Vulkan-capable GPU, and NVIDIA driver 570 or newer when using NVIDIA hardware. Ubuntu 26.04 may work but is outside that tested development baseline.

References:

- [Epic Linux development requirements](https://dev.epicgames.com/documentation/unreal-engine/linux-development-requirements-for-unreal-engine?lang=en-US)
- [Epic Linux development quickstart](https://dev.epicgames.com/documentation/unreal-engine/linux-development-quickstart-for-unreal-engine)

## Install the precompiled build

1. Sign in on the [official Unreal Engine for Linux page](https://www.unrealengine.com/en-US/linux).
2. Download the Unreal Engine 5.8.1 Linux ZIP.
3. Extract it outside this repository, for example under `${HOME}/UnrealEngine/UE_5.8.1`.
4. Define the installation path in the shell:

```bash
export UE_ROOT="${HOME}/UnrealEngine/UE_5.8.1"
```

For zsh, place that export in `~/.zshrc` and open a new terminal or run `source ~/.zshrc`.

## Installed-build toolchain

The UE 5.8.1 Linux ZIP already contains Epic's v26 clang 20.1.8 toolchain and .NET SDK. The archive does not contain `Engine/Build/BatchFiles/Linux/SetupToolchain.sh`; do not run it. The included `Setup.sh` also references source-build scripts omitted from the installed build and is not required for this distribution.

Verify the installed build without opening a project:

```bash
"${UE_ROOT}/Engine/Build/BatchFiles/Linux/Build.sh" -help
```

Launch the editor:

```bash
"${UE_ROOT}/Engine/Binaries/Linux/UnrealEditor"
```

Before launching graphically, verify NVIDIA systems with `nvidia-smi` and Vulkan systems with `vulkaninfo --summary`.

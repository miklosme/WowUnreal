# Setup

WowUnreal has a verified Unreal Engine 5.8.1 Linux editor build. Checked-in
scripts cover vendored dependency generation, the editor build, and runtime
launching with user-supplied engine and game-data paths.

Follow the setup documents in this order:

1. [Install and verify Unreal Engine](unreal-engine.md).
2. [Install development prerequisites](development.md).
3. [Provide compatible WoW game data](game-data.md).
4. Choose an [AzerothCore server route](server.md) if testing login or gameplay.
5. Review [runtime configuration](configuration.md).
6. Use the reproducible [build and launch commands](launching.md).

The setup is complete when UnrealBuildTool runs, the two vendored static libraries exist, the editor target builds, an offline viewer reads the supplied MPQs, and—when applicable—the client authenticates against the selected server.

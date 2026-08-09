# Technical specifications

These documents preserve the intended behavior and acceptance boundaries of WowUnreal subsystems. They originated in the upstream implementation workflow, but the specifications themselves remain useful engineering reference.

Specifications are not status reports. A checked box or implementation claim must be confirmed in current source and tests. GitHub Issues hold live work and acceptance state.

## Documents

- [Client specification](client.md) — overall architecture, protocol, and feature scope
- [Architecture overview](overview.md) — subsystem ordering and major milestones
- [Audio](audio.md)
- [Character rendering](character.md)
- [DBC wrappers](dbc-wrappers.md)
- [Lua API](lua-api.md)
- [M2 animation](m2-animation.md)
- [Movement](movement.md)
- [Networking](networking.md)
- [Sky and atmosphere](sky-atmosphere.md)
- [Static mesh migration](static-mesh.md)
- [Terrain LOD](terrain-lod.md)
- [UI and FrameXML](ui-framexml.md)
- [Water](water.md)

## How to use a specification

1. Confirm the terminology and requirements still match current code and the target AzerothCore behavior.
2. Resolve contradictions in a GitHub issue before implementation.
3. Use the acceptance criteria as inputs to automated and visual tests.
4. Follow the current [development workflow](../setup/development.md), not commands embedded in historical text.

## Stable constraints

- WoW protocol and client-data build: 3.3.5a build 12340
- Runtime scripting: Lua 5.1.5
- Game data: user-supplied original MPQs, currently `enUS`
- Target server: AzerothCore 3.3.5a
- Fork engine target: Unreal Engine 5.8.1 after migration

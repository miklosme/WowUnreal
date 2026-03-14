# Player Movement & Camera

## Goal
Implement player character movement (walk, run, jump, swim, fly) with third-person camera and server synchronization.

## Context
- Depends on: `networking.md` (movement packets), `m2-animation.md` (character animation)
- Current camera: fly camera for viewer mode (`WowViewerPlayerController`)
- No player movement or chase camera yet
- Reference: `azerothcore-wotlk/src/server/game/Movement/` for movement flags/speeds

## Requirements

### Ground Movement
1. WASD movement: forward, backward, strafe left/right
2. Walk/run toggle (default: run, `/` to toggle walk)
3. Movement speeds from server (base run = 7.0 units/sec, walk = 2.5)
4. Terrain following — character stays on terrain surface
5. Slope limit — can't walk up steep slopes (> ~50 degrees)
6. Backpedal speed = 60% of forward speed

### Jump
1. Spacebar triggers jump
2. Parabolic arc: initial upward velocity, gravity pulls down
3. Can move horizontally while jumping
4. Fall damage calculation on landing (height > ~15 yards)

### Swimming
1. Detect water via MH2O data (from water.md)
2. When submerged: swimming movement mode (free 3D movement)
3. Breath timer when underwater (3 minutes, then drowning damage)
4. Swim speed = ~67% of run speed

### Flying
1. Only in Outland/Northrend zones with flying mount
2. Ascend/descend with spacebar/X or mouse + forward
3. Flying mount speed tiers (60%, 150%, 280%, 310%)

### Collision
1. Character capsule collider
2. Terrain collision (walk on terrain mesh)
3. WMO/building collision (can't walk through walls)
4. Doodad collision for solid objects (optional, lower priority)

### Server Sync
1. Send movement packets (CMSG_MOVE_*) on state changes
2. Heartbeat position sync every 500ms while moving
3. Include: position, orientation, movement flags, fall time, timestamp
4. Handle server position corrections (teleport to server position if diverged)

### Third-Person Camera
1. Camera orbits behind and above player character
2. **Left mouse drag** — turns character (character faces camera direction)
3. **Right mouse drag** — orbits camera without turning character
4. **Scroll wheel** — zoom in/out (min ~2 yards, max ~30 yards)
5. Camera collision — zoom in when terrain/buildings block view
6. Smooth interpolation on all camera movements
7. First-person at minimum zoom (camera at head position)

### Input Mapping
1. WASD — movement
2. Space — jump / swim up / ascend
3. X — swim down / descend
4. Mouse — camera control (see above)
5. Left click — interact/target
6. Right click — auto-run toggle (with both mouse buttons)
7. Num Lock — auto-run toggle

## Architecture

### New/Modified Files
- `WowClient/WowPlayerController.h/.cpp` — Replaces viewer controller for gameplay
- `WowClient/WowPlayerMovement.h/.cpp` — Movement state machine + physics
- `WowClient/WowCameraManager.h/.cpp` — Third-person camera logic
- `WowNetwork/` — Movement packet send/receive

### Movement State Machine
```
States: Idle, Walking, Running, Jumping, Falling, Swimming, Flying, Dead
Transitions triggered by: input, terrain contact, water detection, mount
```

## Acceptance Criteria
- [ ] Builds without errors
- [ ] WASD moves character across terrain
- [ ] Character follows terrain height (doesn't float or sink)
- [ ] Jump works with proper arc
- [ ] Third-person camera orbits behind player
- [ ] Scroll wheel zooms camera
- [ ] Camera doesn't clip through terrain
- [ ] Movement packets sent to server (visible in server log)
- [ ] Screenshot shows character on terrain with chase camera perspective

## Verification Steps
1. `./run_test.sh build`
2. `./run_test.sh` — login, enter world
3. Use WASD to move, verify terrain following
4. Jump with spacebar
5. Right-drag to orbit camera
6. Screenshot from gameplay perspective (behind character)

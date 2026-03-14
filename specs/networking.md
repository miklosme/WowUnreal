# Networking & Entity System

## Goal
Implement packet handlers for world state synchronization and the entity system that tracks all objects (players, NPCs, items, game objects) in the world.

## Context
- Auth + world socket done in `Source/WowNetwork/`
- Encrypted packet framing works (ARC4-drop1024)
- No packet handler dispatch or entity system yet
- Reference: `azerothcore-wotlk/src/server/game/Server/` for packet structures
- Reference: `azerothcore-wotlk/src/server/game/Entities/` for update fields
- Test server: `127.0.0.1:3724` (auth), `:8085` (world)

## Requirements

### Packet Dispatch
1. Opcode → handler function map for all SMSG packets
2. Handler receives opcode + raw payload buffer
3. Thread-safe: network thread parses, game thread processes
4. Decompression for SMSG_COMPRESSED_UPDATE_OBJECT (zlib)

### Entity System (GUIDs)
1. 64-bit GUIDs with packed type/entry: `GUID = (type << 48) | (entry << 24) | low`
2. GUID types: Player, Unit, Item, Container, GameObject, DynamicObject, Corpse, Pet, Vehicle
3. Entity registry: `TMap<uint64, TSharedPtr<FWowEntity>>` — lookup by GUID
4. Entity base class with typed subclasses matching WoW object hierarchy

### UPDATE_OBJECT Handler (the big one)
1. Parse `SMSG_UPDATE_OBJECT` — contains create/update/destroy blocks
2. Block types: VALUES (partial field update), MOVEMENT (position), CREATE_OBJECT, CREATE_OBJECT2, OUT_OF_RANGE, NEAR_OBJECTS
3. **Update fields** — bitmask-based partial updates:
   - OBJECT_FIELD_* (base: GUID, type, entry, scale)
   - UNIT_FIELD_* (health, power, level, faction, displayID, flags, stats)
   - PLAYER_FIELD_* (XP, money, inventory slots, quest log)
   - ITEM_FIELD_* (owner, stack count, charges, enchantments)
4. Movement info: position (x,y,z,o), velocity, movement flags, transport GUID, swim/fly state
5. ~1400 total update fields across all object types

### Priority Packet Handlers (Tier 1)
| Opcode | Handler |
|--------|---------|
| SMSG_AUTH_RESPONSE | Login result |
| SMSG_CHAR_ENUM | Character list |
| SMSG_LOGIN_VERIFY_WORLD | Initial position |
| SMSG_UPDATE_OBJECT | Entity create/update |
| SMSG_COMPRESSED_UPDATE_OBJECT | Compressed entity updates |
| SMSG_DESTROY_OBJECT | Entity removal |
| MSG_MOVE_* (all) | Movement sync |
| SMSG_MESSAGECHAT | Chat messages |
| SMSG_INITIAL_SPELLS | Known spells |
| SMSG_ACTION_BUTTONS | Action bar |

### Client → Server (CMSG)
1. Movement packets: CMSG_MOVE_START_FORWARD, STOP, STRAFE, JUMP, etc.
2. CMSG_PLAYER_LOGIN — enter world with character
3. CMSG_CAST_SPELL, CMSG_ATTACK_SWING
4. CMSG_MESSAGECHAT — send chat
5. Heartbeat: periodic position updates

## Architecture

### New Files
- `WowNetwork/WowPacketHandler.h/.cpp` — Dispatch table, handler base
- `WowNetwork/WowEntityManager.h/.cpp` — Entity registry, GUID lookup
- `WowNetwork/WowEntity.h` — Base entity + typed subclasses
- `WowNetwork/WowUpdateFields.h` — Field index enums (generated from azerothcore defs)
- `WowNetwork/Handlers/` — Individual handler files grouped by system

## Acceptance Criteria
- [ ] Builds without errors
- [ ] Connects to test server and receives SMSG_AUTH_RESPONSE
- [ ] SMSG_CHAR_ENUM parses character list
- [ ] SMSG_LOGIN_VERIFY_WORLD provides spawn position
- [ ] UPDATE_OBJECT creates entity entries in registry
- [ ] Player position updates from server reflected in entity state
- [ ] Log shows "Received X entities, Y updates" type messages
- [ ] Screenshot shows game running after login (even if no entities rendered yet)

## Verification Steps
1. `./run_test.sh build`
2. `./run_test.sh` — auto-login to test server
3. Check log for packet handler messages
4. Verify entity count in log matches expected NPCs/objects near spawn

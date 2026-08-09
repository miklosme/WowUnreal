# Audio System

## Goal
Play zone-appropriate music, ambient sounds, and sound effects from MPQ data files.

## Context
- MPQ reading works — audio files are MP3/WAV inside MPQ
- No audio system exists yet
- DBC tables: `SoundEntries.dbc`, `ZoneMusic.dbc`, `ZoneIntroMusicTable.dbc`, `SoundAmbience.dbc`
- Audio files in MPQ: `Sound/Music/`, `Sound/Ambience/`, `Sound/Spells/`, `Sound/Character/`
- UE5 audio: `USoundWave`, `UAudioComponent`, `USoundCue`

## Requirements

### Music System
1. Parse `ZoneMusic.dbc` — maps zone IDs to music file sets (day/night variants)
2. Parse `SoundEntries.dbc` — maps sound IDs to file paths in MPQ
3. Load MP3 from MPQ → `USoundWave` at runtime
4. Play zone music when entering a zone
5. Crossfade between tracks (2-3 second fade)
6. Day/night music variants
7. Combat music (detected via PLAYER_REGEN_DISABLED event)
8. Music volume controlled by settings

### Ambient Sounds
1. Parse `SoundAmbience.dbc` — ambient sound sets per zone
2. Day/night ambient variants (birds during day, crickets at night)
3. Indoor/outdoor switching
4. Ambient loop with randomized variant playback
5. Weather-related ambience (rain sound during rain weather)

### Sound Effects
1. UI sounds: button click, bag open/close, quest accept/complete jingle
2. Spell cast/impact sounds (from `SpellVisual.dbc` → `SoundEntries.dbc`)
3. Footstep sounds (terrain-type dependent — grass, stone, dirt, snow)
4. Weapon swing/hit sounds
5. 3D positional audio for world sounds (campfires, waterfalls)
6. NPC greeting voice lines on interaction

### MP3 → USoundWave Pipeline
1. Extract MP3 bytes from MPQ
2. Decode MP3 to PCM (UE5 handles MP3 import, or use a decoder)
3. Create `USoundWave` with PCM data
4. Cache loaded sounds (LRU, ~100MB budget for audio cache)

## Architecture

### New Files
- `WowAudio/WowAudioManager.h/.cpp` — Central audio controller
- `WowAudio/WowMusicPlayer.h/.cpp` — Zone music, crossfading
- `WowAudio/WowAmbiencePlayer.h/.cpp` — Ambient sound loops
- `WowAudio/WowSoundFactory.h/.cpp` — MP3 from MPQ → USoundWave
- `WowData/Formats/Dbc/ZoneMusicDbc.h` — ZoneMusic.dbc wrapper
- `WowData/Formats/Dbc/SoundEntriesDbc.h` — SoundEntries.dbc wrapper
- `WowData/Formats/Dbc/SoundAmbienceDbc.h` — SoundAmbience.dbc wrapper

## Acceptance Criteria
- [ ] Builds without errors
- [ ] MP3 files load from MPQ and play through UE5 audio
- [ ] Zone music plays when in Elwynn Forest
- [ ] Music changes when moving to a different zone
- [ ] Crossfade between tracks (no abrupt cut)
- [ ] Ambient sounds play (birds, wind)
- [ ] Volume controllable (even if just via code/cvar for now)
- [ ] Screenshot + log verify: "Playing music: Sound/Music/ZoneMusic/Forest/..." in log

## Verification Steps
1. Build the editor target using [development setup](../setup/development.md)
2. Launch the relevant map using [development setup](../setup/development.md) — fly around Elwynn Forest
3. Listen for music and ambient sounds
4. Fly to Westfall — music should change
5. Check log for audio file load messages

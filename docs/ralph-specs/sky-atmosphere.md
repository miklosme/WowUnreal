# Sky & Atmosphere System

## Goal
Implement zone-based outdoor lighting with sky colors, fog, sun/moon, and day/night cycle driven by Light.dbc data.

## Context
- DBC generic reader exists in `Source/WowData/Public/Formats/DbcParser.h`
- No sky/atmosphere system exists yet
- Light data lives in: `Light.dbc`, `LightParams.dbc`, `LightIntParams.dbc`, `LightFloatParams.dbc`
- Reference: `noggit3/src/noggit/` for light interpolation logic
- Reference: `pywowlib/file_formats/` for DBC field layouts

## Requirements

### DBC Parsing
1. **Light.dbc** — Light sources positioned in world space with falloff radii
   - Fields: ID, mapID, x, y, z, falloffStart, falloffEnd, paramID[8] (one per time-of-day band)
2. **LightParams.dbc** — Links to int/float param sets + sky/cloud textures
   - Fields: ID, highlightSky, lightSkyboxID, glow, waterShallowAlpha, waterDeepAlpha, oceanShallowAlpha, oceanDeepAlpha
3. **LightIntParams.dbc** — Color values for 18 lighting properties across time-of-day
   - 18 rows per LightParams entry (one per property: sky top color, sky middle, horizon, fog, sun, etc.)
   - Each row has values at time intervals that get interpolated
4. **LightFloatParams.dbc** — Float values (fog distance, cloud density)

### Time-of-Day Interpolation
1. 24-hour cycle with configurable speed (default: 1 game-minute = 1 real-second → 24 min full cycle, or match server time)
2. Interpolate all 18 color bands based on current time
3. Smooth transitions between day/night/dawn/dusk

### Sky Rendering
1. Skydome or sky material with gradient bands:
   - Sky top color
   - Sky middle color (band 1, 2, 3)
   - Horizon color
   - Fog color (matches horizon for seamless blend)
2. Sun disc positioned by time of day
3. Moon disc (opposite sun)
4. Cloud layers (scrolling texture with alpha from DBC)

### Zone-Based Blending
1. When player is within a Light.dbc entry's falloff radius, blend toward that light's colors
2. Multiple overlapping lights blend by distance weight
3. Default/global light for areas outside any specific light zone

### Fog
1. Distance fog color = horizon/fog color from DBC
2. Fog start/end distances from LightFloatParams
3. Integrates with UE5 ExponentialHeightFog or atmospheric fog

### UE5 Integration
1. Use `UDirectionalLightComponent` for sun/moon (rotate based on time)
2. Use `USkyLightComponent` for ambient fill (color from DBC)
3. Use `USkyAtmosphereComponent` or custom skydome mesh
4. Use `UExponentialHeightFogComponent` for distance fog

## Architecture

### New Files
- `WowData/Formats/DbcWrappers/LightDbc.h` — Light.dbc typed wrapper
- `WowData/Formats/DbcWrappers/LightParamsDbc.h` — LightParams wrapper
- `WowData/Formats/DbcWrappers/LightIntParamsDbc.h` — Color interpolation data
- `WowRenderer/WowSkyManager.h/.cpp` — Sky dome, sun/moon, time-of-day
- `WowRenderer/WowLightManager.h/.cpp` — Zone light blending, fog control

## Acceptance Criteria
- [ ] Builds without errors
- [ ] Light.dbc, LightParams, LightIntParams parse correctly
- [ ] Sky colors change based on time of day
- [ ] Sun/moon visible and move across sky
- [ ] Fog color matches horizon
- [ ] Screenshot at dawn shows warm orange/pink sky gradient
- [ ] Screenshot at night shows dark sky with moon
- [ ] Zone transitions blend lighting smoothly

## Verification Steps
1. `./run_test.sh build`
2. `./run_test.sh` — fly in Elwynn Forest
3. Add debug command to set time of day (e.g., 6:00 dawn, 12:00 noon, 21:00 dusk, 0:00 midnight)
4. Screenshot at each time — verify sky gradient, sun position, fog color

#!/bin/bash
# Launch WowUnreal with a specific map
# Usage: ./run_map.sh <MapName> [build]
#   ./run_map.sh WowWorld           - launch main game map
#   ./run_map.sh TerrainTest build  - build then launch terrain test
#   ./run_map.sh CharacterTest      - launch character test scene
#
# Available maps: WowWorld, TerrainTest, CharacterTest, AnimationTest,
#                 MobTest, WmoTest, UITest, NetworkTest, StreamingTest

set -e

MAP_NAME="${1:-WowWorld}"
BUILD_FLAG="$2"

PROJECT_DIR="/Users/clancey/Projects/WowUnreal"
UE_DIR="/Users/Shared/Epic Games/UE_5.7"

# Kill any existing instance
pkill -f "UnrealEditor.*WowUnreal.*-game" 2>/dev/null || true
sleep 1

# Build if requested
if [ "$BUILD_FLAG" = "build" ]; then
    echo "Building..."
    "$UE_DIR/Engine/Build/BatchFiles/Mac/Build.sh" WowUnrealEditor Mac Development \
        -project="$PROJECT_DIR/WowUnreal.uproject" 2>&1 | tail -5
    echo ""
fi

MAP_PATH="/Game/Maps/$MAP_NAME"
echo "Launching map: $MAP_PATH"

# WowWorld with autologin needs networking, others are test maps
EXTRA_ARGS=""
if [ "$MAP_NAME" = "WowWorld" ]; then
    EXTRA_ARGS="-autologin"
fi

"$UE_DIR/Engine/Binaries/Mac/UnrealEditor" \
    "$PROJECT_DIR/WowUnreal.uproject" \
    "$MAP_PATH" \
    -game -log -windowed -resx=1280 -resy=720 -nosplash \
    $EXTRA_ARGS \
    2>/dev/null &

GAME_PID=$!
echo "Game PID: $GAME_PID"
echo "Press Ctrl+C to stop."
wait $GAME_PID 2>/dev/null

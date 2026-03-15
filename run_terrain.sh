#!/bin/bash
# Launch straight into terrain viewing (skips login screen)
# Usage: ./run_terrain.sh [build]
#   ./run_terrain.sh         - just run
#   ./run_terrain.sh build   - build then run

set -e

PROJECT_DIR="/Users/clancey/Projects/WowUnreal"
UE_DIR="/Users/Shared/Epic Games/UE_5.7"
WOW_DATA="/Users/clancey/Downloads/World of Warcraft 3.3.5a/Data"
LOG_DIR="$HOME/Library/Logs/WowUnreal"

# Kill any existing instance
pkill -f "UnrealEditor.*WowUnreal.*-game" 2>/dev/null || true
sleep 1

# Build if requested
if [ "$1" = "build" ]; then
    echo "Building..."
    "$UE_DIR/Engine/Build/BatchFiles/Mac/Build.sh" WowUnrealEditor Mac Development \
        -project="$PROJECT_DIR/WowUnreal.uproject" 2>&1 | tail -5
    echo ""
fi

# Find latest log file
find_log() {
    ls -t "$LOG_DIR"/WowUnreal*.log 2>/dev/null | head -1
}

SCREENSHOT_DIR="$PROJECT_DIR/Saved/Screenshots"
mkdir -p "$SCREENSHOT_DIR"
SCREENSHOT_PATH="$SCREENSHOT_DIR/terrain_verify.png"

echo "Launching terrain viewer above Northshire Abbey..."
"$UE_DIR/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$PROJECT_DIR/WowUnreal.uproject" \
    -game -log -windowed -resx=1280 -resy=720 \
    "-wowdata=$WOW_DATA" \
    -autologin -startpos \
    "-autoscreenshot=$SCREENSHOT_PATH" \
    -autoscreenshotdelay=30 \
    -ExecCmds="r.VolumetricFog 0,r.Fog 0" \
    2>/dev/null &

GAME_PID=$!
echo "Game PID: $GAME_PID"
echo "Waiting for terrain to load..."
sleep 35

LOG_FILE=$(find_log)
if [ -z "$LOG_FILE" ]; then
    echo "No log file found!"
    exit 1
fi
echo "Log: $LOG_FILE"

echo ""
echo "=== Terrain ==="
grep -i "sections.*textured\|Loaded terrain material\|Tile.*textured" "$LOG_FILE" 2>/dev/null | tail -5
echo ""
echo "=== Entities ==="
grep -i "Spawned player model\|Spawned creature model\|Destroyed entity model\|OnEntityCreated\|entity.*GUID" "$LOG_FILE" 2>/dev/null | tail -10
echo ""
echo "=== Objects ==="
grep -i "Spawned WMO\|Instanced.*doodad\|Skipping large" "$LOG_FILE" 2>/dev/null | head -10
echo ""
echo "=== Errors ==="
grep -i "fatal\|error.*metal\|crash\|error.*wow\|warning.*failed\|Failed to compile" "$LOG_FILE" 2>/dev/null | grep -v "LogConfig\|LogInit\|LogAudio\|LogSlate\|CrashGUID\|CrashReport\|LogStreaming\|r.GPU\|LogHLSL\|LogRHI\|LogWowXml" | head -10

echo ""
echo "=== Screenshot ==="
if [ -f "$SCREENSHOT_PATH" ]; then
    SIZE=$(stat -f%z "$SCREENSHOT_PATH" 2>/dev/null || echo "0")
    echo "Screenshot saved: $SCREENSHOT_PATH ($SIZE bytes)"
else
    echo "Screenshot not yet saved (may appear after more frames render)"
fi

echo ""
echo "Game running with fly camera. Use WASD + mouse to fly, scroll to adjust speed."
echo "Press Ctrl+C to stop."
wait $GAME_PID 2>/dev/null

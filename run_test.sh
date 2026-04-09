#!/bin/bash
# Quick test script for WoW Unreal client
# Usage: ./run_test.sh [build]
#   ./run_test.sh         - just run (skip build)
#   ./run_test.sh build   - build then run

set -e

PROJECT_DIR="/Users/clancey/Projects/WowUnreal"
UE_DIR="/Users/Shared/Epic Games/UE_5.7"
WOW_DATA="/Users/clancey/Downloads/World of Warcraft 3.3.5a/Data"
LOG_FILE="$HOME/Library/Logs/WowUnreal/WowUnreal.log"

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

# Clear old log
rm -f "$LOG_FILE"

echo "Launching game..."
"$UE_DIR/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$PROJECT_DIR/WowUnreal.uproject" \
    -game -log -windowed -resx=1280 -resy=720 \
    "-wowdata=$WOW_DATA" \
    -autologin \
    2>/dev/null &

GAME_PID=$!
echo "Game PID: $GAME_PID"
echo "Waiting for load..."
sleep 30

# Show key log lines
echo ""
echo "=== Terrain ==="
grep -i "sections.*textured\|merged mesh" "$LOG_FILE" 2>/dev/null | head -10
echo ""
echo "=== Objects ==="
grep -i "Spawned WMO\|doodad.*spawn\|Skipping large" "$LOG_FILE" 2>/dev/null | head -10
echo ""
echo "=== Errors ==="
grep -i "fatal\|error.*metal\|crash\|error.*wow\|warning.*failed" "$LOG_FILE" 2>/dev/null | grep -v "LogConfig\|LogInit\|LogAudio\|LogSlate\|CrashGUID\|CrashReport\|LogStreaming\|r.GPU" | head -10
echo ""
echo "=== Player ==="
grep "Teleported" "$LOG_FILE" 2>/dev/null

echo ""
echo "Game running. Use WASD + mouse to fly. Press Ctrl+C to stop."
echo "Log: $LOG_FILE"
wait $GAME_PID 2>/dev/null

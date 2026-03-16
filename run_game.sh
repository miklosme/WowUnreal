#!/bin/bash
# Launch WoW client with login screen (like the original client)
# Usage: ./run_game.sh [build]
#   ./run_game.sh         - just run
#   ./run_game.sh build   - build then run

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
        "$PROJECT_DIR/WowUnreal.uproject" -SkipDeploy 2>&1 | tail -5
    echo ""
fi

echo "Launching World of Warcraft..."
"$UE_DIR/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$PROJECT_DIR/WowUnreal.uproject" \
    "/Game/Maps/WowWorld" \
    -game -log -windowed -resx=1280 -resy=720 \
    "-wowdata=$WOW_DATA" \
    2>/dev/null &

GAME_PID=$!
echo "Game PID: $GAME_PID"
echo ""
echo "Login screen should appear. Log in to connect to your server."
echo "Press Ctrl+C to quit."
echo ""

# Tail the log if it appears
sleep 5
LOG_FILE=$(ls -t "$LOG_DIR"/WowUnreal*.log 2>/dev/null | head -1)
if [ -n "$LOG_FILE" ]; then
    echo "Log: $LOG_FILE"
fi

wait $GAME_PID 2>/dev/null

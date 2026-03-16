#!/bin/bash
# Launch WoW client with login screen (like the original client)
# Usage: ./run_game.sh [options]
#   ./run_game.sh                  - show login screen (credentials prefilled)
#   ./run_game.sh --autologin      - auto-login and enter world with first character
#   ./run_game.sh --build          - build first, then launch
#   ./run_game.sh --build --autologin  - build, then auto-login

set -e

PROJECT_DIR="/Users/clancey/Projects/WowUnreal"
UE_DIR="/Users/Shared/Epic Games/UE_5.7"
WOW_DATA="/Users/clancey/Downloads/World of Warcraft 3.3.5a/Data"
LOG_DIR="$HOME/Library/Logs/WowUnreal"

BUILD=false
AUTOLOGIN=false
EXTRA_ARGS=""

for arg in "$@"; do
    case "$arg" in
        --build|build) BUILD=true ;;
        --autologin|autologin) AUTOLOGIN=true ;;
        *) EXTRA_ARGS="$EXTRA_ARGS $arg" ;;
    esac
done

# Kill any existing instance
pkill -f "UnrealEditor.*WowUnreal.*-game" 2>/dev/null || true
sleep 1

# Build if requested
if [ "$BUILD" = true ]; then
    echo "Building..."
    "$UE_DIR/Engine/Build/BatchFiles/Mac/Build.sh" WowUnrealEditor Mac Development \
        "$PROJECT_DIR/WowUnreal.uproject" -SkipDeploy 2>&1 | tail -5
    echo ""
fi

if [ "$AUTOLOGIN" = true ]; then
    EXTRA_ARGS="$EXTRA_ARGS -autologin"
    echo "Launching with auto-login..."
else
    echo "Launching World of Warcraft..."
fi

"$UE_DIR/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$PROJECT_DIR/WowUnreal.uproject" \
    "/Game/Maps/WowWorld" \
    -game -log -windowed -resx=1280 -resy=720 \
    "-wowdata=$WOW_DATA" \
    $EXTRA_ARGS \
    2>/dev/null &

GAME_PID=$!
echo "Game PID: $GAME_PID"
echo "Press Ctrl+C to quit."
echo ""

sleep 5
LOG_FILE=$(ls -t "$LOG_DIR"/WowUnreal*.log 2>/dev/null | head -1)
if [ -n "$LOG_FILE" ]; then
    echo "Log: $LOG_FILE"
fi

wait $GAME_PID 2>/dev/null

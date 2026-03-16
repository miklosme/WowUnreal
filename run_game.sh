#!/bin/bash
# Launch WoW client with login screen (like the original client)
# Usage: ./run_game.sh [options]
#   ./run_game.sh                      - show login screen (credentials prefilled)
#   ./run_game.sh --autologin          - auto-login and enter world with first character
#   ./run_game.sh --build              - build first, then launch
#   ./run_game.sh --createchar         - create a new character and enter world
#   ./run_game.sh --map ModelViewer    - launch a specific map
#
# Only one instance can run at a time. Kill existing before launching.

set -e

PROJECT_DIR="/Users/clancey/Projects/WowUnreal"
UE_DIR="/Users/Shared/Epic Games/UE_5.7"
WOW_DATA="/Users/clancey/Downloads/World of Warcraft 3.3.5a/Data"
LOG_DIR="$HOME/Library/Logs/WowUnreal"
LOCKFILE="$PROJECT_DIR/Saved/.game_running.lock"

BUILD=false
MAP="WowWorld"
EXTRA_ARGS=""

for arg in "$@"; do
    case "$arg" in
        --build|build) BUILD=true ;;
        --autologin|autologin) EXTRA_ARGS="$EXTRA_ARGS -autologin" ;;
        --createchar|createchar) EXTRA_ARGS="$EXTRA_ARGS -createchar" ;;
        --map) shift 2>/dev/null; ;; # handled below
        *) EXTRA_ARGS="$EXTRA_ARGS $arg" ;;
    esac
done

# Parse --map separately (needs next arg)
while [[ $# -gt 0 ]]; do
    case "$1" in
        --map) MAP="$2"; shift 2 ;;
        *) shift ;;
    esac
done

# Check for existing instance
if [ -f "$LOCKFILE" ]; then
    EXISTING_PID=$(cat "$LOCKFILE" 2>/dev/null)
    if kill -0 "$EXISTING_PID" 2>/dev/null; then
        echo "ERROR: Game already running (PID $EXISTING_PID, map may differ)."
        echo "Kill it first: kill $EXISTING_PID"
        exit 1
    else
        rm -f "$LOCKFILE"
    fi
fi

# Kill any stale instances
pkill -f "UnrealEditor.*WowUnreal.*-game" 2>/dev/null || true
sleep 1

# Build if requested
if [ "$BUILD" = true ]; then
    echo "Building..."
    "$UE_DIR/Engine/Build/BatchFiles/Mac/Build.sh" WowUnrealEditor Mac Development \
        "$PROJECT_DIR/WowUnreal.uproject" -SkipDeploy 2>&1 | tail -5
    echo ""
fi

echo "Launching: $MAP"
"$UE_DIR/Engine/Binaries/Mac/UnrealEditor.app/Contents/MacOS/UnrealEditor" \
    "$PROJECT_DIR/WowUnreal.uproject" \
    "/Game/Maps/$MAP" \
    -game -log -windowed -resx=1280 -resy=720 \
    "-wowdata=$WOW_DATA" \
    -ExecCmds="r.Shadow.Virtual.Enable 0,r.VolumetricFog 0,r.Fog 0,r.Lumen.DiffuseIndirect.Allow 0,r.Lumen.Reflections.Allow 0" \
    $EXTRA_ARGS \
    2>/dev/null &

GAME_PID=$!
echo "$GAME_PID" > "$LOCKFILE"

# Clean up lockfile on exit
trap "rm -f '$LOCKFILE'" EXIT

echo "Game PID: $GAME_PID"
echo "Press Ctrl+C to quit."
echo ""

sleep 5
LOG_FILE=$(ls -t "$LOG_DIR"/WowUnreal*.log 2>/dev/null | head -1)
if [ -n "$LOG_FILE" ]; then
    echo "Log: $LOG_FILE"
fi

wait $GAME_PID 2>/dev/null

#!/bin/bash
# Build everything for WowUnreal
# Usage: ./build.sh [clean]
#   ./build.sh         - incremental build
#   ./build.sh clean   - clean then build

set -e

PROJECT_DIR="/Users/clancey/Projects/WowUnreal"
UE_DIR="/Users/Shared/Epic Games/UE_5.7"
BUILD="$UE_DIR/Engine/Build/BatchFiles/Mac/Build.sh"

# Kill running game instances to release build mutex
pkill -f "UnrealEditor.*WowUnreal.*-game" 2>/dev/null || true

if [ "$1" = "clean" ]; then
    echo "=== Cleaning ==="
    rm -rf "$PROJECT_DIR/Binaries"
    rm -rf "$PROJECT_DIR/Intermediate"
    echo "Cleaned Binaries/ and Intermediate/"
    echo ""
fi

echo "=== Building Editor (Development) ==="
"$BUILD" WowUnrealEditor Mac Development \
    -project="$PROJECT_DIR/WowUnreal.uproject" -waitmutex 2>&1 | tail -20

echo ""
echo "=== Build Complete ==="

#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
PROJECT_FILE="${REPO_ROOT}/WowUnreal.uproject"

# shellcheck source=Scripts/load_env.sh
source "${SCRIPT_DIR}/load_env.sh"

usage() {
    cat <<'EOF'
Build the WowUnreal editor target on Linux.

Usage:
  Scripts/build.sh [clean]

Environment:
  UE_ROOT   Path to the Unreal Engine 5.8.1 Linux installed build.

Variables are read from the repository-root .env file when not already set in
the environment.

The optional clean mode asks UnrealBuildTool to clean the editor target before
building it. It does not delete repository directories directly.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

MODE=build
case "${1:-}" in
    "") ;;
    clean|--clean) MODE=clean ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        fail "unknown argument: $1"
        ;;
esac

[[ $# -le 1 ]] || fail "too many arguments"
[[ "$(uname -s)" == Linux ]] || fail "the supported build path is Linux"
[[ -n "${UE_ROOT:-}" ]] || fail "UE_ROOT is not set (define it in .env); see docs/setup/unreal-engine.md"

UE_ROOT="$(realpath -e -- "${UE_ROOT}")" || fail "UE_ROOT does not exist: ${UE_ROOT}"
BUILD_TOOL="${UE_ROOT}/Engine/Build/BatchFiles/Linux/Build.sh"

[[ -x "${BUILD_TOOL}" ]] || fail "Unreal build tool is not executable: ${BUILD_TOOL}"
[[ -f "${PROJECT_FILE}" ]] || fail "project file not found: ${PROJECT_FILE}"

"${SCRIPT_DIR}/bootstrap_linux_dependencies.sh" --check

BUILD_ARGS=(
    WowUnrealEditor
    Linux
    Development
    "-Project=${PROJECT_FILE}"
    -WaitMutex
)

if [[ "${MODE}" == clean ]]; then
    printf 'Cleaning WowUnrealEditor (Linux Development)...\n'
    "${BUILD_TOOL}" "${BUILD_ARGS[@]}" -Clean
fi

printf 'Building WowUnrealEditor (Linux Development)...\n'
exec "${BUILD_TOOL}" "${BUILD_ARGS[@]}"

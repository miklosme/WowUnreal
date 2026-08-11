#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"
PROJECT_FILE="${REPO_ROOT}/WowUnreal.uproject"

# shellcheck source=Scripts/load_env.sh
source "${SCRIPT_DIR}/load_env.sh"
MAP_NAME=WowWorld
TEST_SCENE=
WINDOW_WIDTH=1280
WINDOW_HEIGHT=720
LOG_PATH="${REPO_ROOT}/Saved/Logs/WowUnreal.log"
BUILD_FIRST=false
DRY_RUN=false
RUN_DOCTOR=true
EXTRA_ARGS=()

usage() {
    cat <<'EOF'
Launch WowUnreal through the Unreal Engine Linux editor.

Usage:
  Scripts/run_game.sh [options] [-- Unreal arguments...]

Options:
  --build                 Build the editor target before launching.
  --map NAME              Open Content/Maps/NAME.umap (default: WowWorld).
  --scene NAME            Select login, character, terrain, wmo, ui, or network.
  --autologin             Use the configured default saved credential.
  --createchar            Create a Human Mage when the account has no character.
  --startpos              Start with the terrain fly camera near Northshire.
  --account ALIAS         Select a saved credential alias for autologin.
  --resolution WIDTHxHEIGHT
                          Set window dimensions (default: 1280x720).
  --log PATH              Set the Unreal log path (default: Saved/Logs/WowUnreal.log).
  --list-maps             List checked-in map assets and exit.
  --no-doctor             Skip the server health check (Scripts/doctor.sh).
                          Useful for offline scenes that need no server.
  --dry-run               Validate configuration and print the command only.
  -h, --help              Show this help.

Environment:
  UE_ROOT                 Path to the UE 5.8.1 Linux installed build.
  WOW_DATA                Path to a WoW 3.3.5a build-12340 Data directory.
  WOW_SERVER_HOST         Server host checked by Scripts/doctor.sh.
  WOW_AUTH_PORT           Auth port checked by Scripts/doctor.sh.
  WOW_WORLD_PORT          World port checked by Scripts/doctor.sh.

Variables are read from the repository-root .env file when not already set in
the environment.

Unknown arguments are passed through to Unreal. Use `--` when an argument could
be mistaken for a launcher option.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

need_value() {
    [[ $# -ge 2 && -n "$2" ]] || fail "$1 requires a value"
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --build|build)
            BUILD_FIRST=true
            shift
            ;;
        --map)
            need_value "$1" "${2:-}"
            MAP_NAME="$2"
            shift 2
            ;;
        --map=*)
            MAP_NAME="${1#*=}"
            [[ -n "${MAP_NAME}" ]] || fail "--map requires a value"
            shift
            ;;
        --scene)
            need_value "$1" "${2:-}"
            TEST_SCENE="$2"
            shift 2
            ;;
        --scene=*)
            TEST_SCENE="${1#*=}"
            [[ -n "${TEST_SCENE}" ]] || fail "--scene requires a value"
            shift
            ;;
        --autologin|autologin)
            EXTRA_ARGS+=(-autologin)
            shift
            ;;
        --createchar|createchar)
            EXTRA_ARGS+=(-createchar)
            shift
            ;;
        --startpos|startpos)
            EXTRA_ARGS+=(-startpos)
            shift
            ;;
        --account)
            need_value "$1" "${2:-}"
            EXTRA_ARGS+=("-account=$2")
            shift 2
            ;;
        --account=*)
            ACCOUNT_ALIAS="${1#*=}"
            [[ -n "${ACCOUNT_ALIAS}" ]] || fail "--account requires a value"
            EXTRA_ARGS+=("-account=${ACCOUNT_ALIAS}")
            shift
            ;;
        --resolution)
            need_value "$1" "${2:-}"
            RESOLUTION="$2"
            shift 2
            if [[ ! "${RESOLUTION}" =~ ^([1-9][0-9]*)x([1-9][0-9]*)$ ]]; then
                fail "invalid resolution '${RESOLUTION}'; expected WIDTHxHEIGHT"
            fi
            WINDOW_WIDTH="${BASH_REMATCH[1]}"
            WINDOW_HEIGHT="${BASH_REMATCH[2]}"
            ;;
        --resolution=*)
            RESOLUTION="${1#*=}"
            shift
            if [[ ! "${RESOLUTION}" =~ ^([1-9][0-9]*)x([1-9][0-9]*)$ ]]; then
                fail "invalid resolution '${RESOLUTION}'; expected WIDTHxHEIGHT"
            fi
            WINDOW_WIDTH="${BASH_REMATCH[1]}"
            WINDOW_HEIGHT="${BASH_REMATCH[2]}"
            ;;
        --log)
            need_value "$1" "${2:-}"
            LOG_PATH="$2"
            shift 2
            ;;
        --log=*)
            LOG_PATH="${1#*=}"
            [[ -n "${LOG_PATH}" ]] || fail "--log requires a value"
            shift
            ;;
        --list-maps)
            find "${REPO_ROOT}/Content/Maps" -maxdepth 1 -type f -name '*.umap' \
                -printf '%f\n' | sed 's/\.umap$//' | sort
            exit 0
            ;;
        --no-doctor)
            RUN_DOCTOR=false
            shift
            ;;
        --dry-run)
            DRY_RUN=true
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        --)
            shift
            EXTRA_ARGS+=("$@")
            break
            ;;
        *)
            EXTRA_ARGS+=("$1")
            shift
            ;;
    esac
done

[[ "$(uname -s)" == Linux ]] || fail "the supported launcher path is Linux"
[[ -n "${UE_ROOT:-}" ]] || fail "UE_ROOT is not set (define it in .env); see docs/setup/unreal-engine.md"
[[ -n "${WOW_DATA:-}" ]] || fail "WOW_DATA is not set (define it in .env); see docs/setup/game-data.md"

UE_ROOT="$(realpath -e -- "${UE_ROOT}")" || fail "UE_ROOT does not exist: ${UE_ROOT}"
WOW_DATA="$(realpath -e -- "${WOW_DATA}")" || fail "WOW_DATA does not exist: ${WOW_DATA}"
EDITOR="${UE_ROOT}/Engine/Binaries/Linux/UnrealEditor"

[[ -x "${EDITOR}" ]] || fail "UnrealEditor is not executable: ${EDITOR}"
[[ -f "${PROJECT_FILE}" ]] || fail "project file not found: ${PROJECT_FILE}"
[[ -f "${WOW_DATA}/common.MPQ" ]] || fail "WOW_DATA is not a supported Data directory (common.MPQ is missing): ${WOW_DATA}"
[[ -f "${WOW_DATA}/enUS/locale-enUS.MPQ" ]] || fail "WOW_DATA is missing the required enUS locale archive: ${WOW_DATA}/enUS/locale-enUS.MPQ"

MAP_NAME="${MAP_NAME#/Game/Maps/}"
MAP_NAME="${MAP_NAME%.umap}"
[[ "${MAP_NAME}" =~ ^[A-Za-z0-9_]+$ ]] || fail "invalid map name: ${MAP_NAME}"
[[ -f "${REPO_ROOT}/Content/Maps/${MAP_NAME}.umap" ]] || {
    printf 'Available maps:\n' >&2
    find "${REPO_ROOT}/Content/Maps" -maxdepth 1 -type f -name '*.umap' \
        -printf '  %f\n' | sed 's/\.umap$//' | sort >&2
    fail "map not found: ${MAP_NAME}"
}

if [[ -n "${TEST_SCENE}" ]]; then
    TEST_SCENE="${TEST_SCENE,,}"
    case "${TEST_SCENE}" in
        login|character|terrain|wmo|ui|network) ;;
        *) fail "unknown scene '${TEST_SCENE}'; expected login, character, terrain, wmo, ui, or network" ;;
    esac
    EXTRA_ARGS+=("-testscene=${TEST_SCENE}")
fi

if [[ "${RUN_DOCTOR}" == true && "${DRY_RUN}" == false ]]; then
    "${SCRIPT_DIR}/doctor.sh" \
        || fail "the server is not functioning; fix the issues above or pass --no-doctor to launch without a server"
fi

if [[ "${BUILD_FIRST}" == true ]]; then
    "${SCRIPT_DIR}/build.sh"
fi

if [[ "${LOG_PATH}" != /* ]]; then
    LOG_PATH="${REPO_ROOT}/${LOG_PATH}"
fi
LOG_PATH="$(realpath -m -- "${LOG_PATH}")"
mkdir -p -- "$(dirname -- "${LOG_PATH}")"

COMMAND=(
    "${EDITOR}"
    "${PROJECT_FILE}"
    "/Game/Maps/${MAP_NAME}"
    -game
    -log
    -windowed
    "-resx=${WINDOW_WIDTH}"
    "-resy=${WINDOW_HEIGHT}"
    -nosplash
    "-wowdata=${WOW_DATA}"
    "-abslog=${LOG_PATH}"
    "${EXTRA_ARGS[@]}"
)

printf 'Map: %s\n' "${MAP_NAME}"
[[ -z "${TEST_SCENE}" ]] || printf 'Scene: %s\n' "${TEST_SCENE}"
printf 'Log: %s\n' "${LOG_PATH}"

if [[ "${DRY_RUN}" == true ]]; then
    printf 'Command:'
    printf ' %q' "${COMMAND[@]}"
    printf '\n'
    exit 0
fi

exec "${COMMAND[@]}"

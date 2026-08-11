#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"

# shellcheck source=Scripts/load_env.sh
source "${SCRIPT_DIR}/load_env.sh"

STORM_SOURCE="${REPO_ROOT}/Source/ThirdParty/StormLib"
STORM_BUILD="${STORM_SOURCE}/build"
STORM_ARCHIVE="${STORM_BUILD}/libstorm.a"
LUA_SOURCE="${REPO_ROOT}/Source/ThirdParty/lua"
LUA_ARCHIVE="${LUA_SOURCE}/liblua.a"

usage() {
    cat <<'EOF'
Build the Linux static dependencies required by WowUnreal.

Usage:
  Scripts/bootstrap_linux_dependencies.sh [--check]

Environment:
  UE_ROOT             Required path to an Unreal Engine Linux installed build.
  UE_TOOLCHAIN_ROOT   Optional explicit target toolchain directory.
  JOBS                Optional parallel build count.

Variables are read from the repository-root .env file when not already set in
the environment.

The script discovers the x86_64 target toolchain inside UE_ROOT, builds StormLib
and Lua from the vendored sources, and verifies the resulting archives. Generated
objects and archives are ignored by Git.
EOF
}

fail() {
    printf 'error: %s\n' "$*" >&2
    exit 1
}

require_command() {
    command -v "$1" >/dev/null 2>&1 || fail "required command not found: $1"
}

validate_archive() {
    local archive="$1"
    local label="$2"

    [[ -s "${archive}" ]] || fail "${label} archive is missing or empty: ${archive}"
    "${AR_TOOL}" t "${archive}" | grep -q . || fail "${label} archive has no members: ${archive}"
}

MODE=build
case "${1:-}" in
    "") ;;
    --check) MODE=check ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        fail "unknown argument: $1"
        ;;
esac

[[ "$(uname -s)" == Linux ]] || fail "this bootstrap currently supports Linux only"
[[ "$(uname -m)" == x86_64 ]] || fail "this bootstrap currently supports x86_64 only"
[[ -n "${UE_ROOT:-}" ]] || fail "UE_ROOT is not set (define it in .env); see docs/setup/unreal-engine.md"

UE_ROOT="$(realpath -e -- "${UE_ROOT}")" || fail "UE_ROOT does not exist: ${UE_ROOT}"
[[ -x "${UE_ROOT}/Engine/Build/BatchFiles/Linux/Build.sh" ]] || fail "UE_ROOT is not an Unreal Engine Linux installed build: ${UE_ROOT}"

if [[ -n "${UE_TOOLCHAIN_ROOT:-}" ]]; then
    TOOLCHAIN_ROOT="$(realpath -e -- "${UE_TOOLCHAIN_ROOT}")" || fail "UE_TOOLCHAIN_ROOT does not exist: ${UE_TOOLCHAIN_ROOT}"
else
    SDK_ROOT="${UE_ROOT}/Engine/Extras/ThirdPartyNotUE/SDKs/HostLinux/Linux_x64"
    [[ -d "${SDK_ROOT}" ]] || fail "Unreal Linux SDK directory not found: ${SDK_ROOT}"

    shopt -s nullglob
    TOOLCHAIN_CANDIDATES=("${SDK_ROOT}"/*/x86_64-unknown-linux-gnu)
    shopt -u nullglob

    ((${#TOOLCHAIN_CANDIDATES[@]} == 1)) || fail "expected one x86_64 Unreal toolchain under ${SDK_ROOT}; found ${#TOOLCHAIN_CANDIDATES[@]}. Set UE_TOOLCHAIN_ROOT explicitly."
    TOOLCHAIN_ROOT="${TOOLCHAIN_CANDIDATES[0]}"
fi

CC_TOOL="${TOOLCHAIN_ROOT}/bin/clang"
CXX_TOOL="${TOOLCHAIN_ROOT}/bin/clang++"
AR_TOOL="${TOOLCHAIN_ROOT}/bin/llvm-ar"
RANLIB_TOOL="${TOOLCHAIN_ROOT}/bin/llvm-ranlib"
LIBCXX_INCLUDE="${TOOLCHAIN_ROOT}/include/c++/v1"

for tool in "${CC_TOOL}" "${CXX_TOOL}" "${AR_TOOL}" "${RANLIB_TOOL}"; do
    [[ -x "${tool}" ]] || fail "Unreal toolchain executable not found: ${tool}"
done
[[ -d "${LIBCXX_INCLUDE}" ]] || fail "Unreal libc++ headers not found: ${LIBCXX_INCLUDE}"

printf 'Unreal Engine: %s\n' "${UE_ROOT}"
printf 'Linux toolchain: %s\n' "${TOOLCHAIN_ROOT}"
"${CXX_TOOL}" --version | sed -n '1p'

if [[ "${MODE}" == check ]]; then
    validate_archive "${STORM_ARCHIVE}" StormLib
    validate_archive "${LUA_ARCHIVE}" Lua
    printf 'Dependencies are ready.\n'
    exit 0
fi

require_command cmake
require_command ninja
require_command make

JOBS="${JOBS:-$(getconf _NPROCESSORS_ONLN 2>/dev/null || printf '1')}"
[[ "${JOBS}" =~ ^[1-9][0-9]*$ ]] || fail "JOBS must be a positive integer: ${JOBS}"

[[ -f "${STORM_SOURCE}/CMakeLists.txt" ]] || fail "vendored StormLib source is incomplete"
[[ -f "${LUA_SOURCE}/Makefile" ]] || fail "vendored Lua source is incomplete"

printf '\nBuilding StormLib...\n'
UE_CXX_FLAGS="-fPIC -nostdinc++ -isystem${TOOLCHAIN_ROOT}/include -isystem${LIBCXX_INCLUDE}"
cmake \
    -S "${STORM_SOURCE}" \
    -B "${STORM_BUILD}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER="${CC_TOOL}" \
    -DCMAKE_CXX_COMPILER="${CXX_TOOL}" \
    -DCMAKE_AR="${AR_TOOL}" \
    -DCMAKE_RANLIB="${RANLIB_TOOL}" \
    -DCMAKE_C_FLAGS="-fPIC" \
    -DCMAKE_CXX_FLAGS="${UE_CXX_FLAGS}" \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DBUILD_SHARED_LIBS=OFF \
    -DSTORM_BUILD_TESTS=OFF \
    -DSTORM_SKIP_INSTALL=ON \
    -DSTORM_USE_BUNDLED_LIBRARIES=ON
cmake --build "${STORM_BUILD}" --target storm --parallel "${JOBS}"

printf '\nBuilding Lua...\n'
make -C "${LUA_SOURCE}" clean
make \
    -C "${LUA_SOURCE}" \
    -j "${JOBS}" \
    a \
    CC="${CC_TOOL}" \
    AR="${AR_TOOL} rcu" \
    RANLIB="${RANLIB_TOOL}" \
    CFLAGS="-O2 -Wall -fPIC -DLUA_USE_POSIX"

validate_archive "${STORM_ARCHIVE}" StormLib
validate_archive "${LUA_ARCHIVE}" Lua

printf '\nDependencies are ready:\n'
printf '  %s\n' "${STORM_ARCHIVE}" "${LUA_ARCHIVE}"

#!/usr/bin/env bash

set -Eeuo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
REPO_ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd -P)"

# shellcheck source=Scripts/load_env.sh
source "${SCRIPT_DIR}/load_env.sh"

WOW_SERVER_HOST="${WOW_SERVER_HOST:-127.0.0.1}"
WOW_AUTH_PORT="${WOW_AUTH_PORT:-3724}"
WOW_WORLD_PORT="${WOW_WORLD_PORT:-8085}"
AZEROTHCORE_DIR="${AZEROTHCORE_DIR:-$(realpath -m -- "${REPO_ROOT}/../azerothcore-wotlk")}"
CREDENTIALS_FILE="${REPO_ROOT}/Saved/WowCredentials.json"

usage() {
    cat <<'EOF'
Check the health of the WoW server the client connects to.

Usage:
  Scripts/doctor.sh [-h|--help]

Environment:
  WOW_SERVER_HOST   Auth/world server host (default: 127.0.0.1).
  WOW_AUTH_PORT     Authentication port (default: 3724).
  WOW_WORLD_PORT    World/realm port (default: 8085).
  AZEROTHCORE_DIR   Local AzerothCore checkout with the Docker stack
                    (default: ../azerothcore-wotlk beside this repository).

Variables are read from the repository-root .env file when not already set in
the environment.

Exits 0 when the server is reachable; exits 1 with fix instructions otherwise.
EOF
}

case "${1:-}" in
    "") ;;
    -h|--help)
        usage
        exit 0
        ;;
    *)
        usage >&2
        printf 'error: unknown argument: %s\n' "$1" >&2
        exit 1
        ;;
esac

PROBLEMS=0
FIXES=()

ok()   { printf '  ok    %s\n' "$*"; }
warn() { printf '  warn  %s\n' "$*"; }
bad()  { printf '  FAIL  %s\n' "$*"; PROBLEMS=$((PROBLEMS + 1)); }
fix()  { FIXES+=("$*"); }

probe_tcp() {
    timeout 3 bash -c ": >/dev/tcp/$1/$2" 2>/dev/null
}

is_local_host() {
    case "${WOW_SERVER_HOST}" in
        127.*|localhost|::1) return 0 ;;
        *) return 1 ;;
    esac
}

container_state() {
    docker inspect -f '{{.State.Status}} {{.State.ExitCode}}' "$1" 2>/dev/null \
        || printf 'absent -\n'
}

printf 'WowUnreal doctor — server %s (auth port %s, world port %s)\n' \
    "${WOW_SERVER_HOST}" "${WOW_AUTH_PORT}" "${WOW_WORLD_PORT}"

AUTH_UP=false
WORLD_UP=false

if probe_tcp "${WOW_SERVER_HOST}" "${WOW_AUTH_PORT}"; then
    AUTH_UP=true
    ok "auth server accepts connections on ${WOW_SERVER_HOST}:${WOW_AUTH_PORT}"
else
    bad "auth server is not reachable on ${WOW_SERVER_HOST}:${WOW_AUTH_PORT}"
fi

if probe_tcp "${WOW_SERVER_HOST}" "${WOW_WORLD_PORT}"; then
    WORLD_UP=true
    ok "world server accepts connections on ${WOW_SERVER_HOST}:${WOW_WORLD_PORT}"
else
    bad "world server is not reachable on ${WOW_SERVER_HOST}:${WOW_WORLD_PORT}"
fi

if [[ "${AUTH_UP}" == false || "${WORLD_UP}" == false ]]; then
    if ! is_local_host; then
        fix "The server address is remote. Verify WOW_SERVER_HOST, WOW_AUTH_PORT, and WOW_WORLD_PORT in .env, confirm the remote server is running, and test connectivity with: nc -zv ${WOW_SERVER_HOST} ${WOW_AUTH_PORT}"
    elif ! command -v docker >/dev/null 2>&1; then
        fix "Docker is not installed or not on PATH. For the containerized server route, install Docker Engine and the Compose plugin, then follow docs/setup/server.md. For a native server, start its authserver and worldserver manually."
    else
        DB_STATE="$(container_state ac-database)"
        AUTH_STATE="$(container_state ac-authserver)"
        WORLD_STATE="$(container_state ac-worldserver)"

        printf '  info  containers: ac-database=%s ac-authserver=%s ac-worldserver=%s\n' \
            "${DB_STATE%% *}" "${AUTH_STATE%% *}" "${WORLD_STATE%% *}"

        if [[ "${DB_STATE%% *}" == absent && "${AUTH_STATE%% *}" == absent && "${WORLD_STATE%% *}" == absent ]]; then
            if [[ -d "${AZEROTHCORE_DIR}" ]]; then
                fix "The AzerothCore containers do not exist yet. Build and start the stack (the first run compiles the server and can take a long time): cd ${AZEROTHCORE_DIR} && docker compose up -d --build"
            else
                fix "No AzerothCore checkout found at ${AZEROTHCORE_DIR}. Clone the pinned version and start the stack as described in docs/setup/server.md, or set AZEROTHCORE_DIR in .env to your existing checkout."
            fi
        else
            STOPPED=()
            for name in ac-database ac-authserver ac-worldserver; do
                state="$(container_state "${name}")"
                [[ "${state%% *}" == running ]] || STOPPED+=("${name}")
            done
            if ((${#STOPPED[@]} > 0)); then
                fix "Stopped or missing containers: ${STOPPED[*]}. Start the stack with: cd ${AZEROTHCORE_DIR} && docker compose start (or 'docker compose up -d' if some containers are missing)"
            else
                fix "The containers are running but the ports are not accepting connections yet — the worldserver can take a while to start. Watch its progress with: docker logs -f ac-worldserver"
            fi
        fi

        for name in ac-db-import ac-client-data-init; do
            state="$(container_state "${name}")"
            if [[ "${state%% *}" == exited && "${state##* }" != 0 ]]; then
                bad "one-shot container ${name} exited with code ${state##* }"
                fix "Initialization container ${name} failed. Inspect it with: docker logs ${name}. See docs/setup/server.md for the first-run expectations."
            fi
        done
    fi
fi

if [[ -f "${CREDENTIALS_FILE}" ]]; then
    ok "saved credentials file exists (Saved/WowCredentials.json)"
else
    warn "Saved/WowCredentials.json is missing — --autologin will not work; see docs/setup/configuration.md"
fi

if ((PROBLEMS == 0)); then
    printf 'Server is healthy.\n'
    exit 0
fi

printf '\nHow to fix:\n'
i=1
for entry in "${FIXES[@]}"; do
    printf '  %d. %s\n' "${i}" "${entry}"
    i=$((i + 1))
done
exit 1

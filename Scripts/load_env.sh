#!/usr/bin/env bash

# Sourced by the launcher and build scripts. Loads variables such as UE_ROOT
# and WOW_DATA from the repository-root .env file. Variables already present
# in the environment keep their values, so one-off overrides like
# `UE_ROOT=/elsewhere Scripts/build.sh` still win over .env.
#
# Expects REPO_ROOT to be set by the sourcing script.

load_env_file() {
    local env_file="$1"
    [[ -f "${env_file}" ]] || return 0

    local key
    local preserved=()
    while IFS= read -r key; do
        if [[ -n "${!key+x}" ]]; then
            preserved+=("${key}=${!key}")
        fi
    done < <(sed -nE 's/^[[:space:]]*(export[[:space:]]+)?([A-Za-z_][A-Za-z0-9_]*)=.*/\2/p' "${env_file}")

    set -a
    # shellcheck disable=SC1090
    source "${env_file}"
    set +a

    local entry
    for entry in "${preserved[@]}"; do
        export "${entry?}"
    done
}

load_env_file "${REPO_ROOT}/.env"

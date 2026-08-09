#!/usr/bin/env bash

set -Eeuo pipefail

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
printf 'note: run_test.sh is a compatibility launcher, not an automated test; see docs/research/testing-foundations.md\n' >&2
exec "${REPO_ROOT}/Scripts/run_game.sh" --autologin "$@"

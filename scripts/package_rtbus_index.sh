#!/usr/bin/env sh
# SPDX-License-Identifier: MPL-2.0
#
# Generate the local Arduino Boards Manager package index for RTBus RTDuo.

set -eu

SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
REPO_ROOT=$(CDPATH= cd -- "$SCRIPT_DIR/.." && pwd)

BASE_URL=${1:-http://127.0.0.1:8000}

cd "$REPO_ROOT"
python3 scripts/package_arduino.py \
	--output-dir dist/arduino \
	--index-file package_rtbus_index.json \
	--base-url "$BASE_URL"

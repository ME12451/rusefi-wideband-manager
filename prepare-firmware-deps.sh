#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${WIDEBAND_DEP_SOURCE:=$PWD/../epicefi/firmware/controllers/can/wideband_firmware}"
for dependency in ChibiOS libfirmware; do
  case "$dependency" in
    ChibiOS) expected=a8865dc84b114e74f62d8e470458839da71d333b ;;
    libfirmware) expected=39b3a04dab617e7ff24577e99d0d7fac2ce44c9f ;;
  esac
  source_path="$WIDEBAND_DEP_SOURCE/firmware/$dependency"
  actual=$(git -C "$source_path" rev-parse HEAD)
  test "$actual" = "$expected" || { echo "Wrong $dependency revision: $actual" >&2; exit 1; }
  target="firmware-f0/firmware/$dependency"
  if ! test -e "$target"; then ln -s "$source_path" "$target"; fi
  test "$(git -C "$target" rev-parse HEAD)" = "$expected"
done

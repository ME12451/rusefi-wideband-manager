#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
for dependency in ChibiOS libfirmware; do
  case "$dependency" in
    ChibiOS) expected=a8865dc84b114e74f62d8e470458839da71d333b ;;
    libfirmware) expected=39b3a04dab617e7ff24577e99d0d7fac2ce44c9f ;;
  esac
  target="firmware-f0/firmware/$dependency"
  if ! test -e "$target"; then
    if test -n "${WIDEBAND_DEP_SOURCE:-}"; then
      source_path="$WIDEBAND_DEP_SOURCE/firmware/$dependency"
      actual=$(git -C "$source_path" rev-parse HEAD)
      test "$actual" = "$expected" || { echo "Wrong $dependency revision: $actual" >&2; exit 1; }
      ln -s "$(realpath "$source_path")" "$target"
    else
      case "$dependency" in
        ChibiOS) url=https://github.com/ChibiOS/ChibiOS.git ;;
        libfirmware) url=https://github.com/rusefi/libfirmware.git ;;
      esac
      git clone --no-checkout "$url" "$target"
      git -C "$target" checkout --detach "$expected"
    fi
  fi
  test "$(git -C "$target" rev-parse HEAD)" = "$expected"
done

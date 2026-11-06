#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${CC_WINDOWS:=x86_64-w64-mingw32-gcc}"
: "${OUT_DIR:=$PWD/../artifacts/wideband-manager-0.5.3}"
mkdir -p "$OUT_DIR"
"$CC_WINDOWS" -std=c11 -O2 -Wall -Wextra -Werror -Wno-cast-function-type -static -mwindows src/main.c -o "$OUT_DIR/rusEFI-Wideband-Manager.exe" -lcomctl32 -lcomdlg32 -lgdi32

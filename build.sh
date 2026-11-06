#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${CC_WINDOWS:=x86_64-w64-mingw32-gcc}"
mkdir -p ../artifacts/wideband-manager-0.5.3
"$CC_WINDOWS" -std=c11 -O2 -Wall -Wextra -Werror -Wno-cast-function-type -static -mwindows src/main.c -o ../artifacts/wideband-manager-0.5.3/rusEFI-Wideband-Manager.exe -lcomctl32 -lcomdlg32 -lgdi32

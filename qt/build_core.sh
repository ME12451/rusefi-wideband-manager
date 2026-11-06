#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cc -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-function -fPIC -shared core.c -lm -o libwideband.so
if test -n "${CC_WINDOWS:-}"; then
 "$CC_WINDOWS" -std=c11 -O2 -Wall -Wextra -Werror -Wno-unused-function -shared -static-libgcc core.c -lm -o libwideband.dll
fi

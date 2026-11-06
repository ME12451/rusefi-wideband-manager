#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
test_build=$(mktemp -d /tmp/wb-desktop-tests.XXXXXX)
trap 'rm -rf "$test_build"' EXIT
export ASAN_OPTIONS=detect_leaks=0
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_protocol.c -o "$test_build/protocol"
"$test_build/protocol"
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=address,undefined tests/test_analytics.c -o "$test_build/analytics"
"$test_build/analytics"
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=address,undefined tests/test_trace_math.c -lm -o "$test_build/trace"
"$test_build/trace"

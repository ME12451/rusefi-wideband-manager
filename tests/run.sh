#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
builddir=$(mktemp -d /tmp/wb-manager-tests.XXXXXX)
trap 'rm -rf "$builddir"' EXIT
export ASAN_OPTIONS=detect_leaks=0
cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_protocol.c -o "$builddir/protocol"
"$builddir/protocol" firmware-f0/firmware/build/wideband_image.bin
cc -std=c11 -Wall -Wextra -Werror -Wno-unused-function -fsanitize=address,undefined tests/test_analytics.c -o "$builddir/analytics"
"$builddir/analytics"
python3 tests/test_service.py
python3 tests/test_client_flow.py
python3 firmware-f0/test/test_can_bitrate.py
python3 firmware-f0/test/test_software_calibration.py
python3 firmware-f0/test/test_sensor_selection.py
python3 firmware-f0/test/test_manager_snapshot.py

#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
: "${ARM_TOOLCHAIN:=$PWD/../build-tools/arm-gnu-toolchain-14.3.rel1-x86_64-arm-none-eabi/bin}"
export PATH="$ARM_TOOLCHAIN:$PWD/tools:$PWD/../build-tools/bin:$PATH"
cd firmware-f0/firmware/boards/f0_module
# This is the standalone wideband submodule build, not the ECU build system.
export USE_OPT='-Os -ggdb -fomit-frame-pointer -falign-functions=1 -fsingle-precision-constant -fstack-usage -fcallgraph-info=su'
export USE_PROCESS_STACKSIZE=0x400
export USE_EXCEPTIONS_STACKSIZE=0x400
bash ./build_wideband.sh

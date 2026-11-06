# rusEFI Wideband Manager 0.6 — Windows and Linux

One Qt/PySide6 application, one CAN service/update workflow and the existing shared C protocol/analytics core. Select **English / Deutsch** at the top right. Language switches immediately and is saved per user. Unsaved inputs, live data and paused history remain intact. CSV schemas and backup files are language-independent. Driver/OS diagnostics may retain their original technical text.

## Run

Windows x64: extract the whole ZIP and launch `rusEFI-Wideband-Manager.exe`. Keep `app` and `runtime` beside it. Python and Qt are included. PCAN still requires the PEAK driver and 64-bit PCANBasic DLL (official installation or DLL beside the EXE). Target: updated Windows 10/11. Tested through WineHQ 11.21; no physical Windows hardware tests.

Linux x86_64: extract the whole archive and run `./rusEFI-Wideband-Manager`. Keep `_internal` beside it. Python and Qt are included. This binary package includes Ubuntu 26.04 system libraries and requires **glibc 2.43 or newer**. Tested on Ubuntu 26.04.1/X11. Older distributions need a rebuild from the included common source. Wayland and multi-monitor DPI remain untested.

Use `--demo` for clearly labeled simulated values with all CAN transmission actions blocked. Normal startup contains no synthetic measurements.

## Features

Live cards; multi-module dashboard; six selectable traces (lambda, temperature, heater %, pump PWM %, Nernst DC V, ESR Ohm); synchronized plots; 15/30/60/300-second windows; fixed/auto scales; cursor; pause, history, zoom and CSV. Space pauses, Home resumes, wheel or +/- zooms, drag or Shift+wheel pans. Up to 10 Hz and 3000 points per index; timestamps are PC elapsed time, not module uptime. Acquisition continues while the graph is paused; CSV exports the visible window/frozen snapshot. Invalid/stale/missing values are not plotted as valid continuous traces.

F0 detect/read and verified configuration writes; saved/active bitrate; standard/diagnostic TX flags; selectable Rtrim; Windows/Linux-compatible WBF0 v2 CRC backups and verified restore; LED identification, restart and factory defaults; extended diagnostic snapshots; legacy settings; raw CAN monitor/CSV, ID/STD/EXT filters, events/warmup and statistics exports; F0 CAN firmware update; 50-ms broadcast bench heater control with ECU-sender/error stop.

Only CAN communication with the module: no EGT, AEMNet, analog-output or TunerStudio interface. SLCAN's serial link is only the CAN adapter transport.

Windows adapters: PCAN via python-can/PCANBasic or local SLCAN port such as COM3. Linux: SocketCAN such as can0 or local SLCAN such as /dev/ttyUSB0. Configure SocketCAN bitrate externally before connecting; the application does not invoke sudo or change network interface configuration. The bitrate dropdown configures PCAN/direct SLCAN only. Direct SLCAN uses pyserial at 115200 baud/8N1 with checked Lawicel initialization. Local port permission is required; do not share it with slcand/other tools. No remote serial URLs. SLCAN throughput can limit recording on busy buses.

## Module configuration and firmware

Hardware IDs must be unique; duplicate IDs cannot be reliably distinguished. Extended replies are matched by hardware ID, opcode and transaction. Detect/read before writing. Legacy ACKs provide no sender identity/readback. Do not use concurrent configuration tools.

Software Rtrim must use your own sensor's labeled 30.00..300.00 Ohm value, never a guessed value. Software mode requires the physical trim resistor to be absent. Sensor, hardware and selected mode must match. Decimal entries accept dot or comma.

Backups save the last confirmed readback, not unsaved fields. Restoring uses two verified writes; errors can leave partial changes. A non-Bosch backup temporarily restores trim in normal Bosch mode, then applies and verifies the final sensor/configuration. Configure on the bench.

Firmware updates use broadcast bootloader commands: connect exactly one compatible F0 module and keep power stable. Only a 25600-byte application BIN with expected vectors and CRC is accepted. Full/bootloader images are rejected. No automatic retries after erase/data failure. Transfer acknowledgements do not provide flash readback; check Pong/live data after restart.

**No module firmware was built or changed for 0.6.** The graph and language dropdown require no flashing. Extended configuration uses the same compatible F0 Manager firmware 0.4 as before. Existing application SHA256:
`9f03e1734c03e658f9715469c975fd756b23b80628ad8ae24b46cbee34855cd3`.

## Validation

Shared C warning-clean builds and ASAN/UBSAN protocol/analytics/trace tests. Common client tested against actual F0 dispatcher/config functions with a fake HAL, including writes/readback, Rtrim, non-Bosch restore, defaults, wrong replies, flash failure and no retry. Virtual CAN and SLCAN pseudo-terminal adapter tests. Complete firmware transfer and failure flows against a simulated bootloader.

Each packaged Windows/Linux application passes 28 demo and 18 offline GUI checks: language changes, saved preference, preserved unsaved inputs/history, identical paused CSV across languages, language change during a running test worker, small-window layout and normal startup. Catalog/format placeholders tested separately. Logs and screenshots included.

**No physical CAN adapter/module or real flashing tests.** Windows GUI tests use Wine; Linux uses Xvfb/X11. These software tests do not establish approval for engine operation.

## Source

`source/qt` is the common application; `source/src` is the shared C core. Same Python source ID on both platforms, native C library as DLL/SO. Install `qt/requirements.txt`, build with `qt/build_core.sh`, run `python qt/main.py`. Linux packaging: `qt/build_linux.py` with PyInstaller 6.19.0. Windows packaging: `qt/build_windows.py` with the official embeddable Python ZIP, pinned Windows wheels and a native/cross C compiler. Source and file hashes are in manifests. Third-party notices and license texts are included. See README-DE.md for the complete German operating guide.

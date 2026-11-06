# rusEFI Wideband Manager 0.5 — Linux x86_64 — English

Native GTK application with a shared C protocol/analytics library. No Wine, Windows DLL, browser or web server is used. This package includes the executable launcher `wideband-manager`, Python UI/backend source, and a prebuilt x86_64 `libwideband.so`.

## Install dependencies and run

On Ubuntu/Debian, install the distribution's GTK3 Python bindings once:

```sh
sudo apt install python3 python3-gi python3-gi-cairo python3-cairo gir1.2-gtk-3.0
```

Extract the archive, open its folder in a terminal, then run:

```sh
./wideband-manager
```

For a simulated preview without hardware:

```sh
./wideband-manager --demo
```

Demo is clearly labeled and all CAN transmission actions are blocked. Normal startup contains no synthetic measurements. Screenshots named preview-demo, preview-tools and preview-small use simulated data.

The GUI uses system Python/GTK3 packages, so this is not a dependency-free AppImage. The included C library is x86_64; other Linux architectures require rebuilding with a native compiler via `linux/build.sh`. GUI tested on Ubuntu 26.04.1 with GTK3 under Xvfb. Other distributions, native Wayland and high-DPI multi-monitor setups have not been tested.

## CAN adapters

**SocketCAN:** enter an existing configured CAN network interface such as `can0`. Linux kernel drivers expose compatible CAN adapters as network interfaces. PCAN hardware can be used when its Linux driver exposes such an interface; Windows PCANBasic.dll is not used.

Configure the actual CAN interface bitrate before connecting. Example for a 500 kbit/s bench bus, using your own interface name:

```sh
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up
```

For 1 Mbit/s, use `1000000`. These commands change the adapter interface and require administrative privileges. The application itself runs as a regular user and never invokes sudo or changes network interface configuration. The bus bitrate dropdown controls direct SLCAN initialization only; it does not modify SocketCAN bitrate. After changing module bitrate and resetting it, configure the adapter/interface to match before reconnecting.

**Direct SLCAN / Lawicel:** select SLCAN and enter `/dev/ttyUSB0` or the appropriate device. Initialization uses 115200 baud, 8N1, then Lawicel C/S6-or-S8/O commands with acknowledgement checking. Your user needs read/write permission for that device, usually via the distribution's serial-access group. Do not open a device simultaneously with slcand or another program. At high CAN traffic, the 115200-baud link cannot carry all bus frames; prefer SocketCAN for busy buses.

Only CAN communication with the module is implemented. SLCAN's serial connection is the CAN adapter transport. There are no EGT, AEMNet, analog-output or TunerStudio interfaces.

References: [Linux SocketCAN documentation](https://docs.kernel.org/networking/can.html), [PyGObject setup](https://pygobject.gnome.org/getting_started.html). This application uses GTK3; install the GTK3 packages listed above rather than the GTK4 example packages in the general setup page.

## Features

| Area | Linux implementation |
|---|---|
| Live values | Lambda, sensor temperature, heater duty and Nernst DC cards; select data index 0..255 |
| Live graph | Six selectable traces: lambda, temperature, heater %, pump PWM %, Nernst DC V, ESR Ohm; four synchronized plots |
| Graph controls | 15/30/60/300-second windows, fixed/auto scales, mouse cursor, wheel zoom, pause, drag history, Shift+wheel pan, Space pause, Home live, +/- zoom |
| Graph CSV | Visible time window and selected index, frozen snapshot while paused; missing/stale values blank; source and validity/freshness columns |
| Configuration | Detect F0, read config and Rtrim; save index, sensor type, saved bitrate and standard/diagnostic TX flags with targeted readback |
| Backup | Windows-compatible WBF0 v2 files with CRC; show backup/current values before restoring; two verified writes |
| Selectable Rtrim | Save the sensor's labeled resistance in 0.01 Ohm steps, choose Bosch normal/software mode, or restore physical Rtrim mode |
| Module control | Targeted LED identification, reset and factory defaults; defaults retain saved bitrate |
| Extended diagnostics | On-demand snapshots of Nernst AC, pump current target/actual, effective heater voltage, received CAN supply voltage and heater state |
| Legacy settings | Separate legacy index/sensor/bitrate commands for compatible earlier firmware; legacy ACKs do not identify a sender or provide readback |
| CAN monitor | STD/EXT and hexadecimal ID filters, display pause, raw CAN CSV recording; filtering does not stop acquisition |
| Events/statistics | Session state changes, data-loss/invalid-lambda warnings, observed warmup transitions, per-ID frequency/gaps; events and CAN statistics CSV exports |
| Firmware update | Same F0 application BIN checks and CAN bootloader flow as Windows; progress display; no automatic retries after erase/data failure |
| Bench heater control | Broadcast every 50 ms with actual supply voltage and pump gain; stops on detected ECU status sender or CAN error |

The application samples up to 10 Hz into the shared 3000-point ring per index. All graph times are PC elapsed time, not module uptime. Graph pause retains a PC snapshot while reception continues. Standard A0 broadcasts provide lambda/temperature; diagnostic broadcasts provide heater/pump/Nernst/ESR. Enable diagnostic TX for those curves. Stale/invalid data creates gaps rather than a valid continuous line. Warmup times are observations during this session, not measurements stored in the module. Events and CAN statistics are bounded session histories, not controller error counters or a hardware bus-load meter.

Only one configuration/update operation runs at a time. Replies are matched by hardware ID, opcode and transaction number. Hardware IDs must be unique on the bus; duplicates cannot be reliably distinguished. Configuration readback is required before extended writes. A failed write or restore may leave partial changes; read settings again. A restore of a non-Bosch sensor backup temporarily uses normal Bosch mode to restore the Rtrim record, then applies and verifies the final sensor/configuration in a second write. Perform configuration on a bench, not during engine tuning.

For software Rtrim, enter your own sensor's labeled value, 30.00..300.00 Ohm. Do not guess. The physical trim resistor must be absent in software mode; hardware and selected mode must match.

Firmware updates are bootloader broadcasts: connect exactly one compatible F0 module and keep power stable. Only a 25600-byte application BIN with expected vectors and CRC is accepted. Full-flash/bootloader images are rejected. Successful transfer/ACK does not provide flash readback; check Pong and live data after restart.

## Firmware unchanged

No new firmware is included or built for this Linux release. The live graph works from regular CAN messages. Extended configuration functions require the same compatible F0 Manager firmware 0.4 as the Windows version. Existing application SHA256:

`9f03e1734c03e658f9715469c975fd756b23b80628ad8ae24b46cbee34855cd3`

## Validation

Shared C library builds with warnings as errors. Existing portable protocol/analytics/trace tests pass with AddressSanitizer/UndefinedBehaviorSanitizer. Linux client tests run against the actual F0 dispatcher and configuration functions with a fake HAL: detect/read, write/readback, Rtrim, backup/restore including non-Bosch mode, defaults, identification, diagnostic decoding, wrong replies, flash errors and no write retry. Additional tests cover CRC rejection, actual C analytics, SocketCAN frame packing/filtering over a local socket pair, SLCAN acknowledgement/framing and BEL errors through a pseudo terminal, firmware image preflight and complete bootloader transfer/failure flow against a simulated bootloader.

GTK GUI smoke tests cover rendering, demo transmission locks, six trace switches, paused CSV consistency, history panning, time-window selection, auto scales, index changes, tools page, smaller-window layout and disconnected startup. These are software tests. **No physical CAN adapter/module, real SocketCAN interface routing, physical flashing or native Wayland tests have been performed.** No approval for engine operation follows from these results. Test logs are included.

## Source and rebuild

`linux/` contains Linux source; `src/` contains the shared Windows protocol/analytics headers. Rebuild the native library with:

```sh
./linux/build.sh
```

The top-level launcher delegates to `linux/wideband-manager`; it does not compile on startup. Linux regression test source is included under `tests/`; actual-dispatcher tests need the existing `firmware-f0` repository tree and a C++20 compiler, neither bundled in this desktop-only archive. Existing portable tests can be run with `tests/run_desktop.sh` when a C compiler is installed.

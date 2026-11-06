# rusEFI Wideband Manager 0.5.3 — native Windows / F0 CAN

This release updates both the small native Windows manager and the F0 application firmware. It adds configuration addressing by **Hardware ID + current data index**, including selectable Rtrim. The UI remains in English. Qt/Python runtimes are not included.

## Two HW-4 modules with indices 0 and 1

1. Update each module **individually**, with only that module connected to the CAN update bus. Select `firmware/wideband_f0_manager_indexed_app.bin` in the firmware dialog. Update both modules before configuring them together.
2. Connect both modules at their active CAN bitrate. Select **Hardware ID 4** in the main window, then open **Configuration**.
3. In the popup choose **target data index 0**, then **Detect / read F0**. The main fields now show this module's index, sensor type and saved bitrate; the report also shows the active bitrate and TX settings.
4. Select **Rtrim / Sensor selection**. For software correction, choose **LSU 4.9 Rtrim SW** in the main window, enter the value printed for that sensor, then **Save Rtrim**. The manager verifies the response and reads it back.
5. Choose **target data index 1**, read it, then configure the second module in the same way. Switching the target clears the editable Rtrim value and requires a fresh configuration read.

Use the sensor's own resistance, 30.00–300.00 Ohm in 0.01 Ohm steps. Software mode requires the physical trim resistor to be absent. **Physical Rtrim** selects ordinary Bosch LSU4.9 operation and software value 0; the physical resistor must then be present.

**Target data index** in the popup is the module's current address. **Data index** in the main window selects the graph and is also the proposed new index when you press **Save index** / **Write settings**. Editing that main field alone does not redirect configuration to another module. Selecting a Data row in the main list selects both the live stream and the current configuration target; choose its HW ID and read it before editing.

## What changed

| Function | Behavior in 0.5.3 |
|---|---|
| Read F0 | Reads exactly the chosen HW/index pair: board/capabilities, settings, then Rtrim. Updates the main fields. |
| Separate Rtrim | Read/write/verification affects only the selected pair, including when both boards have HW 4. |
| Sensor, bitrate, TX flags | The same targeting applies to all configuration commands. Saved bitrate takes effect after module reset. |
| Index changes | Request and acknowledgement use the old index; subsequent verification uses the new index. The popup target follows a confirmed change. |
| Backup/restore | Existing backup format is retained. Restore follows the selected target and verifies its Rtrim and configuration; it still comprises two persistent write steps. |
| Defaults, identify, reset, diagnostic snapshots | Addressed by the selected HW/index pair. Defaults may change the index. |
| Cached values | Configuration and Rtrim belong to an exact pair. Switching indices requires a fresh read. Extended diagnostic snapshots are cleared when switching/reading a target. |
| Pump current display | Extended diagnostics now labels the actual and nominal pump-current fields correctly. |
| No response | Times out without falling back to a HW-only write or automatically retrying a write. |
| Existing interface | Compact 1080p main page, live graphs, buffered CAN console and normal configuration popup from 0.5.2 are retained. |

The new manager requires the **indexed firmware** for configuration. Old firmware can still supply normal live CAN data and can be updated through the existing firmware dialog, but will not answer the new configuration requests. New firmware also retains the legacy protocol for older tools; those tools still address HW ID alone and can change both modules with duplicate HW IDs. Use one configuration tool at a time.

**Find modules** continues to show one Pong group for duplicate HW IDs. The separate Data rows show the received indices; a Pong does not uniquely identify a physical board. HW ID comes from the board straps and is not changed by this firmware.

## Addressing and limitations

Requests are extended CAN DATA frames, DLC 8, ID `0x0EFDhhii`; replies use `0x0EFEhhii`, where `hh` is HW ID and `ii` is the current index. The existing eight-byte service payload remains unchanged. INFO reports service version 2 on this route; the old `0x0EFB0000` route still reports version 1. The module captures the reply address before applying an index change.

- Each **HW/index pair must be unique**. Two modules with identical HW and identical index still cannot be separated. No unique serial-number addressing is added. For distinct live measurement streams, use different data indices across the entire bus, even if HW IDs differ.
- The manager refuses index changes, restore or defaults into an index already observed on this connection. This is a conservative check: a formerly observed index can require reconnecting after its conflict is removed. A silent/disconnected module cannot be detected by this check.
- If an index-change acknowledgement is lost, the write may already have succeeded. Find the new data stream or select the intended new index and read again. There is no automatic write retry.
- Configuration/Rtrim/bitrate still use the existing shared flash page. Interrupted erase/programming can lose settings; the existing fallback can be 500 kbit/s and the default sensor mode. The small RAM rollback does not protect against power loss. Restore is not atomic across its two write steps.
- CAN firmware update remains a broadcast bootloader operation. **Exactly one module** may be connected during flashing, irrespective of the new configuration addressing.
- Measurement conversion, selectable Rtrim calculation, heating/pump control, periods, persisted configuration format, application compiler settings and stack budgets are unchanged from the previous Manager firmware 0.4.

## Firmware files / STM32CubeProgrammer

| File | Use | Start / length |
|---|---|---|
| `wideband_f0_manager_indexed_app.bin` | Manager CAN updater; alternatively SWD application-only programming | `0x08001800`, 25,600 bytes, padding + CRC included |
| `wideband_f0_manager_indexed_full.bin` | SWD complete bootloader + application | `0x08000000`, 31,744 bytes |
| `wideband_f0_manager_indexed_full.hex` | Same complete image with addresses | Addresses included |
| `wideband_f0_manager_indexed_app.elf` | Debug symbols and memory analysis | Not an input for the CAN updater |

The full image stops before the configuration page `0x08007C00..0x08007FFF`. When using SWD, retain this page and avoid full-chip erase if you want to keep configuration/Rtrim. Read and back up settings before updating. The CAN updater replaces only the application. Its completion means acknowledged transfer/restart; it has no independent flash-readback response. Read settings and check live values after updating.

The bootloader is **byte-identical to the existing 500 kbit/s / 1 Mbit/s selectable bootloader**, SHA256 `900a987bf46bb1af098845a286ebbe8451d983d81441a93de34985d9976ee4e2`. Application update does not replace the installed bootloader. Older fixed-bitrate bootloaders need the existing separate SWD migration if selectable bootloader behavior is required.

App SHA256: `115fdf2327c34ee9c6c4a42efd5959679e707484614573c498f71ea976b8750c`.

## Measured firmware memory

Comparison against the previously supplied Manager/Rtrim firmware 0.4, using the same ARM GNU 14.3.Rel1 toolchain and build settings:

| Resource | Previous firmware | Indexed firmware | Difference |
|---|---:|---:|---:|
| Application flash used | 20,748 B | 20,808 B | **+60 B** |
| Free application space before CRC | 4,848 B | 4,788 B | −60 B |
| ELF `.data` section | 8 B | 8 B | **0 B** |
| ELF `.bss` section | 3,816 B | 3,816 B | **0 B** |
| Free heap from ELF | 16 B | 16 B | **0 B** |
| Main / interrupt stack budgets | 1,024 / 1,024 B | 1,024 / 1,024 B | **0 B** |
| CAN RX stack budget | 512 B | 512 B | **0 B** |
| CAN RX complete working area | 680 B | 680 B | **0 B** |
| Temporary settings rollback payload | 12 B | 12 B | **0 B** |

The compiler still reports a 112-byte local CAN RX frame and 264 bytes across the known direct call path. External/indirect calls and RTOS/hardware context are not fully bounded by that analysis. No hardware stack-watermark or timing measurement has been made. The new address check allocates no additional firmware buffers or dynamic heap objects.

## Validation

Strict Windows cross-build; ARM build; application/full-image size, CRC, vectors, bootloader identity and ELF memory checks. Host tests run the actual native client state machine against two actual firmware dispatchers using separate simulated configurations with the same HW ID. They cover isolated Rtrim, index changes and reply routing, restore, defaults, wrong/stale replies, flash failure and lost acknowledgement without automatic retry. Legacy service, Rtrim/calibration, torn writes, original settings preservation, both bitrates and portable graph/analytics tests are included.

Automated GUI tests under isolated Wine/Xvfb exercise separate HW4/index0 and HW4/index1 Rtrim writes/readback, changing index, target-cache isolation, diagnostic field labels and timeouts. Compact layout/console navigation are checked at 100%, 125% and 150% DPI on a 1920×1080 desktop, and graph pause/CSV controls are regression-tested. The test-only fake PCAN DLL is **not shipped**.

**Physical modules/adapters and native Windows graphics have not been tested; no board has been flashed in this session.** Real concurrent CAN traffic, reset/power-loss recovery, flash behavior, timing and stack watermark still require bench tests. This is a built and software-tested development release.

## Build / included sources

`source/build.sh` builds the small Win32 EXE with MinGW-w64. Override `CC_WINDOWS` if needed. No Python, Qt or .NET runtime is needed to run the EXE. PCAN requires the real PEAK driver and matching 64-bit PCANBasic library; SLCAN remains a serial adapter transport to CAN, not a serial module interface.

`source/build-firmware.sh` builds F0 with ARM GNU 14.3.Rel1. The package includes the dependency subset used by the F0 build with original licenses. To bind complete existing Git checkouts instead, see `prepare-firmware-deps.sh`. Dependencies: ChibiOS `a8865dc84b114e74f62d8e470458839da71d333b`, libfirmware `39b3a04dab617e7ff24577e99d0d7fac2ce44c9f`.

`source/tests/run.sh` runs firmware/client regressions; `source/tests/run_desktop.sh` runs portable desktop tests. The included GUI drivers require a separately configured isolated Windows/Wine environment and a test-only fake DLL. `firmware-indexed.patch` records only this firmware change against the previous source. `source-manifest.json`, `validation/` and `SHA256SUMS` provide hashes and build/test evidence. The older 0.4 function/risk descriptions are retained under `historical/`; current targeting rules are documented here.

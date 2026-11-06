# Native Windows desktop 0.5.3

The small Win32 Windows application continues 0.5.2, with configuration targeting by HW ID + current data index. It uses English UI text and CAN as the only module interface. See [release notes](RELEASE-0.5.3.md) for the required firmware update and two-module workflow.

## Connect and select a module

Extract the archive and start `rusEFI-Wideband-Manager.exe`. PCAN requires the real driver and matching 64-bit PCANBasic library. SLCAN uses the existing 115200-baud serial connection to a CAN adapter; it is not a serial module interface. No Python, Qt or .NET runtime is required. Select the active CAN bitrate and connect.

**Find modules** lists HW Pong groups and separately observed Data streams. Two HW-4 boards can produce one HW-4 group plus Data 0 and Data 1. Select the HW ID in the main window and the **current target data index** in the Configuration popup, then **Detect / read F0**. Selecting a Data row also sets the target index. **Read F0** beside Find modules uses the same selection.

Read F0 updates index, sensor type, saved bitrate, TX flags and Rtrim. Active bitrate is shown separately; saving a bitrate does not change the adapter or the running module bitrate until reset. The main **Data index** field selects the graph and is the proposed destination when saving a new index; it is separate from the popup's current target address. After changing target, read again before writing.

The new indexed firmware is required for configuration. Old Manager firmware 0.4 still supplies live data and can be updated, but this manager will not fall back to HW-only configuration commands. Firmware update remains a broadcast operation: connect exactly one module while flashing.

## Layout and navigation

The main window uses a compact responsive layout with a 1120×636 logical-pixel fallback canvas. At 1920×1080 the controls fit without root scrollbars at 100%, 125% and 150% DPI in Wine tests. The graph expands to available space; below the compact canvas size, scrolling remains as a fallback.

Configuration opens as a normal centered popup at about 70% of the work-area width and height. Resize or maximize it manually. The view dropdown, current target selector and **Back / Close** remain at the top. **Escape**, including from the raw CAN monitor, returns to the main window. Opening Configuration restores its normal size.

Raw CAN text is buffered and appended in batches at most every 100 ms. Hidden monitor history is bounded. Filters and pause affect only the visible raw monitor; full raw CAN CSV and analytics continue independently.

## Live graph

| Trace | Unit | Source |
|---|---|---|
| Lambda | dimensionless | Standard CAN frame |
| Sensor temperature | °C | Standard CAN frame |
| Heater duty | % | Diagnostic CAN frame |
| Pump PWM | % | Diagnostic CAN frame |
| Nernst DC | V | Diagnostic CAN frame |
| ESR | Ohm | Diagnostic CAN frame |

Enable traces individually. Four stacked plots share a time axis; temperature and ESR have separate scales. Select 15, 30, 60 or 300 seconds, fixed or automatic scales and a lambda reference at 1.0. Invalid lambda and stale/missing data do not appear as valid continuous traces. Diagnostic broadcasts must be enabled for their traces.

- Move the mouse over the graph to inspect values.
- Mouse wheel or +/- changes the time window.
- Pause or Space freezes a snapshot while acquisition continues.
- Drag or Shift+wheel navigates frozen history.
- Home or Resume / Live returns to live display.
- CSV time window exports the visible window for the selected index, using the frozen snapshot while paused. Missing/stale values are blank.

Time is PC elapsed time since program start, not module uptime. Sampling is limited to 10 Hz with 3000 points per index. Extended service diagnostics remain separately requested snapshots.

## Other tools

Dashboard/warmup analysis, session events, CAN statistics and filtered raw monitor, configuration backup/compare/restore, standard/diagnostic TX flags, targeted LED identify and reset, diagnostic snapshots and selectable Bosch LSU4.9 Rtrim are retained. Configuration writes are verified by readback. Restore uses two write steps and may be partial after an error. Changes to settings should be performed on the bench.

Heater messages are an explicitly started broadcast for all modules, not a persistent setting. Enter the actual supply voltage; do not run another ECU status sender at the same time. The manager stops its heating broadcasts when another ECU status sender or a receive error is detected.

For a simulated preview without CAN hardware:

```powershell
.\rusEFI-Wideband-Manager.exe --demo
```

Demo is labeled and blocks CAN transmission, heater control and flashing. Normal startup displays only received values. Packaged Rtrim screenshots use a test-only fake CAN bus; their values are test data, not sensor recommendations.

## Validation and source

Strict Windows cross-build, ASan/UBSan portable tests, actual firmware/client regressions and automated isolated Wine GUI tests pass. GUI coverage includes separate HW4/index0 and HW4/index1 configuration, cache isolation, index change, timeout, diagnostic labels, compact 1080p layout at three DPI settings, repeated console navigation, graph pause/history/zoom and paused CSV consistency.

Physical adapters/modules and native Windows GPU/multi-monitor DPI behavior remain untested. No hardware has been flashed in this session. See the release notes and `validation/` for evidence and practical limits.

`source/build.sh` builds with MinGW-w64; `CC_WINDOWS` overrides its path. Firmware source and the used dependency subset are included. `source/tests/run_desktop.sh` runs portable desktop tests; `source/tests/run.sh` also exercises actual firmware functions. GUI drivers require a separate Windows/Wine setup. `tests/fake_pcan.c` is test source and must not be used as a real adapter driver; no compiled fake DLL is shipped.

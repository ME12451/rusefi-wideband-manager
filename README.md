# rusEFI Wideband Manager 0.5.3

Native Windows x64 manager and F0 CAN firmware. **Configuration now targets Hardware ID + current data index**, so two HW-4 boards with indices 0 and 1 can have independent settings and Rtrim. The new manager requires the indexed firmware for configuration. Update each board individually before using them together.

- [Release 0.5.3, update steps, protocol and measured memory](RELEASE-0.5.3.md)
- [Desktop controls and live graph](README-DESKTOP.md)
- [Settings and CAN protocol](SETTINGS.md)

The EXE continues the small native Win32 0.5 line; no Qt/Python runtime is bundled. UI language is English. Only CAN module communication is implemented, through PCAN or SLCAN adapters. No AEMNet, EGT or analog-output configuration is added.

The firmware adds 60 bytes of flash compared with Manager firmware 0.4. RAM section sizes, heap reserve, stack budgets, configuration format and bootloader are unchanged. Measurement/control algorithms and selectable Rtrim calculation remain unchanged.

Strict cross-builds, firmware image checks, actual-code client/dispatcher tests and simulated Windows GUI tests pass. No physical module/adapter has been tested or flashed in this session. Current validation details and limitations are in the release notes and packaged validation directory.

Build Windows with `./build.sh` and MinGW-w64 (`CC_WINDOWS` overrides the compiler). Build firmware with `./build-firmware.sh` and ARM GNU 14.3.Rel1. See the release notes for dependencies and included sources. Run `./tests/run.sh` plus `./tests/run_desktop.sh`; then `python3 verify_release.py` and `python3 analyze_stack.py`.

The older descriptions in `FUNKTIONEN.md`, `RISIKEN.md`, `VERGLEICH.md` and `docs/historical/README-0.4.md` describe firmware 0.4 and earlier. The 0.5.3 release notes supersede their HW-only targeting and memory figures. The separate Qt 0.6 and Linux 0.5 projects are retained in the development workspace and are not part of this Windows release.

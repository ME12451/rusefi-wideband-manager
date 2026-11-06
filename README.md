# rusEFI Wideband Manager — separate lightweight applications

Two independent applications configure rusEFI F0 wideband modules over CAN:

| Platform | Current version | Implementation | Runtime |
| --- | --- | --- | --- |
| Windows x64 | 0.5.3 | Native Win32 C application in `src/` | Small EXE; no Python/Qt/.NET bundle |
| Linux | 0.5.3 | GTK3 application in `linux/`, small shared C library | System Python 3, GTK3/PyGObject/Cairo |

Download the separate Windows ZIP or Linux tar.gz from [Releases](https://github.com/ME12451/rusefi-wideband-manager/releases/tag/v0.5.3). The applications share protocol/analytics headers but retain their existing platform interfaces and adapter implementations. The Qt 0.6 line has been discarded and its release downloads/tags withdrawn.

## Windows

Extract the complete Windows archive and run `rusEFI-Wideband-Manager.exe`. PCAN requires the PEAK driver and 64-bit PCANBasic DLL. SLCAN uses a local COM port. The preserved, software-tested native 0.5.3 executable and firmware package are unchanged.

[Windows 0.5.3 guide and protocol changes](RELEASE-0.5.3.md). Build with MinGW-w64: `bash build.sh` (`CC_WINDOWS` selects the compiler, `OUT_DIR` selects the output directory).

## Linux

Extract the complete Linux archive and run `./wideband-manager`. Uses SocketCAN or direct SLCAN; no Windows DLL or bundled GUI runtime. Install distribution packages once:

```sh
sudo apt install python3 python3-gi python3-gi-cairo python3-cairo gir1.2-gtk-3.0
```

The Linux application now includes the latest indexed configuration functionality from Windows 0.5.3, retaining its GTK interface. See the [Linux guide](linux/README.md). Rebuild its C library with `bash linux/build.sh`; launch `linux/wideband-manager` from source. `--demo` blocks CAN transmissions and supplies labeled simulated data.

## Module configuration

Both applications address extended configuration by **hardware ID plus current target data index**. Two modules with the same HW ID need different indices. Read before writing. Data index selects the live stream and proposed new index; Target data index selects the current module address. Confirmed index changes follow the new address and verify readback. Switching targets invalidates cached settings, trim and diagnostics. Firmware updates still connect exactly one module. Legacy commands require unique HW IDs.

Indexed configuration requires [F0 Manager firmware 0.5.3](https://github.com/ME12451/wideband/releases/tag/manager-f0-0.5.3). The separate [minimal Selectable firmware](https://github.com/ME12451/wideband/releases/tag/selectable-f0-2026-10-25) only adds persisted bitrate selection. This desktop update does not change firmware.

Änderungshistorie: [CHANGELOG.md](CHANGELOG.md).

## History and validation

`v0.1.0` preserves the original Windows 0.1 sources and archive. `v0.5.3` contains the latest native Windows sources and updated separate Linux application. These are imports of preserved release snapshots; intermediate development commits were not reconstructed. Historical source manifests refer to their original snapshots. The abandoned Qt work remains only in prior Git commits, with no active release or tag.

```sh
bash linux/build.sh
python3 tests/test_linux.py
python3 tests/test_indexed_service.py
bash tests/run_desktop.sh
```

Backend tests use actual F0 C++ dispatcher/configuration code with a fake HAL and two same-HW simulated nodes. Linux GUI tests use GTK/Xvfb; Windows GUI evidence uses Wine. CI checks native protocol/client code. See `validation/native-0.5.3`. No physical CAN module/adapter tests have been performed.

Firmware sources are mirrored under `firmware-f0/`; canonical development lives in [ME12451/wideband](https://github.com/ME12451/wideband). Fetch pinned dependencies with `bash prepare-firmware-deps.sh` or set `WIDEBAND_DEP_SOURCE` to a matching local clone. Provide ARM GNU 14.3.Rel1 using `ARM_TOOLCHAIN`, then run `bash build-firmware.sh`.

Source license: [MIT](LICENSE). System GTK/Python libraries and PCAN drivers retain their respective licenses.

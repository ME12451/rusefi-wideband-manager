# rusEFI Wideband Manager

CAN configuration and diagnostics for rusEFI F0 wideband modules on Windows and Linux. The current shared Qt application is **0.6.1**, in English and German. It includes the indexed configuration workflow from the native Windows Manager 0.5.3.

## Releases and history

- `v0.1.0`: original Windows Manager sources recovered from the existing 0.1 release archive.
- `v0.6.0`: preserved shared Qt 0.6 sources plus native Windows 0.5.3 development sources.
- `v0.6.1`: shared Windows/Linux manager with indexed module configuration.

These are imports of preserved snapshots, not a reconstruction of intermediate development commits. Historical source manifests describe their original release snapshots. Original sources and archives remain unchanged in the development workspace. Native Windows 0.5.3 remains in `src/`; the current shared application lives in `qt/`. The older GTK application in `linux/` and packaging scripts for older versions are historical.

## Download and operate

Download Windows x64 ZIP or Linux x86_64 tar.gz from Releases, extract the whole archive and launch the manager. Python and Qt are bundled. The Linux binary requires **glibc 2.43 or newer**; rebuild from source for older distributions. Windows supports PCAN and SLCAN; Linux supports SocketCAN and SLCAN. Set SocketCAN bitrate outside the application.

Configuration now requires indexed F0 Manager firmware **0.5.3 or newer** from [ME12451/wideband](https://github.com/ME12451/wideband/tree/manager-latest). The minimal [wideband-selectable](https://github.com/ME12451/wideband/tree/wideband-selectable) branch only adds persisted CAN bitrate selection and does not provide the extended Manager configuration service. Firmware updates connect exactly one module. No firmware change is required when updating from native Manager 0.5.3.

Choose Hardware ID and **Target data index** to address the module's current configuration. **Data index** selects the graph and proposed new index when saving settings. Read before writing. A confirmed index change moves the target and verifies readback at the new address. Changing the target clears cached settings, editable Rtrim and diagnostics. Two modules must not share both HW ID and index. Older legacy commands still require a unique HW ID.

See [English guide](qt/README-EN.md), [German guide](qt/README-DE.md), and [0.6.1 changes](RELEASE-0.6.1.md).

## Run from source

```sh
python3 -m venv .venv
. .venv/bin/activate
pip install -r qt/requirements.txt
bash qt/build_core.sh
python qt/main.py
```

`--demo` simulates values and blocks CAN transmissions. C library build requires a C compiler; Windows can build the DLL with `CC_WINDOWS=x86_64-w64-mingw32-gcc bash qt/build_core.sh`. Build native Windows without Qt using `bash build.sh`.

Linux packaging: install PyInstaller 6.19.0, then `python qt/build_linux.py`. If XCB libraries are outside standard system paths, set `WB_QT_XCB_LIB_DIR` to their directory; the release bundles the five optional XCB image/keysyms/ICCCM/cursor/render-util libraries. Windows packaging: `python qt/build_windows.py --python-zip <official-embeddable-python.zip> --wheels <pinned-wheel-directory> --output <output-directory>`. See each script's options.

Firmware sources are mirrored under `firmware-f0/`. Fetch pinned dependencies with `bash prepare-firmware-deps.sh` (or set `WIDEBAND_DEP_SOURCE` to an existing matching wideband clone), provide ARM GNU 14.3.Rel1 through `ARM_TOOLCHAIN`, then run `bash build-firmware.sh`. The firmware repository is the canonical source. Release firmware is attached there; generated images and dependency checkouts are excluded here.

## Validation

```sh
bash qt/build_core.sh
python tests/test_qt_backend.py
python tests/test_qt_i18n.py
python tests/test_indexed_service.py
bash tests/run_desktop.sh
```

Backend tests run the actual F0 C++ dispatcher with a fake HAL, including two modules with one HW ID, independent Rtrim, index changes, backup/restore, rejection of wrong replies and no automatic retry. Full `tests/run.sh` also requires a firmware build. GUI smoke tests run with `python qt/main.py --demo --settings-file /tmp/wb-test.ini --smoke-test /tmp/wb-smoke`.

No physical module/CAN-adapter tests have been performed. Windows GUI validation uses Wine; Linux uses a virtual display. Firmware binary date fields and CRC can change on rebuild.

Project source license: [MIT](LICENSE). Bundled dependencies retain their licenses; see [third-party notices](qt/licenses/THIRD-PARTY-NOTICES.md).

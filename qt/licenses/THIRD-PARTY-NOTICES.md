# Third-party components

Qt/PySide6/Shiboken 6.10.2 are distributed as dynamically replaceable upstream libraries. Open-source license texts are included. Upstream source: https://code.qt.io/cgit/qt/qtbase.git/?h=v6.10.2 and https://code.qt.io/cgit/pyside/pyside-setup.git/?h=v6.10.2 . Qt module-specific notices and the upstream wheel metadata remain with the runtime. No Qt code is statically linked into the Manager.

python-can 4.6.1: https://github.com/hardbyte/python-can/tree/4.6.1 . pyserial 3.5: https://github.com/pyserial/pyserial/tree/v3.5 . Their license files/metadata are copied from the upstream wheels.

CPython: https://www.python.org/ . Windows uses the official CPython 3.12.10 embeddable distribution and retains its LICENSE.txt. Linux uses CPython 3.12.16 from python-build-standalone release 20261026: https://github.com/astral-sh/python-build-standalone/releases/tag/20261026 . The packaged Linux executable is built with PyInstaller 6.19.0: https://github.com/pyinstaller/pyinstaller/tree/v6.19.0 . Its bootloader exception permits packaging applications; upstream license is included.

rusEFI protocol/shared code retains the accompanying original rusEFI license notice. This notice does not relicense the user's complete application or firmware.

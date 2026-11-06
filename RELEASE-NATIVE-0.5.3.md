# Separate native Windows and Linux applications — 0.5.3

Windows retains the existing compact Win32 0.5.3 executable, sources and firmware archive. Linux retains the GTK3 interface and system Python/GTK dependencies, with the newest indexed module configuration ported from Windows. No Qt or bundled Python/GUI runtime is included.

Linux now targets configuration by HW ID and current target data index, routes replies strictly, follows verified index changes and clears cached settings, Rtrim and diagnostics when switching targets. Two modules sharing an HW ID can have independent Rtrim and settings if their indices differ. No HW-only configuration fallback occurs on timeout. Legacy commands and firmware updates retain their existing behavior.

Firmware remains the indexed F0 0.5.3 image. Native Windows sources match the preserved release archive. Linux backend tests cover actual firmware dispatch, two-node isolation, index changes, backup/restore, wrong replies, failures and no retry; GTK smoke tests cover rendering, graphs, startup and target-cache changes. No physical hardware tested.

The Qt 0.6 experiment and 0.6.1 packages are withdrawn. Separate Windows and Linux downloads replace them.

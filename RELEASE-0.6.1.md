# Manager 0.6.1 — indexed configuration on Windows and Linux

The common Qt manager now uses the same hardware-ID + current-data-index service protocol as native Windows Manager 0.5.3. Requests use `0x0EFDhhii`, replies `0x0EFEhhii`; opcode, transaction, extended frame format and full target address must match. No fallback to HW-only configuration occurs on timeout.

A dedicated Target data index field selects the current module. Data index retains its graph/proposed-setting role. Writes and factory defaults acknowledge the old address, then read back at the returned new index. Target changes clear cached settings and trim input and invalidate diagnostic snapshots. Backups retain the existing WBF0 v2 CRC format. Firmware updates and explicitly labeled legacy operations retain their previous bus behavior.

This carries the latest configuration functionality into Linux and the shared Windows app while preserving 0.6 graphs, language switching and adapter support. Firmware 0.5.3 is unchanged. Existing application SHA256: `115fdf2327c34ee9c6c4a42efd5959679e707484614573c498f71ea976b8750c`.

Validation: real firmware dispatcher with fake HAL, two same-HW simulated nodes, isolated Rtrim, index migration and restore, wrong/stale replies, timeout without fallback, protocol/analytics/trace regressions, English/German catalog checks and packaged GUI smoke checks. No physical hardware tested; Windows checks use Wine. Linux binary requires glibc 2.43+.

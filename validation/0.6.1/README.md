# 0.6.1 validation

7 backend/adapter tests and 2 language-catalog tests passed. Backend tests use actual firmware dispatcher/configuration code with fake HAL, including two same-HW nodes. Firmware/client, protocol, analytics and graph regressions passed (`wb061-regressions.log`). Packaged applications passed 30 demo and 20 offline GUI checks on each platform. Windows ran under Wine 11.21/Xvfb; Linux ran under Xvfb on Ubuntu 26.04.1. No physical CAN hardware tested.

The current firmware rebuilt successfully using ARM GNU 14.3.Rel1. Compared with the existing 0.5.3 application, the rebuild differs only at offset 10816 (embedded build day 5 -> 6) and its four-byte CRC. Releases preserve the original image SHA256 `115fdf2327c34ee9c6c4a42efd5959679e707484614573c498f71ea976b8750c`. The manager update does not change firmware sources.

# Einstellungen laut lokalem Wideband-Source

Quelle: `firmware/boards/port.h`, `firmware/can.cpp`, `for_rusefi/wideband_can.h`, `for_rusefi/wideband_bitrate.h`, `firmware/wideband_config.h`. Keine erfundenen CAN-Register oder Write-Memory-Kommandos.

| Feld / Funktion | Werte / Umfang | Erreichbarkeit in Version 0.1 |
|---|---|---|
| Hardware-ID | 0..8, Loetbruecken `3 * sel1 + sel2` | Pong lesen; nicht per CAN aenderbar |
| `sensorType` | LSU49=0, LSU42=1, LSUADV=2, LSU49_FAE=3 | CAN schreiben; keine vollstaendige Ruecklesung, Flags im Standardframe nur LSU49/FAE |
| `afr[i].RusEfiIdx` | uint8, bis 4 Kanaele strukturell | CAN setzt Startindex plus Kanalnummer; Mess-ID 0x190 + 2 * Index |
| `egt[i].RusEfiIdx` | uint8, bis 4 Kanaele strukturell | Derselbe CAN-Indexbefehl setzt auch EGT-Startindex; kein separates EGT-Kommando |
| F0-Bitrate | 500k=0, 1M=1 | Schreiben mit eigener ACK-Zuordnung, nur erweiterte Firmware/Bootloader; nach Reset aktiv, kein Get-Kommando |
| Heizfreigabe | Bit 0 im ECU-Status | Laufzeit-Broadcast alle 50 ms; kein persistentes Setting |
| Externe Versorgungsspannung | Byte in 0.1 V | Laufzeit-Broadcast; Timeout 250 ms |
| Pumpenverstaerkung | CAN-Kommentar 0..200%; Empfaenger clampF(0, gain, 1) | Effektiv 0..100%; GUI begrenzt auf den wirksamen Bereich |
| `auxOutBins[2][8]` | 2 Kennlinien mit je 8 Eingangswerten | Kein CAN-Read/Write-Kommando |
| `auxOutValues[2][8]` | 2 Kennlinien mit je 8 Ausgangswerten | Kein CAN-Read/Write-Kommando |
| `auxOutputSource[2]` | AFR0, AFR1, Lambda0, Lambda1, EGT0, EGT1 | Kein CAN-Read/Write-Kommando |
| `afr[0..3].RusEfiTx` | bool | Kein CAN-Read/Write-Kommando |
| `afr[0..3].RusEfiTxDiag` | bool | Kein CAN-Read/Write-Kommando |
| `afr[0..3].AemNetTx` | bool | Kein CAN-Read/Write-Kommando |
| `afr[0..3].AemNetIdOffset` | uint8 | Kein CAN-Read/Write-Kommando |
| `egt[0..3].RusEfiTx` | bool, laut Source nicht implementiert | Kein CAN-Read/Write-Kommando |
| `egt[0..3].RusEfiTxDiag` | bool | Kein CAN-Read/Write-Kommando |
| `egt[0..3].AemNetTx` | bool | Kein CAN-Read/Write-Kommando |
| `egt[0..3].AemNetIdOffset` | uint8 | Kein CAN-Read/Write-Kommando |

Tag, `NoLongerUsed0`, Padding sowie der Bitraten-Magic/Invertiert-Datensatz sind interne Verwaltungsfelder, keine Nutzereinstellungen. Vier AFR-/EGT-Slots in der Struktur bedeuten nicht, dass jedes Board vier Kanaele besitzt. F0 hat einen AFR-Kanal. Die AUX-Standardkurve ist AFR 8.5..18 -> 0..5 V. Standard-Tx: AFR rusEFI+Diagnose aktiv, AFR AEMNet aus; EGT rusEFI aus/AEMNet aktiv (boardabhaengiger Betrieb).

## CAN-Befehle (29 Bit Extended)

| ID | Daten / Bedeutung |
|---|---|
| `0x0EF00000` | App: 1 Byte Hardware-ID oder FF -> Reset; Bootloader: leer -> Start |
| `0x0EF15A5A` | leer -> F0-App loeschen, Broadcast |
| `0x0EF20000 \| offset` | 8 Byte Appdaten ab Offset, Broadcast |
| `0x0EF30000` | leer -> Bootloader verlassen, Broadcast |
| `0x0EF40000` | `[Startindex, Hardware-ID]` -> Index speichern |
| `0x0EF50000` | `[Vbat*10, Heizfreigabe, PumpGainPercent]` -> Status-Broadcast |
| `0x0EF60000` | `[Hardware-ID]`, FF fuer alle -> Pong |
| `0x0EF70000` | `[Hardware-ID, Sensortyp]` -> speichern |
| `0x0EF80000` | `[Hardware-ID, Bitratenwert]` -> Sonderfirmware-Bitrate speichern |
| `0x0EF80001` | Antwort `[Hardware-ID, Bitratenwert]` |
| `0x00727573` | ACK leer oder Pong mit 8 Byte; ACK ohne Ziel/Opcode |

Standard-Datenframe: Version, Flags (Bit0 gueltig, Bit1 LSU49, Bit2 FAE), Lambda uint16 LE /10000, Temperatur uint16 LE in C, Padding. Diagnose: ESR uint16 LE, Nernst uint16 LE /1000 V, Pump-PWM /255, Status, Heater-PWM /255, Padding. IDs sind 11-Bit-Standard und pro Index zwei aufeinanderfolgende IDs. Statuswerte 0..7: Preheat, Warmup, ClosedLoop, DidntHeat, Overheat, Underheat, NotAllowed, CanSilent. Pong enthaelt HW-ID, Protokollversion, Jahr seit 2000, Monat, Tag und 3 reservierte Bytes.

## Compile-Time-Einstellungen

`wideband_config.h` definiert unter anderem ESR-Sense-Filter, Pumpstrom-Sense-Gain/Widerstand/Filter, Nernst-Ziel, Pump-/Heiz-Regelperioden, Vorheizzeit, Warmup-Timeout, Spannungsstabilisierung und Ein-/Ausschaltschwellen, Pumpstart-Temperaturoffset und CAN-Status-Timeout. Weitere Regler-/Sondendaten und Boardoptionen stehen in den jeweiligen Regler-/Boardquellen. Dies sind Buildparameter, keine ueber das vorhandene CAN-Protokoll verfuegbaren Settings. Die Anwendung fuehrt keine universellen Speicherzugriffe aus.

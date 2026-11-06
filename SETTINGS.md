# F0 / CAN: Einstellungen und Service v2 (Manager 0.5.3)

| Einstellung | Werte | Wirkung |
|---|---|---|
| Hardware-ID | 0..8 | Loetbruecken, nur lesen |
| Messindex | 0..255 | Standard-ID 0x190 + 2 * Index; Diagnose-ID +1 |
| Sensortyp | 0 LSU49, 1 LSU42, 2 LSUADV, 3 FAE LSU49, 4 LSU49 Rtrim SW | Persistiert, Hardware-/Sondeneignung vorausgesetzt |
| Standard-Tx / Diagnose-Tx | je an/aus | Persistiert, standardmaessig beide an |
| Bitrate | 0=500k, 1=1M | Gespeicherter Wert nach Reset aktiv; aktiver Wert separat lesbar |
| Software-Rtrim | 0 oder 3000..30000 Centiohm | 0=kein Wert, sonst 30.00..300.00 Ohm; wirkt nur bei Typ 4 |
| Heizfreigabe / Spannung / Pump Gain | Laufzeitstatus | Broadcast, nicht Bestandteil des persistenten Backups |

Werkseinstellungen erhalten die gespeicherte Bitrate, setzen Sondentyp auf Firmwaredefault, Index nach Loetbruecken (Mapping 2,0,3,4,0,1,5,6,7), Tx auf an und Rtrim auf 0. Pro F0 gibt es einen Wideband-Kanal. Keine EGT-/Analog-/AEMNet-Konfiguration.

## Neue gezielte CAN-Servicebefehle

Alle Frames Extended DATA, DLC 8. Ab Manager 0.5.3: Request-ID **0x0EFDhhii**, Antwort-ID **0x0EFEhhii**; `hh` = Hardware-ID, `ii` = aktueller Datenindex. INFO meldet auf diesem Weg Serviceversion 2. Beide Selektoren muessen passen. Bei Indexaenderungen erfolgt die Antwort auf der alten Adresse, danach muss unter dem neuen Index gelesen werden.

Der bisherige Weg bleibt in der Firmware erhalten: Request **0x0EFB0000**, Antwort **0x0EFC0000 | Hardware-ID**, INFO-Version 1. Er adressiert nur die HW-ID und ist bei doppelten HW-IDs nicht getrennt nutzbar. Der neue Manager verwendet ihn nicht als Konfigurationsfallback. Payloads und Befehle sind fuer beide Wege gleich. Diese IDs sind getrennt von den Kalibrierbefehlen aus dem anderen Quellstand.

Request: `[Opcode, HW-ID, Transaktion, Payload0..4]`.
Antwort: `[Opcode, Transaktion, Ergebnis, Payload0..4]`.
Ergebnis: 0 OK, 1 ungueltig, 2 Flashfehler, 3 passender Bootloader fehlt, 4 unbekannter Befehl.

| Opcode | Funktion | Request-Payload | Antwort-Payload |
|---|---|---|---|
| 1 | Info | 0 | F0=0xF0, Protokoll=1, Capabilities=0x7F, 0, Kanalzahl=1 |
| 2 | Konfiguration lesen | 0 | Index, Sensor, gespeicherte Bitrate, Tx-Flags, aktive Bitrate |
| 3 | Konfiguration schreiben | Index, Sensor, Bitrate, Tx-Flags, 0xA5 | wie Opcode 2 |
| 4 | Werkseinstellungen | 0xA5, 0, 0, 0, 0 | wie Opcode 2 |
| 5 | Identifizieren | Sekunden 0..30, sonst 0 | Sekunden, sonst 0 |
| 6 | MCU-Neustart | 0xA5, sonst 0 | 0; Reset nach Antwort |
| 7 | Diagnose 1 | 0 | Nernst-AC uint16 LE mV, Pumpstrom Ist int16 LE uA, Heizzustand |
| 8 | Diagnose 2 | 0 | Pumpstrom Soll int16 LE uA, effektive Heizspannung uint16 LE mV, CAN-Vbat in 0.1 V |
| 9 | Rtrim lesen | 0 | Centiohm uint16 LE, Sensortyp, Kalibrierung bereit 0/1, 0 |
| 10 | Rtrim + Bosch-Modus schreiben | Centiohm uint16 LE, Sensor 0/4, 0xA5, 0 | wie Opcode 9 |

Tx-Flags: Bit 0 Standard, Bit 1 Diagnose. Sensor 4 erfordert einen gueltigen Nichtnull-Rtrim. Antworten muessen zur erwarteten Hardware-ID, DLC, Opcode und Transaktion passen. In Version 0.4 gibt es keinen RAM-Antwortcache. Neue Service-Schreibauftraege vergleichen die Einstellungen; ein unveraenderter Auftrag wird ohne Flash-Erase bestaetigt, wenn auch der echte Flashinhalt einschliesslich Bitratenrecord dazu passt. Nach einem Erasefehler wird die Speicherung deshalb nicht allein wegen des zurueckgesetzten RAM-Zustands als erfolgreich angesehen. Der Manager wiederholt unklare Schreibauftraege nicht automatisch.

Die bestehenden Befehle fuer Index/Sensor/Ping/Heizstatus/Bitrate und F0-Bootloader bleiben erhalten. Auch die selectable-Rtrim-Befehle des anderen Quellstands bleiben erhalten: Set 0x0EF90000 DLC4 `[HW, Rlo, Rhi, Sensor]`, Get 0x0EF90002 DLC1 `[HW]`, Antwort 0x0EF90001 DLC5 `[HW,Rlo,Rhi,Result,Sensor]`. Der Manager verwendet fuer erweiterte Aktionen die neuen zuordenbaren Serviceantworten. Keine serielle Modul-Anbindung.

## Backupformat

20 Byte, little endian: Magic `WBF0` (0..3), Version 2 (4), Board 0xF0 (5), Quell-HW-ID (6), reserviert 0 (7), Index/Sensor/Bitrate/Tx-Flags (8..11), Rtrim uint16 (12..13), reserviert 0 (14..15), CRC32 ueber Bytes 0..15 (16..19). Keine ungeprueften Raw-Memory-Schreibzugriffe. Wiederherstellung hat zwei verifizierte Persistierschritte; sie ist nicht atomar.

# rusEFI Wideband Manager 0.1

Erste native Windows-x64-Anwendung im Stil eines CAN-Modulmanagers. Gebaute EXE: `../artifacts/wideband-manager/rusEFI-Wideband-Manager.exe`. Kein .NET-/Python-Installationsbedarf. **Entwicklungsstand: Cross-Compilation und Host-Protokolltests bestanden; keine Ausfuehrung unter Windows und kein Hardwaretest.** Keine produktionsvalidierte Flashsoftware.

## Bedienung

1. EXE unter Windows 10/11 x64 starten. Bei PCAN den offiziellen PEAK-Treiber und die 64-Bit-PCANBasic.dll installieren; alternativ die offizielle DLL neben die EXE legen. Treiber/DLL sind nicht enthalten. API: https://www.peak-system.com/support/software-information/development-kits/pcan-basic/
2. Adapter waehlen: PCAN-USB Kanalnummer 1..16 oder SLCAN/Lawicel COM-Portnummer. SLCAN erwartet **115200 Baud, 8N1**, Standardbefehle C/S6/S8/O und quittierte Kommandos. Andere USB-CAN-Protokolle, Vector/Kvaser/J2534 und CAN-FD sind nicht implementiert.
3. Aktuelle Busbitrate waehlen (500k/1M), verbinden. Verbinden sendet keine Wideband-Einstellungen. CAN-Empfang startet sofort. `Module suchen` sendet einen Broadcast-Ping. Die Modulzeile setzt bei Auswahl die Hardware-ID.
4. Hardware-ID und Messindex bewusst getrennt angeben: Die Loetbruecken ergeben die Hardware-ID 0..8; CAN-Messdaten verwenden `0x190 + 2 * Index` sowie die folgende Diagnose-ID. Das Protokoll liefert **keine Zuordnung zwischen Hardware-ID und Messindex**. Bei unklarer Zuordnung jeweils nur ein Modul anschliessen. Doppelte Hardware-IDs antworten auf denselben Befehl; sie lassen sich nicht getrennt adressieren.
5. Index/Sensortyp bei Bedarf speichern. Sensortyp muss zur angeschlossenen Sonde und Boardbeschaltung passen; die Firmware prueft die Hardwareeignung nicht. Legacy-ACKs enthalten keinen Absender. Keine anderen Konfigurationswerkzeuge gleichzeitig senden lassen. ACK ist keine Ruecklesung oder unabhaengige Flash-Verifikation.
6. Modulbitrate nur bei unterstuetzter F0-Sonderfirmware speichern. Erfolg verlangt ein passendes ACK mit Hardware-ID und Wert. Aenderung erst nach Modulreset aktiv. Alle Module am bisherigen Bus vorbereiten; danach Module neu starten und Adapter/ECU passend umstellen. Das Programm setzt Module nicht automatisch zurueck. Ohne Antwort wird kein Erfolg behauptet.
7. Gewuenschten Messindex eingeben, um Lambda, AFR (Referenz 14.7), Temperatur, Validitaet, Sensorflags, ESR, Nernstspannung, Pump-/Heater-PWM und Diagnosestatus zu lesen. Datenalter zeigt veraltete Werte. HW-ID und Builddatum kommen aus Pong. Eine vollstaendige gespeicherte Konfiguration kann ueber das vorhandene CAN-Protokoll nicht ausgelesen werden.
8. CAN-Monitor zeigt Standard/Extended-Datenframes; CSV speichert empfangene und eigene gesendete Frames mit PC-Millisekundenzaehler. Keine CAN-Remote-, Error- oder FD-Frames. Anzeige ist auf etwa 45 kB begrenzt; CSV bleibt fortlaufend. Zeitstempel sind Host-Verarbeitungszeiten, keine Hardwarezeitstempel. Keine Garantie verlustfreier Aufzeichnung bei hoher Buslast.

## Heating enforce / Force Heating

Heizfreigabe ist im Modul **kein dauerhaft gespeichertes Force-Heating-Setting**. Die Anwendung kann das ECU-Statustelegramm alle 50 ms senden: reale Versorgungsspannung (0..25.5 V), Freigabebit, Pumpenverstaerkung (0..100%). Das wirkt auf **alle Module am Bus**, nicht nur die eingestellte Hardware-ID. Die interne Reglerlogik bleibt aktiv.

Nur ohne anderen ECU-Statussender betreiben. Erkennt die Anwendung dessen Telegramm, stoppt sie eigene Heiztelegramme und sperrt einen Neustart bis zum erneuten Verbinden. Die Erkennung ersetzt keine Buskoordination. Die Spannung muss der echten Versorgung entsprechen. Eine trockene, geeignete Sonde ist Voraussetzung. Stoppen sendet einmal Freigabe=0; Trennen/Schliessen beendet die Telegramme. Nach 250 ms verwendet die Firmware ihren eigenen Timeout/Fallback; je nach Board mit lokaler Spannungsmessung bedeutet fehlendes CAN nicht zwingend Heizung aus. Das F0-Modul ist auf die CAN-Versorgungsspannung angewiesen. Beim Firmwareupdate werden eigene Heiztelegramme beendet.

## Firmwareupdate ueber CAN

Implementiert ist ausschliesslich der **eigene STM32F042/F0-Modul-Bootloader**, nicht OpenBLT auf F1-Boards. Boardtyp wird nicht automatisch erkannt; Pong.HwId ist die Loetbruecken-ID, keine Boardkennung. Vorher Board und Image selbst zuordnen.

- Ausschliesslich **ein Modul** am Updatebus anschliessen. Der Anwendungs-Neustart ist adressiert, Bootloader-Start/Erase/Data/Reboot sind **Broadcast**. Es gibt keine sichere softwareseitige Begrenzung auf ein Zielmodul.
- Der Dateidialog akzeptiert nur ein vollstaendiges, aufgefuelltes **Application-BIN mit 25600 Bytes**, Startadresse `0x08001800`, gueltigen F0-Vektoren und CRC32 in Big Endian am Ende. Full-BIN, ELF, HEX, Bootloader-BIN und SREC werden abgelehnt. CRC und Vektoren sind Integritaetspruefungen, keine umfassende Hardwarekompatibilitaetspruefung.
- App tritt in Bootloader ein; leere Startframes fangen das Bootfenster ab. Ein bereits wartender Bootloader wird ebenfalls angesprochen. Danach Erase, 8-Byte-Schreibbloecke mit ACK pro Block, Reboot. Keine automatischen Wiederholungen bei unklarem Schreibresultat. Der Bootloader laesst nur eine CRC-gueltige App starten.
- Erfolgsmeldung bedeutet: Uebertragung und Reboot wurden quittiert. Es gibt **keinen Flash-Readback und kein CRC-Ergebnis-Telegramm**. Nachher Build-Pong und Livewerte kontrollieren. Bei Fehler bleibt eine Wiederherstellung ueber den wartenden Bootloader oder SWD erforderlich. Spannungsunterbrechungen wurden nicht getestet.
- Update ersetzt nur die Anwendung und erhaelt beim analysierten F0-Layout Bootloader/Konfigurationsseite. Die erstmalige Installation des bitratefaehigen Bootloaders erfordert das passende Full-Image ueber SWD; ein CAN-App-Update kann das nicht erledigen. Bei alten Bootloadern kann die Updatebitrate von der App-Bitrate abweichen.

Firmwaredateien werden bewusst nicht automatisch ausgewaehlt oder mitgeflasht. Vorhandenes lokales Beispiel: `artifacts/wideband-selectable/wideband_f0_selectable_app.bin`. Dessen separate Dokumentation und Hardwarevoraussetzungen beachten.

## Source-Basis und Umfang

Analysierter lokaler Checkout: `epicefi/firmware/controllers/can/wideband_firmware`, HEAD `64cdf9f97ff9a1110ce3e7a47eb22ebdebe4bf86`. Die Einstellungs-/Protokollquellen sind in `source-manifest.json` per SHA256 dokumentiert. Es wurde kein Anspruch auf den neuesten Online-Stand erhoben. Vorhandene Repositories/Firmware wurden nicht geaendert.

`SETTINGS.md` listet alle Nutzkonfigurationsfelder aus `Configuration`, ihre CAN-Erreichbarkeit sowie Laufzeit- und Compile-Time-Einstellungen. Diese Version stellt alle vorhandenen CAN-Konfigurationsbefehle bereit. AUX/AEMNET/Tx-Flags sind sichtbar dokumentiert, aber mangels CAN-Schreibbefehl nicht editierbar. Vollstaendige Bearbeitung dieser internen Felder benoetigt eine Firmwareerweiterung oder die gesonderte serielle TunerStudio-Anbindung. Diese Version implementiert diese Anbindung nicht. F1/OpenBLT-Update und automatische Konfigurationsruecklesung sind ebenfalls nicht enthalten.

## Build und Pruefung

Linux mit MinGW-w64 C-Compiler: `./build.sh`. Alternativ `CC_WINDOWS=/pfad/zum/x86_64-w64-mingw32-gcc-posix ./build.sh`. Nur Windows-Systembibliotheken, keine Compiler-Runtime-DLL noetig. Build verwendet `-Wall -Wextra -Werror`.

Hosttests:

```sh
gcc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined tests/test_protocol.c -o /tmp/wb-protocol-tests
ASAN_OPTIONS=detect_leaks=0 /tmp/wb-protocol-tests ../artifacts/wideband-selectable/wideband_f0_selectable_app.bin
```

LeakSanitizer ist in dieser ptrace-Umgebung deaktiviert; AddressSanitizer und UBSan bleiben aktiv. Tests pruefen CAN-Konstanten, SLCAN-Kodierung/Dekodierung und fehlerhafte Frames, CRC-Referenz, Vektoren, beschaedigte/falsch grosse Images und das vorhandene App-Image. PE-Importtabelle wurde geprueft. **Keine Tests des GUI-Laufs, der Adaptertreiber, des Thread-/Timing-Verhaltens oder einer realen Flashtransaktion.**

Vor Hardwarefreigabe fehlen: Windows-Start/DPI, beide Adapter, Scan/Mehrmodulbetrieb, echte ACK-Zuordnung, Index/Sensortyp, Bitratenwechsel inkl. Reset, Heater-Timeout und Konkurrenz mit ECU, Flashen/Wiederherstellung bei beiden Bitraten und gestoertem Bus. Erst am isolierten Pruefstand validieren.

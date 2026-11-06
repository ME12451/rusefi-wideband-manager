> Native Windows **0.5.2**: kompakte 1080p-Hauptseite, Konfiguration als normales Popup und feste Navigation aus der CAN-Konsole. Siehe [Desktop-Anleitung](README-DESKTOP.md). Firmware unverändert.

> Aktuelle native Windows-Linie: **0.5.1**, Fortsetzung von 0.5 mit Scroll-Korrektur, Read F0 im Hauptfenster und getrennten Hardware-/Datenstrom-Einträgen. Siehe [Desktop-Anleitung](README-DESKTOP.md). Keine Firmwareänderung.

> Version 0.6: gemeinsame Qt-Anwendung für Windows und Linux mit sofortigem Deutsch-/Englisch-Dropdown. Siehe [Anleitung 0.6](qt/README-DE.md). Firmware unverändert.

> Desktop 0.5: Überarbeitete Oberfläche und neuer Live-Graph, ohne Firmwareänderung. Siehe [Desktop-Anleitung](README-DESKTOP.md). Die folgenden Angaben beschreiben Firmware und Paket 0.4.

# rusEFI F0 Wideband Manager 0.4

Windows-x64-EXE mit ausschliesslicher CAN-Anbindung fuer das STM32F042-Wideband-Modul. Neu: Mehrmodul-Dashboard, Verlaeufe, Ereignisse/Warnungen, Aufheizdiagnose, CAN-Filter/Statistik, Konfigurationsbackup und selectable Rtrim. Die separat beiliegende Firmware verbindet die CAN-Service-Erweiterung mit dem Quellstand aus `wideband-software-calibration`.

**Entwicklungsstand:** Windows-Cross-Build mit `-Wall -Wextra -Werror`, ARM-Build, Imagepruefungen und Hosttests bestanden. Kein erfolgreicher Windows-/Wine-GUI-Lauftest und keine Hardwaretests. Der Wine-Test war wegen einer nicht lauffaehigen lokalen Wine-Umgebung nicht auswertbar. Keine Freigabe fuer den Motorbetrieb aus diesen Tests ableiten. Es wurde kein Modul geflasht.

## Aenderungen in Version 0.4

Die urspruenglichen Anwendungs-Stackgroessen sind wiederhergestellt: Hauptthread 1024 Byte, Interrupts 1024 Byte, CAN-Rx-Nutzstack 512 Byte. Der komplette Konfigurations-Snapshot wurde durch eine 12-Byte-Sicherung der betroffenen F0-Einstellungen ersetzt. Die neuen Werkseinstellungen lassen unbeteiligte Legacy-Felder unveraendert. Der RAM-Antwortcache ist entfernt; bei gleichen neuen Servicewerten wird Flash einschliesslich Bitratenrecord geprueft und nur bei Bedarf erneut programmiert. Das verhindert auch einen falschen Erfolg nach vorherigem Erasefehler. Bootloader- und Anwendungsoptionen sind beim Build getrennt.

`FUNKTIONEN.md` erklaert jede Funktion in Tabellen, `RISIKEN.md` bewertet die verbleibenden Probleme mit Pruefnachweisen, `VERGLEICH.md` zeigt Original/0.3/0.4. Version 0.3 und deren bereits verteiltes Image bleiben als eigene Artefakte unveraendert. Diese Version liefert ein neues App-Image.

## Start und Verbindung

- `rusEFI-Wideband-Manager.exe` starten. Schrift 15 pt, DPI-Skalierung, maximierbares Fenster und Scrollleisten fuer kleine Bildschirme.
- Adapter: PCAN-USB (Kanal 1..16) oder SLCAN/Lawicel-CAN-Adapter ueber COM (115200 Baud, 8N1). Der COM-Port ist ausschliesslich Adaptertransport; keine serielle Modul-/TunerStudio-Anbindung. Klassisches CAN, 500 kbit/s oder 1 Mbit/s.
- PCAN braucht den offiziellen Treiber und die 64-Bit-PCANBasic.dll, alternativ DLL neben der EXE. Diese sind nicht enthalten. API: https://www.peak-system.com/support/software-information/development-kits/pcan-basic/
- Aktuelle Busbitrate waehlen und verbinden. `Module suchen` liefert Hardware-ID, Protokollversion und Builddatum. Hardware-ID (Loetbruecken 0..8) und Messindex sind unterschiedliche Werte. Doppelte Hardware-IDs lassen sich nicht getrennt adressieren.
- Vor erweiterten Aktionen die Ziel-Hardware-ID im Hauptfenster einstellen, `Dashboard / F0 CAN Tools` oeffnen und **F0 erkennen / lesen** ausfuehren. Die Board-/Protokollpruefung schaltet die neuen Funktionen frei und liest Konfiguration sowie Rtrim. Dadurch ist die Hardware-ID dem Messindex zugeordnet. Alte Firmware bleibt fuer bisherigen Empfang und die Legacy-Befehle nutzbar; sie beantwortet die neuen Serviceanfragen nicht.

## Dashboard, Verlaeufe und CAN-Auswertung

Das Auswahlfeld im Tools-Fenster wechselt zwischen sieben Ansichten:

1. **Dashboard / Aufheizdiagnose:** alle empfangenen Messindizes, Lambda, Gueltigkeit, Temperatur, Status, PWM, ESR und Datenalter. Zuordnung HW-ID/Index nur fuer einzeln erkannte Module. Aufheizzeit ist die beobachtete Zeit Warmup -> Closed loop; ein bereits laufendes Modul erzeugt keine erfundene Aufheizzeit.
2. **Verlauf:** Messindex im Hauptfenster einstellen, dann Lambda, Temperatur, Heiz-/Pumpen-PWM, Nernst-DC oder ESR waehlen. Ansichten 30 s, 60 s oder 5 min, automatische Y-Skalierung. Ungueltiges Lambda rot; Datenluecken ueber 1 s werden nicht verbunden. Pro Index bis 3000 Punkte, hoechstens 10 Hz. Diagnosewerte stammen aus den Diagnoseframes, nicht aus einer neuen Messung im PC.
3. **Fehlerhistorie / Warnungen:** letzte 1024 Zustandswechsel/Ereignisse der Sitzung, einschliesslich Heizfehler, Ueber-/Untertemperatur, Messdatenverlust und ungueltigem Lambda bei Closed loop. Kein dauerhaftes Fehlerarchiv im Modul. CSV exportiert den noch vorhandenen Ringpuffer.
4. **CAN-Statistik / Filter:** RX/TX-Anzahl, pro empfangener ID Anzahl, mittlere Rate, groesster Abstand und Alter. Bis 2048 unterschiedliche IDs. Rohmonitor nach hexadezimaler ID und STD/EXT filtern oder pausieren. Filter/Pause betreffen nur die Anzeige; CSV und Auswertung laufen weiter. Keine Controller-Buslast-, Error-Counter- oder FD-Analyse. Zeitstempel sind PC-Verarbeitungszeiten.
5. **F0 Konfiguration / Backup:** gelesene Werte, Tx-Flags, Sicherung, Vergleich, Wiederherstellung, Defaults, Identifikation und Neustart.
6. **Erweiterte Diagnose:** auf Anforderung Pumpstrom Soll/Ist, Nernst-AC, effektive Heizspannung als Reglerwert, empfangene CAN-Versorgungsspannung und Heizzustand. Zwei gezielte Momentaufnahmen mit getrenntem Datenalter; keine lokalen Versorgungsspannungsmessungen am F0.
7. **Rtrim / Sensorauswahl:** gelesener Kalibrierwert und aktiver Modus sowie Ohm-Eingabe und Schreib-/Leseaktionen.

Verlaeufe, Ereignisse und CAN-Statistik haben eigene CSV-Exporte. `CAN CSV starten/stoppen` im Hauptfenster zeichnet rohe empfangene und eigene gesendete Datenframes auf. Der sichtbare Rohmonitor ist begrenzt, die Datei fortlaufend. Keine Garantie verlustfreier Aufzeichnung bei hoher Last. SLCAN/115200 ist insbesondere bei mehreren Modulen begrenzt: drei F0 mit beiden 100-Hz-Frames benoetigen bereits mehr UART-Bandbreite als verfuegbar. Neue Diagnoseabfragen erfolgen nur auf Knopfdruck; keine automatische Flash-Schreibschleife.

## Konfiguration, Backup und Restore

Die neue CAN-Schnittstelle liest: Messindex, Sensortyp, Standard-/Diagnose-Tx, gespeicherte Bitrate, aktive Bitrate und Rtrim. Die aktive Bitrate ist der beim Start geladene Wert. Neue Bitrate wird erst nach Modulreset aktiv.

`Felder schreiben` verwendet Messindex, Sensortyp und Modulbitrate aus dem Hauptfenster plus die Tx-Checkboxen im Tools-Fenster. Bei erkannter neuer Firmware nutzen auch die bisherigen Speicherbuttons gezielte Serviceantworten und Ruecklesung. Ohne Erkennung bleiben die alten Befehle mit ihrem Legacy-ACK ohne Absender bestehen.

- **Standard-Tx:** Lambda, Gueltigkeit, Temperatur und Sensorflags. Abschalten nimmt auch der ECU die Lambdawerte.
- **Diagnose-Tx:** ESR, Nernst-DC, Pump-/Heiz-PWM und Status. Abschalten spart CAN-Verkehr, entfernt aber diese Diagnoseinformationen. Normalerweise beide eingeschaltet lassen.
- **Backup speichern:** sichert die zuletzt gelesene Konfiguration inklusive Rtrim in eine `.wbf`-Datei mit Versions-/Boardkennung und CRC. Alter der letzten Konfigurationsruecklesung ist sichtbar. Keine Kopie des gesamten MCU-Flashs.
- **Backup laden / Vergleich:** prueft Datei und zeigt Unterschiede. Laden schreibt noch nichts. Vergleich in Ansicht `F0 Konfiguration / Backup`.
- **Wiederherstellen:** bestaetigte Aktion, zuerst Rtrim/Sondenmodus, dann die restlichen Werte; anschliessend Ruecklesung beider Gruppen. Dies sind zwei Schreibschritte, keine atomare Gesamttransaktion. Bei Fehler/Unterbrechung koennen Teilaenderungen verbleiben; erneut lesen. Kein automatischer Wiederholungsversuch bei unklarem Schreibergebnis.
- **Werkseinstellungen:** setzt Standard-Sondentyp, den Index nach Loetbruecken und Tx-Flags wieder her, Software-Rtrim auf 0. Gespeicherte Bitrate bleibt erhalten. Hardware-ID bleibt unveraendert.
- **10s identifizieren:** beide LEDs des adressierten Moduls blinken abwechselnd fuer 10 Sekunden, danach normale Statusanzeige.
- **Neustart:** echter adressierter MCU-Reset. Das unterscheidet sich vom bisherigen ECU-Tool `Restart all WBO`, das die Heizfreigabe kurz unterbricht. Nach einem Bitratenwechsel Adapter passend neu verbinden und erneut erkennen.

Antworten enthalten Hardware-ID im CAN-Identifier sowie Befehl, Transaktion und Ergebnis. Das verhindert Verwechslungen mit anderen Serviceantworten; es kann doppelte Hardware-IDs oder konkurrierende Konfigurationsprogramme nicht aufloesen. Nur ein Konfigurationswerkzeug gleichzeitig verwenden.

## Selectable Rtrim aus dem anderen Quellstand

Die Integration verwendet den **aktuellen Source-Snapshot**, nicht die aeltere Beschreibung im urspruenglichen Kalibrier-Artefakt. Details und Datei-Hashes: `rtrim-source-manifest.json`.

- **LSU 4.9** (Typ 0): normaler Bosch-Betrieb mit physischem Trimwiderstand; Softwarekorrektur wirkt nicht.
- **LSU 4.9 Rtrim SW** (Typ 4): fuer originale Bosch-LSU4.9 mit fehlendem Stecker-Trimwiderstand. Der gespeicherte sondenspezifische Widerstand wirkt auf die Pumpstromumrechnung vor der Lambda-Berechnung.
- Einstellbar: **30.00..300.00 Ohm in 0.01 Ohm**. Den tatsaechlichen gelabelten Sondenwert eingeben, keinen Beispielwert uebernehmen. Im Softwaremodus darf der physische Trimwiderstand nicht zusaetzlich vorhanden sein.
- Ablauf: F0 erkennen / lesen; danach im Hauptfenster `LSU 4.9 Rtrim SW` waehlen; Tools-Ansicht `Rtrim / Sensorauswahl`; eigenen Ohmwert eingeben; `Rtrim speichern`. Wert und Modus werden gemeinsam gespeichert und anschliessend gelesen. **Lesen zuerst ausfuehren**, da es die Felder aus dem Modul aktualisiert.
- `Physischer Rtrim` waehlt normalen Bosch-LSU4.9-Betrieb und Softwarewert 0. Das setzt voraus, dass der physische Widerstand vorhanden ist.
- Ein gespeicherter Ohmwert allein aktiviert den Softwaremodus nicht. Andere Sensortypen verwenden die Korrektur nicht. Beim Wechsel zwischen Sondenfamilien Modul neu starten: Die Heizparameter werden vom bestehenden Code beim Threadstart geladen. Softwaremodus ohne gueltigen Wert liefert kein gueltiges Lambda. Die Manager-Konfiguration verweigert den Wechsel auf Typ 4, solange kein passender Wert gespeichert ist.
- Der Kalibrierrecord liegt kompatibel im bisherigen Konfigurationspadding bei Offset 200; Struktur bleibt 256 Byte gross. Bestehender Tag und Feldpositionen bleiben erhalten. Modus und Wert werden nach Flash-Readback aktiv und nach Neustart wieder geladen. Update allein stellt den Sondenmodus nicht um.
- Der ADC-Pumpstrom wird im Softwaremodus mit `Rtrim / (Rtrim + 61.9 Ohm)` korrigiert; die Stromtelemetrie folgt derselben Korrektur. Regelalgorithmen und Kennlinien stammen aus der vorhandenen Kalibrier-Erweiterung. Strommessbereich und Referenz-Lambda ohne physischen Parallelwiderstand muessen noch am Pruefstand validiert werden.

## Heating enforce

Heizfreigabe ist ein **fluechtiger Status-Broadcast**, kein gespeichertes Force-Heating-Setting. Die Anwendung sendet auf ausdruecklichen Start alle 50 ms die echte Versorgungsspannung, Freigabe und Pump Gain (wirksam 0..100%) fuer alle Module. Nicht neben einem anderen ECU-Statussender verwenden. Bei erkanntem ECU-Status oder Empfangsfehler beendet der Manager eigene Heiztelegramme. Stoppen sendet einmal Freigabe=0. Trennen/Schliessen beendet die Telegramme; nach 250 ms gilt die vorhandene Firmware-Timeoutlogik. F0 besitzt fuer diese Funktion keine lokale Versorgungsspannungsmessung und benoetigt den richtigen CAN-Spannungswert. Sensorzustand muss zum Heizen passen.

## Gemeinsame F0-Firmware / CAN-Update

`firmware/wideband_f0_manager_rtrim_app.bin` ist das **CAN-Updateimage**, 25.600 Bytes einschliesslich Padding und CRC, Start `0x08001800`. Ausschliesslich dieses App-Image im Manager auswaehlen. Der mitgelieferte Bootloader ist **bytegenau gleich** dem bisherigen 500k/1M-selectable Bootloader (SHA256 `900a987bf46bb1af098845a286ebbe8451d983d81441a93de34985d9976ee4e2`). Ein CAN-App-Update ersetzt den installierten Bootloader nicht.

Am aktuellen Modulbitratentakt flashen. Nur ein Wideband-Modul am Updatebus, stabile Versorgung. Die F0-Bootloaderbefehle sind Broadcast. Das Image wird vor dem Loeschen auf Groesse, Vektoren und CRC geprueft. Danach Erase, quittierte 8-Byte-Bloecke und Reboot. Erfolg bedeutet quittierte Uebertragung; kein Flash-Readback oder CRC-Ergebnis-Telegramm aus dem Bootloader. Danach Pong, Konfiguration und Messwerte kontrollieren. Fehler kann Bootloader-/SWD-Wiederherstellung erfordern.

Full-BIN/HEX und Bootloader-BIN liegen getrennt bei. Full ab `0x08000000`, Laenge `0x7c00`; Konfigurationsseite `0x08007c00..0x08007fff` nicht enthalten. Full-Image ist fuer SWD, nicht den CAN-Appdialog. Wer bereits den passenden selectable Bootloader hat, benoetigt keinen neuen Bootloader. Alte feste Bootloader erfordern fuer vollstaendige Bitratenfunktion den gesonderten Umstieg ueber SWD.

Flash-/RAM-Layout wurde nicht vergroessert. Die Anwendung belegt 20.748 Bytes, 4.848 Bytes bleiben vor der CRC frei. Originale Anwendungs-Stackbudgets: Hauptthread/Interrupts je 1024 Byte, CAN-Rx-Nutzstack 512 Byte plus ChibiOS-/Port-Verwaltung. ELF-gemessene Heapreserve 16 Byte (urspruenglicher Ausgangsbuild 24 Byte); die Stacks sind dabei bereits reserviert. Keine neue dynamische Allokation wurde eingefuehrt. Die Rueckfallsicherung umfasst 12 Byte, nicht eine zweite Konfigurationsstruktur. Sie stellt bei Fehlern nur den laufenden RAM-Zustand wieder her, keine bereits geloeschten Flashdaten.

`-Os` und Alignment 1 bleiben: Vergleichsbuilds mit `-O2` passen selbst nach der Speicherueberarbeitung nicht in den Anwendungsbereich (Alignment16: 1616 Byte zu gross, Alignment1: 1044 Byte zu gross; CRC-Reserve kommt hinzu). Der Compilerbericht nennt fuer CanRx einen lokalen Frame von 112 Byte und bekannte direkte Aufrufpfade bis 264 Byte; Bibliotheks-/indirekte Aufrufe und Hardware-/RTOS-Kontextframes sind damit nicht vollstaendig begrenzt. Das ist kein maximaler Stacknachweis. Die urspruengliche Stackpruefung bleibt eingeschaltet; Hochwassermarken und echte Laufzeiten sind nicht gemessen.

Konfiguration/Rtrim/Bitrate teilen weiterhin die bisherige Flashseite. Stromausfall beim Loeschen/Schreiben kann Einstellungen verlieren; dann gelten Fallbacks inklusive 500 kbit/s. Lesen, Graphen, Logging und Backups auf dem PC schreiben diese Seite nicht.

## Build und Nachweise

Desktop: `./build.sh` mit MinGW-w64; alternativ `CC_WINDOWS=/pfad/x86_64-w64-mingw32-gcc-posix ./build.sh`. Keine .NET-/Python-Runtime auf Windows erforderlich.

Firmware: Abhaengigkeiten ChibiOS `a8865dc84b114e74f62d8e470458839da71d333b`, libfirmware `39b3a04dab617e7ff24577e99d0d7fac2ce44c9f`. In diesem Workspace bindet `./prepare-firmware-deps.sh` die bereits vorhandenen geprueften Checkouts ein. Andernorts `WIDEBAND_DEP_SOURCE=/pfad/zu/wideband_checkout` setzen. Es wird nichts gepullt oder in bestehenden Checkouts umgeschaltet. Dann `ARM_TOOLCHAIN=/pfad/arm-gnu/bin ./build-firmware.sh`; ARM GNU 14.3.Rel1 erforderlich; das crc32-Hilfsprogramm liegt unter `tools/`. Das ZIP enthaelt die fuer den konkreten F0-Build ermittelten Bibliotheksquellen, Header und Builddateien sowie Lizenztexte; andere Plattformen dieser Bibliotheken sind nicht enthalten. `prepare-firmware-deps.sh` ist nur fuer das Einbinden vollstaendiger Git-Checkouts gedacht. Wideband-MIT-Lizenz liegt unter `source/firmware-f0/LICENSE`.

Pruefungen: `./tests/run.sh`, danach `python3 verify_release.py` und `python3 analyze_stack.py`. Hosttests pruefen echte Service-/Konfigurationsfunktionen, den echten Client-Zustandsautomaten gegen den Firmware-Dispatcher, Rtrim und Restore, falsche Antworten und Flashfehler; ausserdem echte 256-Byte-Konfiguration mit kleiner Rueckfallsicherung, Erhalt unbeteiligter Bytes, No-op-Speicherung und Wiederherstellung nach Erasefehler; ausserdem Backup-CRC/Werte, SLCAN/Images, Mehrkanalauswertung, Aufheizmessung, Datenluecken und Ringpuffer. Uebernommene Tests pruefen echte F0-Sondenselektion, Kalibrierrecord/Bitkorruption, Flash-Readback, beide Bitraten und ADC-/Lambdaumrechnung. HAL und CAN-Bus sind dabei simuliert. ASan/UBSan fuer Protokoll/Auswertung, UBSan fuer C++-Tests; LeakSanitizer ist wegen ptrace-Umgebung deaktiviert.

Bekannte Buildwarnungen: bestehende nicht deklarierte shared-Region, RWX-Segment und LTO-Assemblerlisting-Optionen. `validation/` enthaelt Logs, Imagepruefung und Speicherbericht. Vor Hardwarefreigabe fehlen Windows-/DPI-/Adaptertests, reale Flashtransaktionen, Wiederherstellung und Regel-/Stack-/Timingpruefungen bei beiden Bitraten. Keine neuen anderen Modulschnittstellen, AEMNet-, EGT- oder Analogausgangsfunktionen.

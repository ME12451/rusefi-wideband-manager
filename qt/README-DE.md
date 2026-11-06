# rusEFI Wideband Manager 0.6 – Windows und Linux

Version 0.6 verwendet erstmals dieselbe Qt/PySide6-Oberfläche und dieselben Konfigurations-, Diagnose-, Backup- und Updateabläufe unter Windows und Linux. Der bisherige C-Kern für Protokoll, Messwertauswertung, Graph-Skalierung, CRC und Backupformat bleibt erhalten. Die Modulfirmware ist unverändert.

## Start

**Windows x64:** ZIP vollständig entpacken und `rusEFI-Wideband-Manager.exe` starten. Die Ordner `app` und `runtime` müssen neben der EXE bleiben. Python und Qt sind enthalten. PCAN benötigt weiterhin den passenden PEAK-Treiber und die 64-Bit-PCANBasic-DLL. Diese kann neben die EXE gelegt werden oder über die offizielle PEAK-Installation verfügbar sein. Ziel sind aktuelle Windows-10-/11-Systeme; getestet wurde unter WineHQ 11.21, nicht auf einem physischen Windows-Rechner.

**Linux x86_64:** Archiv vollständig entpacken und `./rusEFI-Wideband-Manager` starten. `_internal` muss neben dem Programm bleiben. Python und Qt sind enthalten. Das ausgelieferte Binärpaket enthält Systembibliotheken aus Ubuntu 26.04 und benötigt **glibc 2.43 oder neuer**; GUI-Test unter Ubuntu 26.04.1/X11. Für ältere Distributionen ist ein eigener Build aus dem beiliegenden gemeinsamen Quellcode erforderlich. Wayland und Mehrmonitor-DPI wurden nicht geprüft.

Explizite Simulation ohne Hardware:

```sh
./rusEFI-Wideband-Manager --demo
```

Unter Windows entsprechend `rusEFI-Wideband-Manager.exe --demo`. Simulierte Werte sind deutlich markiert. CAN-Senden, Konfiguration, Heizfreigabe und Flashen bleiben im Demo-Modus gesperrt. Beim normalen Start werden keine synthetischen Messdaten erzeugt.

## Sprache

Oben rechts im Dropdown **English / Deutsch** auswählen. Die Oberfläche wird sofort umgeschaltet: Bedienelemente, Graphtexte, Konfigurationsbericht, eigene Meldungen und Bestätigungen. Die Auswahl wird pro Benutzer gespeichert und beim nächsten Start wiederhergestellt. Sprache wechseln setzt keine Messdaten zurück und verändert keine ungespeicherten Eingaben oder CAN-Einstellungen. Auch die pausierte Graph-Historie bleibt erhalten. Betriebssystem-/Treiberfehler können technische Originaltexte enthalten. Produktnamen und CAN-Bezeichner werden nicht übersetzt.

CSV-Spalten, Zahlenformat und das Backup-Dateiformat bleiben sprachunabhängig identisch. Dezimaleingaben für Versorgungsspannung und Rtrim akzeptieren Punkt oder Komma.

## Gemeinsame Funktionen

| Bereich | Funktion |
|---|---|
| Live-Anzeige | Lambda, Sondentemperatur, Heizleistung und Nernst DC; Messindex 0..255 |
| Mehrmodul-Dashboard | Empfangene Messindizes mit Status, sechs Messwerten, Datenalter und beobachteter Aufheizzeit |
| Live-Graph | Lambda, Temperatur, Heizung %, Pump PWM %, Nernst DC V und ESR Ohm; vier synchronisierte Diagramme |
| Bedienung | 15/30/60/300 Sekunden, feste/automatische Skalen, Cursor, Pause, Historie verschieben, Zoom und CSV |
| F0-Konfiguration | Boardkennung sowie Index, Sensortyp, gespeicherte/aktive Bitrate und TX-Optionen lesen; Schreiben mit Rücklesung |
| Rtrim | Selectable Rtrim lesen/speichern; Bosch-Softwaremodus oder physischen Trimwiderstand wählen |
| Backup | Windows-/Linux-kompatibles WBF0-v2-Format mit CRC; Vergleich vor Wiederherstellung und verifizierte Schreibschritte |
| Modulsteuerung | LED-Identifikation, Neustart, Werkseinstellungen; gespeicherte Bitrate bleibt bei Werkseinstellungen erhalten |
| Zusatzdiagnose | Momentaufnahme: Pumpstrom Soll/Ist, Nernst AC, effektive Heizspannung, CAN-Versorgung und Heizzustand |
| CAN-Werkzeuge | Rohmonitor mit ID-/STD-/EXT-Filter und Anzeige-Pause, CAN-Rohdaten-CSV, Statistik und Ereignisse mit Export |
| Legacy-Einstellungen | Separate Index-/Sensor-/Bitratenbefehle für passende ältere Firmware; keine Konfigurationsrücklesung über Legacy-ACKs |
| Firmwareupdate | Bestehender F0-CAN-Bootloaderablauf mit Imageprüfung und Fortschritt |
| Prüfstand | Heizfreigabe als Broadcast alle 50 ms mit Ist-Versorgung und Pump Gain; Stopp bei ECU-Sender oder CAN-Fehler |

Es gibt ausschließlich CAN-Kommunikation zum Modul. Keine EGT-, AEMNet-, Analogausgangs- oder TunerStudio-Anbindung.

## CAN-Adapter

Windows: PCAN über `python-can`/PCANBasic, beispielsweise `PCAN_USBBUS1`, oder lokaler SLCAN-Port, beispielsweise `COM3`.

Linux: SocketCAN, beispielsweise `can0`, oder lokaler SLCAN-Port, beispielsweise `/dev/ttyUSB0`. SocketCAN-Bitrate vor dem Verbinden außerhalb des Programms einstellen. Das Programm führt kein sudo aus und verändert keine Linux-Netzwerkschnittstellen. Beispiel für den eigenen Prüfstandsadapter:

```sh
sudo ip link set can0 down
sudo ip link set can0 type can bitrate 500000
sudo ip link set can0 up
```

Für 1 Mbit/s `1000000` verwenden. Das Bitratenfeld im Programm initialisiert PCAN oder direktes SLCAN; es konfiguriert keine SocketCAN-Schnittstelle. Nach Änderung der gespeicherten Modulbitrate ist ein Modulneustart nötig; anschließend Adapter-/Interface-Bitrate passend einstellen.

Direktes SLCAN nutzt `pyserial`, Lawicel-Befehle und 115200 Baud/8N1 mit geprüfter Initialisierung. Lokale Portberechtigung erforderlich; nicht gleichzeitig mit slcand oder anderen Tools öffnen. Die serielle Verbindung dient nur dem CAN-Adapter. Keine Remote-Serial-URLs. Bei hohem CAN-Verkehr begrenzt der serielle Durchsatz die Aufzeichnung; SocketCAN/PCAN bevorzugen.

## Graph und Daten

Mausrad bzw. +/- ändert das Zeitfenster. Leertaste oder Pause friert eine PC-Kopie ein; Empfang läuft weiter. Ziehen oder Umschalt+Mausrad verschiebt die Historie. Home bzw. Weiter / Live kehrt zur Liveansicht zurück. Cursorwerte beziehen sich auf den nächstliegenden gespeicherten Messpunkt.

Maximal 10 Hz und 3000 Punkte je Messindex. Zeitachse: PC-Laufzeit seit Programmstart, keine Modulbetriebszeit. Standard-A0-Telegramme liefern Lambda/Temperatur; Diagnosetelegramme liefern Heizung/Pump PWM/Nernst DC/ESR. Fehlende/veraltete Werte werden als Lücken dargestellt; ungültiges Lambda nicht als gültige Kurve. Für Diagnosekurven muss Diagnose-TX aktiv sein. Zusatzdiagnoseabfragen bleiben getrennte Momentaufnahmen.

CSV-Zeitfenster exportiert den gewählten Index und das sichtbare Zeitfenster; pausiert wird die eingefrorene Kopie exportiert. Quelle CAN/DEMO und Gültigkeit/Frische sind eigene Spalten; nicht verfügbare Werte bleiben leer. Rohmonitorfilter und dessen Pause beeinflussen weder Empfang noch Rohdaten-CSV. Ereignisse und Aufheizzeiten sind begrenzte Sitzungsbeobachtungen, keine im Modul gespeicherte Fehlerhistorie. CAN-Raten beruhen auf PC-Zeitstempeln, nicht auf Hardware-Buslast-/Fehlerzählern.

## Konfiguration, Backup und Firmware

Hardware-ID muss am Bus eindeutig sein. Doppelte IDs können nicht sicher unterschieden werden. Neue Servicebefehle ordnen Antworten nach Hardware-ID, Befehl und Transaktionsnummer zu. Vor erweiterten Schreibbefehlen F0 erkennen/lesen; danach erfolgt Rückprüfung. Legacy-Bestätigungen enthalten keine Absender-ID. Keine parallelen Konfigurationstools verwenden.

Rtrim: Eigenen auf der Sonde angegebenen Wert verwenden, 30,00..300,00 Ohm in 0,01-Ohm-Schritten. Nicht raten. Im Softwaremodus darf der physische Trimwiderstand nicht zusätzlich vorhanden sein; Sonde und gewählter Modus müssen passen.

Backups enthalten die zuletzt erfolgreich gelesenen Werte, nicht ungespeicherte Eingaben. Wiederherstellung erfolgt in zwei verifizierten Schritten. Bei Fehler sind Teiländerungen möglich; erneut lesen. Ein Backup mit anderem Sensortyp stellt den Rtrim-Datensatz zunächst im normalen Bosch-Modus wieder her und setzt danach den endgültigen Sensortyp samt übriger Konfiguration. Konfiguration auf dem Prüfstand durchführen.

Firmwareupdate: Genau ein passendes F0-Modul anschließen. Bootloaderbefehle sind Broadcast. Versorgung stabil halten. Akzeptiert wird nur ein 25600-Byte-Anwendungs-BIN mit passenden Vektoren ab 0x08001800 und CRC. Voll-/Bootloaderimages werden abgelehnt. Keine automatische Wiederholung nach Erase-/Datenfehler. Übertragung und ACK ersetzen keine Flash-Rücklesung; nach Neustart Pong/Livewerte prüfen.

**Für Version 0.6 wurde keine Modulfirmware gebaut oder geändert.** Live-Graph und Sprachwahl benötigen kein Flashen. Erweiterte Konfigurationsfunktionen benötigen dieselbe passende F0-Manager-Firmware 0.4 wie zuvor. Unverändertes Anwendungsimage, SHA256:

`9f03e1734c03e658f9715469c975fd756b23b80628ad8ae24b46cbee34855cd3`

## Tests und Grenzen

Gemeinsamer C-Kern mit Warnungen als Fehler gebaut; bestehende Protokoll-/Analytics-/Graph-Tests mit ASAN/UBSAN bestanden. Gemeinsamer Qt-Client gegen den tatsächlichen F0-Dispatcher mit Fake-HAL geprüft: Lesen, Schreiben/Rücklesung, Rtrim, Backup/Restore einschließlich anderer Sensortypen, Werkseinstellungen, Identifikation, Diagnosedekodierung, falsche Antworten und Flashfehler ohne Schreibwiederholung. Adaptertests mit virtuellem CAN-Bus und SLCAN-Pseudoterminal. Imageprüfung und vollständiger Firmwareübertragungsablauf gegen simulierten Bootloader.

Je Windows-EXE und gepacktem Linux-Programm: 28 Demo- und 18 Offline-GUI-Prüfungen, darunter beide Sprachen, Erhalt ungespeicherter Werte und pausierter Historie, identische CSVs, Sprachwechsel während eines laufenden Test-Workers, gespeicherte Sprachauswahl und kleinere Fenster. Übersetzungskatalog und Formatplatzhalter separat geprüft. Logs und Ergebnisdateien liegen bei.

**Keine Tests an physischem CAN-Adapter/F0-Modul oder mit echtem Flashvorgang.** Windows-GUI unter Wine, Linux unter Xvfb/X11. Keine Freigabe für Motorbetrieb aus diesen Softwaretests ableiten.

## Gemeinsamer Quellcode

`source/qt` enthält die gemeinsame Anwendung. Nur die Transportauswahl und die Startpaketierung unterscheiden sich nach Betriebssystem. `source/src` enthält den unveränderten gemeinsamen C-Kern; `libwideband.dll` und `libwideband.so` sind dessen jeweilige Builds. CSVs und Backups sind in beiden Programmen gleich.

Aus Quellcode: Python-Umgebung mit `qt/requirements.txt`, C-Kern über `qt/build_core.sh` bauen, dann `python qt/main.py` starten. Linux-Paketierung über `qt/build_linux.py` mit PyInstaller 6.19.0. Der Windows-Build verwendet den kleinen C-Launcher und eine lokale embeddable-Python-Laufzeit; `qt/build_windows.py` beschreibt die reproduzierbare Paketvorbereitung. Jede Plattform benötigt ihre passenden Binärabhängigkeiten. Gemeinsame Source-ID und SHA256-Dateilisten stehen im Manifest. Drittanbieter-Lizenzhinweise liegen unter `licenses`.

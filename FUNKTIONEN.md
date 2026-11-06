> Historischer Vergleich bis Firmware 0.4. Fuer die neue HW-/Index-Adressierung, Bedienung und Speicherwerte gilt [Release 0.5.3](RELEASE-0.5.3.md).

# Funktionen: F0 CAN Manager 0.4 und gemeinsame Rtrim-Firmware

Ausschliesslich STM32F042/f0_module und CAN. PCAN-USB sowie SLCAN/Lawicel sind Adapterwege zum CAN-Bus; keine serielle Modulschnittstelle. Kein AEMNet-, EGT- oder Analogausgangsangebot. Die CAN-Serviceversion bleibt 1 und ist zur Manager-Erweiterung 0.3 kompatibel.

## Neue oder erweiterte Funktionen im Modul

| Funktion | Konkretes Verhalten / Nutzen | Voraussetzung und Bedienung | Speicherung / Auswirkung |
|---|---|---|---|
| F0 erkennen | Meldet F0-Ziel, Protokollversion, unterstuetzte Funktionen und einen Wideband-Kanal | Tools -> F0 erkennen / lesen; Ziel-HW-ID im Hauptfenster | Nur Antwort, kein Flashschreiben; Firmware-Zielkennung ist keine ausgelesene Geraeteseriennummer |
| Konfiguration lesen | Liest Messindex, Sensortyp, Tx-Flags sowie gespeicherte und aktive Bitrate | Gemeinsame Firmware; erkennbare, eindeutige Hardware-ID | Nur lesen; aktive Bitrate ist der beim Start geladene Wert |
| Hardware-ID/Index zuordnen | Die gezielte Konfigurationsantwort verknuepft Loetbruecken-ID mit Messindex | Jedes Modul einzeln ueber seine eindeutige HW-ID abfragen | Keine automatische Loetbruecken-Aenderung; doppelte HW-IDs bleiben untrennbar |
| Konfiguration schreiben und pruefen | Schreibt die F0-Einstellungen, verifiziert den Flash im Modul und fragt anschliessend die Konfiguration ab | Erst erkennen; Felder im Hauptfenster, Tx-Checkboxen im Konfigurationsfenster | Persistiert; unveraenderte Werte werden nur dann ohne Erase bestaetigt, wenn Flash und Bitratenrecord tatsaechlich passen |
| Kleine Rueckfallsicherung | Bei fehlgeschlagener Konfigurationsspeicherung werden Index, Typ, Tx-Flags und Rtrim im laufenden RAM zurueckgesetzt | Bestandteil des neuen Schreibbefehls | 12-Byte-Snapshot im Stack statt kompletter 256-Byte-Kopie; kein zweites Flashbackup und kein Stromausfallschutz |
| Zugeordnete Antworten | Antwort-ID enthaelt HW-ID, Payload enthaelt Befehl, Transaktion und Ergebnis | Neue Servicebefehle auf 0x0EFB0000 / 0x0EFC0000\|HW | Verhindert typische ACK-Verwechslungen; keine Authentifizierung und kein Schutz gegen doppelte HW-IDs |
| Standard-Tx schalten | Sendet oder unterdrueckt Lambda, Gueltigkeit, Temperatur und Sensorflags | Neue Konfiguration, standardmaessig an | Persistiert; AUS nimmt der ECU die Lambdawerte |
| Diagnose-Tx schalten | Sendet oder unterdrueckt ESR, Nernst-DC, Pump-/Heiz-PWM und Status | Neue Konfiguration, standardmaessig an | Persistiert; AUS spart Busverkehr, entfernt aber regelmaessige Diagnosewerte |
| Gespeicherte/aktive Bitrate vergleichen | Zeigt einen vorgemerkten Wechsel vor dem Reset | 500 kbit/s oder 1 Mbit/s | Schreiben war im selectable Stand schon vorhanden; Aenderung erst nach Reset aktiv |
| Selectable Software-Rtrim | Eigener Typ 4: LSU4.9 Rtrim SW. Korrigiert abgeleiteten Pumpstrom vor Lambda mit R/(R+61.9 Ohm) | Originale Bosch LSU4.9 ohne physischen Trimwiderstand; tatsaechlichen Sondenwert angeben | Persistiert als kompatibler Record bei Offset 200; wirksam nur bei Typ 4 |
| Rtrim lesen/schreiben | Wert 30.00..300.00 Ohm in 0.01-Ohm-Schritten, dazu Bosch-Modus; Antwort und Ruecklesung | Erst erkennen/lesen; danach im Hauptfenster Typ 4 waehlen und im Rtrim-Fenster Wert speichern | Wert und Bosch-Modus werden gemeinsam gespeichert; unveraenderte Werte vermeiden Erase |
| Physischer Rtrim | Setzt normalen Bosch-LSU49-Modus und Softwarewert 0 | Physischer Trimwiderstand muss vorhanden sein | Persistiert; Softwarekorrektur aus |
| Kalibrierbereitschaft | Typ 4 ohne gueltigen Nichtnull-Wert liefert kein gueltiges Lambda | Rtrim-Abfrage zeigt bereit/fehlt | Keine automatische komplette Heizungssperre; andere Sondentypen bleiben von Rtrim unberuehrt |
| Erweiterte Diagnose | Nernst-AC, Pumpstrom Soll/Ist, effektive Heizspannung, empfangene CAN-Versorgung, Heizzustand | Diagnose lesen; zwei gezielte Antworten mit Datenalter | Momentaufnahme; keine zusaetzlichen Sensoren. Strom Ist ist ADC-abgeleitet und folgt der Rtrim-Korrektur |
| LED-Identifikation | Adressiertes Modul blinkt 10 Sekunden mit beiden LEDs abwechselnd | Tools -> 10s identifizieren | Nur Laufzeit, anschliessend normale Statusanzeige; Regler werden nicht absichtlich umgeschaltet |
| Gezielter MCU-Neustart | Antwort, danach echter Prozessorreset | Bestaetigte Serviceaktion | Messunterbrechung; vorgemerkte Bitrate wird aktiv. Anders als das bisherige TS-Tool zum kurzen Sperren der Heizfreigabe |
| F0-Werkseinstellungen | Index nach Loetbruecken, Firmware-Sensordefault, Standard-/Diagnose-Tx an, Rtrim 0 | Bestaetigte Serviceaktion; vorher Backup | Gespeicherte Bitrate bleibt; unbeteiligte Legacy-Felder bleiben in 0.4 erhalten |

## Funktionen auf dem PC

| Funktion | Anzeige / Bedienung | Datenbasis / Grenzen | Einfluss auf Modul |
|---|---|---|---|
| Mehrmodul-Dashboard | Gleichzeitige Anzeige aller beobachteten Indizes mit Lambda, Temperatur, Status, PWM, ESR und Alter | Vorhandene Standard-/Diagnoseframes; HW-Zuordnung nur nach Abfrage | Passiver Empfang |
| Live-Verlaeufe | Lambda, Temperatur, Heater-/Pump-PWM, Nernst-DC oder ESR; 30 s / 60 s / 5 min | Gewaehlter Messindex; bis 3000 Punkte je Index, max. 10 Hz; ungueltiges Lambda rot, Datenluecken nicht verbunden | Speicherung im PC-RAM, kein MCU-Flash |
| Aufheizdiagnose | Beobachtete Zeit Warmup -> Closed loop | Kein erfundener Wert bei Einstieg in laufende Sonde; PC muss den Ablauf beobachten | Passive Auswertung, kein neuer Heizalgorithmus |
| Ereignisse und Warnungen | Zustandswechsel, Heizfehler, Ueber-/Untertemperatur, Datenverlust und ungueltiges Lambda bei Closed loop | Letzte 1024 Ereignisse dieser Sitzung, exportierbar; kein dauerhaftes Fehlerarchiv im Modul | Keine Flashschreibvorgaenge |
| CAN-Rohmonitor | STD-/EXT-Datenframes, RX/TX, ID, DLC und Bytes | Kein CAN-FD-, Remote- oder Error-Frame-Decoder | Empfang plus Anzeige eigener gesendeter Frames |
| CAN-Filter/Pause | Einzelne hexadezimale ID, STD/EXT, Monitorpause | Nur sichtbarer Rohmonitor; Auswertung und Logging laufen weiter | Keine Filterprogrammierung im Modul |
| CAN-Statistik | RX/TX gesamt; pro RX-ID Anzahl, mittlere Rate, groesster Abstand und Alter | Bis 2048 IDs; PC-Zeitstempel, keine echte Buslast-/Controllerfehlerzaehlung | Passive Auswertung |
| CSV-Aufzeichnung | Rohe CAN-Frames fortlaufend; Verlauf/Ereignisse/Statistik separat exportieren | Zeitstempel aus PC-Verarbeitung, Export von Ringpuffern begrenzt; bei hoher Last keine Verlustfreiheitsgarantie; SLCAN/115200 ist bei mehr als zwei voll sendenden F0 ein Engpass | Dateien auf PC, kein MCU-Flash |
| Konfigurationsbackup | .wbf mit Board-/Formatkennung, Index, Sensor, Bitrate, Tx-Flags, Rtrim und CRC | Sichert zuletzt gelesene Werte; Alter der Ruecklesung sichtbar | Datei auf PC, kein vollstaendiges MCU-Abbild |
| Backupvergleich | Unterschiede zwischen gelesener Modulkonfiguration und geladener Datei | Datei laden schreibt noch nichts; Ziel-Hardware muss passen | Nur PC-Vergleich |
| Restore mit Ruecklesung | Erst Rtrim/Modus, dann restliche Einstellungen; beide Gruppen pruefen | Bestaetigte Aktion mit zwei Speicherschritten, nicht atomar | Schreibt Modulflash; bei Fehler sind Teilaenderungen moeglich |
| CAN-Firmwareupdate | F0-App-BIN auf Groesse, Vektoren und CRC pruefen; dann Bootloadertransaktion mit ACK pro Block | Nur ein Modul am Updatebus, aktuelle Bitrate, Application-BIN 25600 Byte | Ersetzt Anwendung, nicht Bootloader/Konfigurationsseite; Hardwaretest steht aus |
| Heating enforce | Auf Knopfdruck alle 50 ms Heizfreigabe, echte Versorgungsspannung, Pump Gain | Broadcast auf alle Module, nicht neben anderem ECU-Statussender | Laufzeitsteuerung, kein persistentes Force-Heating-Setting |

## Was bereits vorher existierte

| Bereits vorhanden | Erweiterung durch den Manager |
|---|---|
| Ping mit Hardware-ID, Protokollversion und Builddatum | Explizite F0-/Capability-Erkennung und Konfigurationsabfrage |
| Index und Sensortyp setzen | Neue zugeordnete Antworten und Ruecklesung; Typ 4 aus der Kalibrier-Erweiterung |
| 500k/1M-Bitrate speichern im selectable Stand | Anzeige aktiv vs. gespeichert |
| Regelmaessige Lambda-/Temperatur-/Diagnoseframes | Dashboard, Verlaeufe, Statistiken, Ereignisse und CSV auf dem PC |
| Heizfreigabe und Spannung von ECU ueber CAN | Optionaler PC-Sender fuer Service-/Pruefstandbetrieb |
| F0-CAN-Bootloader | Eigener Windows-Dateidialog und Imagepruefung; Bootloader unveraendert |

Die neuen Funktionen sind implementiert und hostgetestet. Windows-/Adapterlauf, Referenz-Lambda, ADC-Messbereich, reale maximale Stackbelegung und Regel-/Interruptlaufzeiten sind nicht am Modul validiert.

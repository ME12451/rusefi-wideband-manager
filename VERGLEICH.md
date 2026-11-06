> Historischer Vergleich bis Firmware 0.4. Fuer die neue HW-/Index-Adressierung, Bedienung und Speicherwerte gilt [Release 0.5.3](RELEASE-0.5.3.md).

# Vergleich: Ausgangsstand, 0.3 und 0.4

Bezugsquellen: Ausgangsstand `27e115b7faa136d7d1b0554e1694322daca983b9`, bisheriger selectable Stand `64cdf9f97ff9a1110ce3e7a47eb22ebdebe4bf86`. Keine Aussage ueber einen neueren Online-Upstream. Rtrim stammt aus dem getrennten Snapshot `wideband-software-calibration`; dessen Dateihashes sind festgehalten. Bestehende ECU-/Wideband-Checkouts wurden nicht umgeschrieben.

| Bereich | Bisheriger selectable Stand | Gemeinsame Version 0.3 | Gemeinsame Version 0.4 |
|---|---|---|---|
| Bootloader | 500k/1M selectable | Byteidentisch | Weiterhin byteidentisch; Buildoptionen von Anwendung getrennt |
| Anwendungs-Hauptthread-Stack | 1024 Byte | 512 Byte | **Wieder 1024 Byte** |
| Anwendungs-Interrupt-Stack | 1024 Byte | 512 Byte | **Wieder 1024 Byte** |
| CAN-Rx-Nutzstack | 512 Byte | 1024 Byte | **Wieder 512 Byte** |
| Konfigurationskopie fuer neuen Service | Nicht vorhanden | Ganze 256-Byte-Struktur | **12-Byte-Snapshot der betroffenen F0-Felder** |
| Neuer Antwortcache | Nicht vorhanden | Letzter Request/Reply im RAM | Entfernt; unveraenderte Settings samt Flashinhalt werden vor Erase geprueft |
| Werkseinstellungen | Internes LoadDefaults | Setzt auch ungenutzte Layoutfelder zurueck | Nur F0-Index, Sensor, Tx-Flags und Rtrim; Bitrate und andere Layoutfelder bleiben |
| App-Compiler | -O2, Alignment16 | -Os, Alignment1 | -Os, Alignment1 beibehalten, weil beide -O2-Vergleiche zu gross sind |
| App-Flash belegt | 23952 Byte | 20648 Byte | 20748 Byte |
| Frei vor App-CRC | 1644 Byte | 4948 Byte | 4848 Byte |
| Linker-Heapreserve | 24 Byte | 512 Byte durch kleinere Hauptstacks | 16 Byte bei urspruenglichen Stackbudgets |
| Persistente Struktur | 256 Byte | 256 Byte; Rtrim in bisherigem Padding | Unveraendertes Layout |
| Standard-/Diagnoseframes | Schon vorhanden | Layout/IDs/Skalierung/Frequenz bleiben; SW-Rtrim-Sensorflag ergaenzt | Wie 0.3 |
| Neue Servicefunktionen | Nicht vorhanden | Konfiguration lesen, zugeordnete Antworten, Zusatzdiagnose, Identifikation, Reset, Defaults | Vollstaendig erhalten; kleinerer Speicherbedarf des Schreibpfads |
| Selectable Rtrim | Nicht vorhanden | Typ 4, 30..300 Ohm, Korrektur vor Lambda | Vollstaendig erhalten, normaler Typ 0 bleibt unkorrigiert |
| PC-Funktionen | Nicht Bestandteil der Modulfirmware | Dashboard, Verlaeufe, Aufheizdiagnose, Ereignisse, Filter/Statistik, Backup/Restore, CSV | Vollstaendig erhalten; EXE als Version 0.4 gebaut |

Gleiche Struktur-/Flashbereiche: Bootloader 0x08000000..0x080017ff, App 0x08001800..0x08007bff, Konfiguration 0x08007c00..0x08007fff. Gepaddetes App-BIN bleibt 25600 Byte. Der Bootloader hat seine eigenen unveraenderten 512-Byte-Stacks; die wiederhergestellten 1024-Byte-Stacks betreffen die Anwendung.

Die Pumpen-/Heizreglerquellen und Regelperioden wurden fuer 0.4 nicht veraendert. Die Rtrim-Erweiterung veraendert im Typ 4 die Pumpstromumrechnung mit R/(R+61.9 Ohm); normale Sensortypen haben Faktor 1. Typ 4 nutzt die LSU49-Heiz-/Temperaturkennwerte. Die Konfiguration wird vor Threadstart geladen. Fehlender SW-Rtrim liefert ungueltiges Lambda, ist aber keine vollstaendige Heizungssperre.

Die LED-Identifikationsfunktion aus 0.3 bleibt, einschliesslich der geaenderten Hauptschleife. Deren normale Warmup-Blinkkante ist weiterhin 100 statt urspruenglich 50 ms; das ist eine sichtbare Abweichung und keine beabsichtigte Regleraenderung.

Der gepruefte ECU-Decoder verwendet `Valid & 1` und kann das zusaetzliche SW-Rtrim-Flag ignorieren. Alte Tools stellen Typ 4 und den neuen Serviceumfang nicht automatisch dar. Eine allgemeine Zusage fuer fremde CAN-Decoder ergibt sich daraus nicht.

0.4 reduziert die von mir eingefuehrten Speicher-/Stackaenderungen, ist aber weiterhin ohne Hardwarefreigabe. Ausfuehrliche Funktionsbeschreibung: `FUNKTIONEN.md`. Verbleibende Probleme, Stackgrenzen, Vergleichsbuilds und Pruefumfang: `RISIKEN.md`.

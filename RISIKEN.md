> Historischer Vergleich bis Firmware 0.4. Fuer die neue HW-/Index-Adressierung, Bedienung und Speicherwerte gilt [Release 0.5.3](RELEASE-0.5.3.md).

# Erneute Bewertung fuer Version 0.4

Stand: 2026-11-04. F0 / STM32F042, CAN-only. Hardware ist nicht angeschlossen und wurde nicht geflasht. Die Aenderungen beseitigen die von mir eingefuehrte Verkleinerung der Anwendungs-Haupt-/Interrupt-Stacks. Das ist eine Risikoreduktion, keine vollstaendige Hardwarefreigabe.

## Tatsaechlich umgesetzte Speicher-/Buildaenderungen

| Punkt | Version 0.3 | Version 0.4 | Bewertung |
|---|---|---|---|
| Hauptthread-Stack | 512 Byte | 1024 Byte, urspruengliche Groesse | Die zusaetzliche Verkleinerung ist zurueckgenommen |
| Interrupt-Stack | 512 Byte | 1024 Byte, urspruengliche Groesse | Ebenfalls wiederhergestellt |
| CAN-Rx-Nutzstack | 1024 Byte | 512 Byte, urspruengliche Groesse | Vollkopie entfernt; neue Aufrufpfade bleiben hardwareseitig zu pruefen |
| Rueckfallsicherung | Ganze Configuration mit 256 Byte | 12-Byte-Snapshot der betroffenen F0-Felder | Rollback bei Fehler bleibt, ohne Vollkopie |
| Antwortcache | Letzter Request und Antwort in RAM | Entfernt | Weniger dauerhafter RAM-Verbrauch; Settings werden vor Erase verglichen |
| Unveraenderte Einstellungen | Cache verhinderte unmittelbare identische Wiederholung | Kein Erase, wenn RAM und tatsaechlicher Flashinhalt einschliesslich Bitratenrecord passen | Funktioniert auch bei anderer Transaktionsnummer; ein vorheriger Erasefehler wird nicht als Erfolg verdeckt |
| Werkseinstellungen | Vollstaendiges LoadDefaults fuer Legacy-Struktur | Nur die relevanten F0-Felder | Unbeteiligte Felder werden nicht veraendert und brauchen keine Sicherung |
| Firmwarecompiler | -Os, Alignment 1 | -Os, Alignment 1 | Beibehalten: -O2/Alignment16 passt um 1616 Byte nicht; auch -O2/Alignment1 noch 1044 Byte zu gross, jeweils zusaetzlich CRC-Reserve beachten |
| Bootloader | Selectable Stand | Byteidentisch zum bisherigen selectable Image | Buildoptionen sind von denen der Anwendung getrennt; dort bleiben die eigenen urspruenglichen 512-Byte-Stacks |
| App-Flashbelegung | 20648 Byte | 20748 Byte | 4848 Byte frei vor CRC; Layout unveraendert |
| Heapreserve laut Linker | 512 Byte bei kleineren Hauptstacks | 16 Byte bei wiederhergestellten Stacks | Kein allgemeiner RAM-Puffer; Threadstacks sind bereits separat reserviert. Ausgangsbuild hatte 24 Byte. Keine neue dynamische Allokation eingefuehrt |

Compiler-Stackdaten: CAN-Rx lokaler Frame 112 Byte; bekannte direkte Aufrufpfade summieren sich bis 264 Byte. Diese Zahl enthaelt nicht alle Bibliotheks-/indirekten Aufrufe und nicht alle Hardware-/RTOS-Kontextframes. Sie ist daher **keine vollstaendige obere Schranke** und beweist nicht, dass 512 Byte in jeder Situation ausreichen. Die urspruengliche ChibiOS-Stackpruefung bleibt eingeschaltet; sie ersetzt keine Hochwassermessung. Stackfuellung fuer Hochwassermessung wurde nicht nachtraeglich aktiviert.

## Verbleibende Probleme

| Problem / Ausloeser | Was 0.4 verbessert oder unveraendert laesst | Verbleibende Folge | Bewertung / ausstehende Pruefung |
|---|---|---|---|
| Falscher Rtrim oder falscher Sondenmodus | Wertebereich/Record und Ruecklesung werden geprueft; normaler LSU49-Modus hat Faktor 1 | Ein formal gueltiger, aber zur Sonde falscher Wert erzeugt falsches Lambda | Wesentlich fuer Motorbetrieb: eigenen Sondenwert und Referenz-Lambda bestaetigen |
| Physischer Trimwiderstand trotz Softwaremodus | GUI warnt, Modus ist explizit | Doppelte Korrektur, falsche Messwerte | Verdrahtung pruefen; nicht durch Software automatisch erkennbar |
| ADC-Saettigung ohne Parallelwiderstand | Die Korrekturrechnung stammt unveraendert aus dem anderen Chat | Der reale nutzbare fette/magere Messbereich kann kleiner sein | Referenzmessung ueber benoetigten Bereich steht aus |
| Neue CAN-Aufrufpfade auf 512-Byte-Rx-Stack | Vollkopie beseitigt, Compilerbericht erzeugt | Ein nicht abgedeckter tiefer Aufruf kann weiter einen Ueberlauf verursachen | Originalgroesse ist kein Beweis fuer neue Pfade; maximale Nutzung messen |
| Laufzeit unter -Os | Reglerquellen/Perioden unveraendert, Single-Precision-Option bleibt | Andere Maschinenbefehle und Laufzeiten moeglich | ADC-/Regel-/Interrupttiming unter CAN-Last messen |
| Sehr kleiner freier Heap | Keine neuen Heapobjekte; alle Threadstacks statisch | Spaetere dynamische Erweiterungen wuerden kaum Platz finden | 16 Byte sind kein nennenswerter Ausbaupuffer; jetzige statische Belegung ist gelinkt/geprueft |
| Speichern bei laufendem Motor | Kleine RAM-Sicherung, Readback, kein unnoetiger Erase bei gleichen neuen Servicewerten | Flashloeschen/-programmieren kann die Regelarbeit unterbrechen | Nur im Servicebetrieb konfigurieren. Manager erzwingt keinen RPM-Stillstand |
| Schreibfehler / Versorgungsausfall | Betroffene Laufzeitwerte werden im RAM restauriert; Fehlerantwort; nachfolgender identischer Auftrag verifiziert/rekonstruiert den Flash | Geloeschte Flashdaten sind durch RAM-Rollback nicht wiederhergestellt | Nach Fehler nicht blind weiterbetreiben; erst Einstellungen/Bitrate pruefen und sichern |
| Neustart nach Verlust der Flash-Konfiguration | Bestehender Fallback bleibt | 500k und normale Sondendefaults koennen aktiv werden; bei Sonde ohne physischen Rtrim kann der Modus dann falsch sein | Besonders wichtig im SW-Rtrim-Betrieb; nach Speicherfehler/Reset Erreichbarkeit und Modus kontrollieren |
| Restore-Unterbrechung | Zwei verifizierte Schritte und abschliessende Ruecklesung | Nur ein Teil der geladenen Einstellungen koennte gespeichert sein | Nicht atomar; erneut auslesen, kein automatischer Wiederholungsversuch bei unklarem Ergebnis |
| CAN-Hardware-ID doppelt oder konkurrierendes Tool | Neue Antworten passen zu HW/Opcode/Transaktion | Zwei physische Module mit derselben HW-ID bleiben ununterscheidbar | Eindeutige IDs, ein Konfigurationssender; Updatebus mit nur einem Modul |
| Standard-Tx aus, MCU-Reset, Defaults | Nur bestaetigte Aktionen | Lambdawerte fehlen oder Einstellungen passen nicht mehr zur Sonde/ECU | Im normalen Betrieb beide Tx-Flags an; Serviceaktionen nicht waehrend Fahrt/Abstimmung |
| Heating enforce | Eigene Telegramme stoppen bei erkanntem ECU-Sender/Empfangsfehler | Broadcast, falsche Vbat oder falsche Heizsituation kann stoeren | Keine Motor-/Kondensationserkennung im PC; nur passende Serviceumgebung |
| SLCAN mit mehreren Modulen | Transport bleibt 115200 Baud / 8N1 | Zwei 8-Byte-Standardframes alle 10 ms ergeben etwa 44000 UART-Bit/s je Modul; drei Module benoetigen schon etwa 132000 Bit/s ohne weiteren Verkehr | Bei mehr als zwei voll sendenden F0-Modulen ist verlustfreie Vollaufzeichnung ueber diesen Adapterweg nicht moeglich; PC-Auswertung kann Luecken zeigen. Das ist eine Transportgrenze, keine Abschaltung der Module |
| Kein gueltiger Rtrim in Typ 4 | Lambda wird ungueltig | Heizung wird dadurch nicht vollstaendig gesperrt | ECU muss ungueltige Messwerte korrekt behandeln |
| Downgrade oder Sondenfamilienwechsel | Kein automatischer Downgrade; Einstellungen lesbar | Alte Firmware kennt Typ 4 nicht; Heizparameter werden beim Start gesetzt | Passenden Typ/Hardware herstellen, bei Sondenfamilienwechsel neu starten |
| Windows-/Adapter-/Flashtiming | EXE gebaut; Protokoll, Dateipruefung und Clientablauf hostgetestet | Treiber-/GUI-/Hardwarefehler sind nicht ausgeschlossen | Kein erfolgreicher realer Windows-/Adapterlauf und kein Modulflashversuch |

## Durchgefuehrte Pruefungen

| Pruefung | Ergebnis / Aussagegrenze |
|---|---|
| ARM-Build mit urspruenglichen Anwendungs-Stacks | Bestanden; Flash/RAM passen |
| ELF-Pruefung der reservierten Stacks | Haupt/Interrupt 1024 Byte, CAN-Rx-Working-Area 680 Byte fuer 512 Byte Nutzstack plus ChibiOS/Port-Verwaltung |
| Vergleichsbuild -O2 | Flash zu gross; deshalb kein solches Image ausgegeben |
| Compiler-Stack-/Callgraph-Bericht | Vorhanden, mit offengelegten unbekannten Anteilen |
| Tatsachliche F0-Config und Flashhelfer am Host | Kleiner Rollback, Erhalt unbeteiligter Bytes, Defaults, keine Erases bei identischen gueltig gespeicherten Werten, Reparatur nach Erasefehler; beide Bitraten |
| Client/Server-Tests | Lesen, Rtrim, Restore, falsche Antworten und Flashfehler bestanden |
| Bestehende Kalibrier-/Bitratentests | Recordkorruption, ADC-/Lambdaumrechnung, Sondenselektion und Speicherung bestanden |
| Imageformat / CRC / Vektoren / Bootloader | Geprueft; Bootloader byteidentisch |
| Hardware-Messbetrieb | Nicht geprueft; keine Betriebsfreigabe daraus ableiten |

Bewertung: Die vermeidbare Verkleinerung der Haupt-/Interrupt-Stacks ist beseitigt. Das Speichern benoetigt keine grosse Konfigurationskopie mehr und behandelt einen vorherigen Flashfehler besser. Die wesentlichen offenen Betriebsrisiken bleiben reale Rtrim-Messgenauigkeit/-Messbereich sowie Stack-/Timing-/Hardwarevalidierung. Per CAN flashbar beschreibt die Imagekompatibilitaet, nicht die Freigabe fuer normalen Motorbetrieb.

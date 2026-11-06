# Changelog — rusEFI Wideband Manager

Diese Historie beschreibt veröffentlichte und aus vorhandenen Archiven importierte Stände. Sie rekonstruiert keine nicht vorhandenen Zwischencommits. Windows und Linux werden als getrennte schlanke Anwendungen weitergeführt.

## 0.5.3 — Windows und Linux — 2026-11-06

[Downloads und Quellcode-ZIP](https://github.com/ME12451/rusefi-wideband-manager/releases/tag/v0.5.3)

### Gemeinsam

- Erweiterte Modulkonfiguration über **Hardware-ID plus aktuellen Ziel-Datenindex**. Module mit gleicher Hardware-ID lassen sich bei unterschiedlichen Indizes getrennt konfigurieren.
- Lesen und geprüftes Schreiben von Einstellungen und Software-Rtrim; Backup/Restore mit bestehendem WBF0-v2-CRC-Format.
- Geänderte Indizes werden zunächst an der alten Adresse bestätigt und anschließend an der neuen Adresse zurückgelesen.
- Zielwechsel verwerfen zwischengespeicherte Einstellungen, Rtrim und Diagnosedaten; vor erneutem Schreiben muss gelesen werden.
- Falsche oder veraltete Antworten werden verworfen. Kein automatischer Rückfall auf Hardware-ID-only-Konfiguration bei Timeout.
- Diagnoseanzeige unterscheidet tatsächlichen und nominalen Pumpenstrom.
- Legacy-Befehle benötigen weiterhin eindeutige Hardware-IDs. CAN-Firmwareupdates benötigen genau ein angeschlossenes Modul.

### Windows

- Vorhandenen nativen Win32-Stand 0.5.3 übernommen; Quellcode und bisheriges EXE-/Firmwarearchiv unverändert beibehalten.
- Kleine C-Anwendung ohne gebündelte Python-, Qt- oder .NET-Laufzeit.
- PCAN und SLCAN; kompakte Oberfläche, Live-Graph und CAN-Konsole bleiben erhalten.

### Linux

- Bisherige separate GTK3-Anwendung auf den Konfigurationsstand 0.5.3 gebracht.
- Ziel-Datenindex ergänzt; indexbasierte Adressierung, Rtrim-Isolation, Indexwechsel, Rücklesung und Cache-Löschung übernommen.
- Verwendet System-Python, GTK3/PyGObject/Cairo und eine kleine native C-Bibliothek. Keine gebündelte GUI-Laufzeit.
- SocketCAN und direktes SLCAN; bisherige Graph-, CSV- und Diagnosefunktionen bleiben erhalten.
- Rtrim-Eingaben akzeptieren Punkt oder Komma als Dezimaltrennzeichen.

### Firmware und Prüfung

- Keine neue Firmwareänderung durch den Linux-Port. Erweiterte Konfiguration benötigt [indexbasierte F0-Manager-Firmware 0.5.3](https://github.com/ME12451/wideband/releases/tag/manager-f0-0.5.3).
- Tests mit echtem Firmware-Dispatcher und simulierten Modulen: getrennte Rtrim-Werte, Indexänderungen, Restore, falsche Antworten, Flashfehler und Timeouts ohne Wiederholung.
- Linux-Adapter-/GTK-Prüfungen, nativer Windows-Crossbuild, Windows-GUI-Test unter Wine mit Testadapter und GitHub-CI bestanden. Keine physischen CAN-Hardwaretests.

## 0.1 — importierte Windows-Initialversion

[Originalarchiv und Quellstand](https://github.com/ME12451/rusefi-wideband-manager/releases/tag/v0.1.0)

- Ursprüngliche native Windows-Anwendung und zugehörige Quellen aus dem erhaltenen 0.1-Archiv gesichert.
- Als eigener Tag und Release veröffentlicht, damit Initialstand und aktueller Stand nachvollziehbar bleiben.

## Verworfene Qt-Linie

Der zwischenzeitliche gemeinsame Qt-Ansatz 0.6/0.6.1 wurde am 2026-11-06 verworfen. Die entsprechenden Releases und Tags wurden entfernt; Qt-Code ist nicht Bestandteil des aktuellen Quellbaums oder der aktuellen Downloads. Die damaligen Commits bleiben in der Git-Historie nachvollziehbar. Maßgeblich sind die getrennten nativen Versionen 0.5.3.

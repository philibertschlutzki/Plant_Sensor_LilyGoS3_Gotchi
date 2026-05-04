# 🗺️ Feature Roadmap: Plant Sensor LilyGo S3 Gotchi

Diese Roadmap definiert die schrittweise Weiterentwicklung des Projekts vom einfachen Live-Monitor zum intelligenten, historischen Pflanzen-Hub mit Cloud-Benachrichtigungen. Das System ist für den dauerhaften USB-Betrieb des Displays ausgelegt, während die Batterielaufzeit der Sensoren durch intelligentes Polling maximiert wird.

## Phase 1: Multi-Sensor & Dynamisches "Gotchi" UI 👻
In dieser Phase lernt das Display, mit mehreren Sensoren umzugehen und den Zustand der Pflanzen visuell und emotional darzustellen.
* **Batterieschonendes Polling (Sensor-Caching):** Reduzierung der aktiven Bluetooth-Verbindungen. Der ESP32 fragt die Sensoren nur in großen Intervallen (z. B. alle 60–120 Minuten) ab, puffert die Werte für das Display und trennt die Verbindung sofort wieder. Dies maximiert die Lebensdauer der Sensor-Knopfzellen.
* **Multi-BLE-Verbindung:** Erweiterung des Codes zum asynchronen Scannen und Auslesen von drei spezifischen MAC-Adressen im definierten Abfrage-Intervall.
* **Pflanzen-Profile (Min/Max-Schwellenwerte):** 
  1. **Monstera** (Tropenpflanze): mäßiges Wasser, viel indirektes Licht.
  2. **Elefantenfuß** (Sukkulenten-Art): sehr wenig Wasser, volle Sonne/hohes Licht.
  3. **Clusia** (robuste Tropenpflanze): mäßiges Wasser, viel Licht.
* **Visuals & Emotionen (Gotchi-Logik):** 
  * **Glücklicher Geist:** Alle vier Werte (Wasser, Dünger, Temp, Licht) liegen im grünen Bereich des jeweiligen Profils.
  * **Totenkopf-Symbol (💀):** Ein oder mehrere Werte verletzen die Toleranzgrenze stark (z. B. kritische Trockenheit). Markierung des spezifischen Fehlerwerts in Rot auf dem Display.
* **Erweiterte Navigation (2-Tasten-Logik):**
  * *Taste 1 (Pin 0) - Kurz:* Manuell zur Ansicht der nächsten Pflanze wechseln (zeigt gepufferte Werte).
  * *Taste 1 (Pin 0) - Lang:* Umschalten zwischen **Manuellem Modus** und **Auto-Cycle-Modus** (Display wechselt z. B. alle 10 Sekunden automatisch die Pflanze).

## Phase 2: Historie & Graphen (LittleFS) 📈
In dieser Phase erhält das System ein Gedächtnis, um Trends (z.B. Gieß-Zyklen) sichtbar zu machen.
* **Datenspeicherung (LittleFS):** Aktivierung des internen Flash-Speichers des ESP32-S3 für persistente Daten.
* **Daten-Logging & Ringpuffer:** Abspeichern der Messwerte aller drei Pflanzen nach jedem erfolgreichen Polling-Intervall in einer CSV-Datei. Bei Erreichen von z. B. 7 Tagen Historie greift das FIFO-Prinzip (First-In-First-Out), um Speicherüberläufe zu verhindern.
* **Graphen-Ansicht (UI):** 
  * Aufrufbar durch *Taste 2 (Pin 14) - Kurz*.
  * Nutzung von `TFT_eSPI` zum Rendern eines Liniendiagramms.
  * Darstellung des Bodenfeuchtigkeits-Verlaufs der letzten 24h–72h für die aktuell ausgewählte Pflanze.
  * *Taste 2 (Pin 14) - Lang:* Manueller Trigger, um alle drei Sensoren sofort live abzufragen (überspringt das Warte-Intervall).

## Phase 3: Smart Alerts (WLAN & Telegram Bot) 📱
Der ESP32 wird ins Netzwerk integriert, ruft die korrekte Uhrzeit ab und agiert als aktiver Pflanzen-Wächter.
* **WLAN & NTP-Zeitsynchronisation:** Setup der WLAN-Verbindung und Abruf der aktuellen Weltzeit über einen NTP-Server, damit der ESP32 einen exakten Tagesrhythmus hat.
* **Telegram Bot API:** Einrichtung eines kostenlosen Telegram-Bots via BotFather für die direkte Smartphone-Kommunikation.
* **Daily Status Report:** Der ESP32 sendet jeden Tag zu einer festgelegten Uhrzeit (z. B. 18:00 Uhr) eine strukturierte Zusammenfassung über den Zustand aller drei Pflanzen.
* **Ad-Hoc Warnungen:** Eigenständige Push-Nachricht bei akuter Gefahr, falls ein kritischer Schwellenwert nach einer Messung verletzt wird (z. B. "💀 Alarm: Der Elefantenfuß steht komplett trocken!").

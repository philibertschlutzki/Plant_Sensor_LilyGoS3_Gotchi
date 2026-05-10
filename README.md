# 🪴 Plant Sensor LilyGo S3 Gotchi

Willkommen beim **Gotchi-Projekt**! Dieses Projekt verwandelt ein **LILYGO T-Display-S3** in einen praktischen kleinen Helfer, der dir auf einem Display anzeigt, wie es deinen Pflanzen geht. Es verbindet sich dazu über Bluetooth mit **Xiaomi Mi Flora** Sensoren.

Alles, was du tun musst, ist dein Gotchi mit deinem WLAN zu verbinden und die Einstellungen über eine einfache Webseite (Web-GUI) vorzunehmen – ganz ohne kompliziertes Programmieren!

---

## 🌟 Projektbeschreibung: Was kann das Gotchi?

* **Live-Daten:** Liest drahtlos Temperatur, Bodenfeuchtigkeit, Licht und Dünger (Leitfähigkeit) von bis zu 3 Mi Flora Sensoren aus.
* **Historie:** Speichert die Bodenfeuchtigkeit über die Zeit ab und zeigt dir auf einer Webseite ein anschauliches Diagramm (Chart).
* **Push-Warnungen:** Schickt dir eine Nachricht aufs Handy (via ntfy.sh), wenn eine Pflanze zu trocken ist und Wasser braucht.
* **Einfache Konfiguration:** Alle Einstellungen (Sensoren, Pflanzen-Namen, Min/Max-Werte) können bequem über den Webbrowser am PC oder Smartphone geändert werden.

---

## 🛠️ Vorbereitung: MAC-Adresse des Sensors finden

Um dem Gotchi zu sagen, mit welchem Sensor es sprechen soll, brauchst du die sogenannte "MAC-Adresse" des Sensors. Das ist wie eine Telefonnummer für Bluetooth-Geräte.

**So findest du sie heraus:**
1. Lade dir die kostenlose App **"nRF Connect for Mobile"** auf dein Smartphone herunter (verfügbar für [iOS](https://apps.apple.com/app/nrf-connect-for-mobile/id1054362403) und [Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp)).
2. Gehe ganz nah an den Sensor heran, der in der Erde steckt.
3. Öffne die App und drücke oben auf **"Scan"**. *(Android: Du musst der App den Zugriff auf den Standort erlauben).*
4. In der Liste sollte nun ein Gerät namens **"Flower care"** oder **"Flower mate"** auftauchen.
5. Darunter steht eine Nummer wie `C4:7C:8D:12:34:56`. Das ist die gesuchte MAC-Adresse. **Schreibe sie dir auf!**

---

## 💻 Installation (Upload auf das Display)

Damit das Gotchi funktioniert, müssen zwei Dinge auf das LILYGO T-Display-S3 übertragen (geflasht) werden:
1. Das **Programm** selbst (Firmware).
2. Das **Dateisystem** (LittleFS), auf dem die Webseite und später deine Messdaten gespeichert werden.

**Schritt-für-Schritt mit PlatformIO:**
1. Installiere [Visual Studio Code](https://code.visualstudio.com/) und füge die Erweiterung **PlatformIO IDE** hinzu.
2. Lade dieses Projekt herunter und öffne den Projektordner in Visual Studio Code.
3. Schließe dein LILYGO T-Display-S3 per USB an den PC an.
4. **Firmware hochladen:** Klicke unten in der blauen PlatformIO-Leiste auf den kleinen **Rechtspfeil (Upload)** `➔`. Warte, bis der Vorgang mit "SUCCESS" beendet ist.
5. **Dateisystem hochladen (Sehr wichtig!):** Klicke links im Menü auf das Alien-Kopf-Symbol (PlatformIO). Gehe unter `Project Tasks` -> `lilygo-t-display-s3` -> `Platform` und klicke auf **"Build Filesystem Image"**. Wenn das erfolgreich war, klicke direkt darunter auf **"Upload Filesystem Image"**.

Wenn beides erfolgreich war, drücke kurz den Reset-Knopf an der Seite deines LILYGO Displays.

---

## 🌐 Erster Start (WLAN & ntfy.sh einrichten)

Beim allerersten Start kennt dein Gotchi dein WLAN noch nicht. Es öffnet daher ein eigenes kleines WLAN-Netzwerk, um sich mit dir zu verbinden.

1. Schau auf das Display des Gotchis. Es sollte "WLAN: Gotchi-Setup..." anzeigen.
2. Nimm dein Smartphone oder deinen PC und suche nach neuen WLAN-Netzwerken.
3. Verbinde dich mit dem Netzwerk **"Gotchi-Setup"**.
4. Es öffnet sich automatisch eine Anmeldeseite (Captive Portal). Falls nicht, öffne deinen Browser und tippe `192.168.4.1` ein.
5. Klicke auf **"Configure WiFi"**.
6. Wähle dein Heim-WLAN aus und gib dein WLAN-Passwort ein.
7. Ganz unten findest du das Feld **"ntfy.sh Topic"**. Überlege dir hier ein sicheres, geheimes Wort (z. B. `mein_geheimes_gotchi_123`). Lade dir die [ntfy App](https://ntfy.sh/) auf dein Handy und abonniere exakt dieses Wort, um später Push-Nachrichten zu erhalten, wenn du gießen musst.
8. Klicke auf **Save**. Das Gotchi startet neu und verbindet sich nun mit deinem Heim-WLAN. Auf dem Display siehst du die Erfolgsmeldung!

---

## ⚙️ Konfiguration über das Web-GUI

Dein Gotchi ist nun mit dem WLAN verbunden, weiß aber noch nicht, welche Pflanzen es überwachen soll. Das ändern wir jetzt auf der Webseite des Gotchis.

1. Finde heraus, welche IP-Adresse dein Gotchi in deinem WLAN bekommen hat (z. B. durch einen Blick in die Weboberfläche deines Routers, wie der FritzBox).
2. Öffne einen Browser (am PC oder Handy) und tippe diese IP-Adresse ein (z. B. `http://192.168.178.50`).
3. Du siehst nun das **Gotchi Dashboard**. Scrolle nach unten zum Bereich **"Einstellungen / Administration"**.
4. Hier findest du 3 "Slots" für bis zu 3 verschiedene Pflanzen:
   * Setze einen Haken bei **"Aktiviert"**, wenn du diesen Slot nutzen möchtest.
   * Wähle aus dem Dropdown-Menü eine **Vorlage** (z. B. "Monstera"). Die Felder für Wasser, Dünger etc. füllen sich automatisch mit passenden Werten!
   * Trage bei **"MAC Adresse"** die Adresse ein, die du vorhin in der App gefunden hast.
5. Klicke am Ende auf den blauen Button **"💾 Konfiguration speichern"**.

Dein Gotchi übernimmt die Einstellungen sofort und beginnt, die Daten deiner Pflanzen auszulesen und auf dem Display anzuzeigen! 🌱 Viel Spaß!

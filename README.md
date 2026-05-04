# 🪴 Plant Sensor LilyGo S3 Gotchi

Dieses Projekt verwandelt ein **LILYGO T-Display-S3** in einen kleinen Monitor ("Gotchi"), der sich über Bluetooth mit einem **Xiaomi Mi Flora** Pflanzensensor verbindet und die aktuellen Messwerte der Pflanze direkt auf dem Display anzeigt.

Das Besondere: Dank GitHub Actions wird der Code automatisch in der Cloud kompiliert. Du benötigst keine lokale Entwicklungsumgebung wie Arduino IDE oder PlatformIO, um das Gerät zu flashen!

---

## 📦 Hardware & Sensoren

### 1. LILYGO T-Display-S3
Das Herzstück des Projekts. Es handelt sich um ein kompaktes Entwicklungsboard mit folgenden Spezifikationen:
* **Mikrocontroller:** ESP32-S3R8 Dual-Core (unterstützt 2.4 GHz WLAN und Bluetooth 5 / BLE)
* **Display:** 1.9 Zoll ST7789 IPS LCD (Auflösung: 170x320 Pixel)
* **Anschluss:** USB-C (für Strom und Daten)

### 2. Xiaomi Mi Flora (HHCC Flower Care)
Ein batteriebetriebener Sensor, der direkt in die Blumenerde gesteckt wird. Er misst vier entscheidende Parameter:
* Bodenfeuchtigkeit (%)
* Nährstoffgehalt / Leitfähigkeit (µS/cm)
* Temperatur (°C)
* Lichtintensität (Lux)

---

## 🚀 Installation & Nutzung

Um das Projekt zu nutzen, musst du dem Display mitteilen, welchen Sensor es auslesen soll. Folge diesen 3 Schritten:

### Schritt 1: MAC-Adresse des Sensors finden (Smartphone Methode)
Der einfachste Weg, die Bluetooth-Adresse deines Sensors herauszufinden, ist über eine kostenlose Smartphone-App.

1. Lade dir die kostenlose App **"nRF Connect for Mobile"** herunter (verfügbar für [iOS](https://apps.apple.com/app/nrf-connect-for-mobile/id1054362403) und [Android](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp)).
2. Gehe mit deinem Smartphone direkt an die Pflanze mit dem Sensor heran.
3. Öffne die App und tippe oben auf **"Scan"**. *(Android-Nutzer: Die App benötigt hierfür die Standortberechtigung)*.
4. Suche in der durchlaufenden Liste nach einem Gerät mit dem Namen **"Flower care"** oder **"Flower mate"**.
5. Direkt unter oder neben diesem Namen siehst du eine Zeichenfolge, die ungefähr so aussieht: `C4:7C:8D:XX:XX:XX`. Das ist die gesuchte MAC-Adresse. **Notiere sie dir!**

### Schritt 2: Code anpassen & Firmware herunterladen
Jetzt tragen wir diese Adresse in den Code ein und lassen die Server von GitHub die Arbeit machen.

1. Öffne auf der Startseite dieses GitHub-Repositories den Ordner `src` und klicke auf die Datei `main.cpp`.
2. Klicke auf das Stift-Symbol (Bearbeiten) und ändere in **Zeile 7** die Adresse auf deine eigene:
   ```cpp
   static NimBLEAddress floraAddress("C4:7C:8D:12:34:56"); // Trage hier deine notierte Adresse ein
   ```
3. Klicke oben rechts auf **Commit changes**, um die Datei zu speichern.
4. **Automatische Kompilierung:** Sobald du speicherst, startet im Hintergrund die Kompilierung. Das dauert etwa 1 bis 2 Minuten.
5. **Firmware laden:** Gehe zurück auf die Startseite dieses Repositories. Auf der **rechten Seite** findest du den Bereich **"Releases"**. Klicke dort auf die neueste Version (z. B. *Firmware Build #1*) und lade dir die angehängte Datei `LilyGo_Gotchi_Firmware.zip` herunter. Entpacke die ZIP-Datei auf deinem PC.

### Schritt 3: Firmware auf das Display flashen
Du musst keine komplizierte Software auf deinem PC installieren. Wir nutzen einen Web-Browser (Google Chrome oder Microsoft Edge empfohlen):

1. Schließe dein LILYGO T-Display-S3 per USB an den PC an.
2. Öffne das [Adafruit WebSerial ESPTool](https://adafruit.github.io/Adafruit_WebSerial_ESPTool/).
3. Klicke oben rechts auf **Connect** und wähle den USB-Port deines Displays aus.
4. Scrolle nach unten zum Bereich "Firmware". Lade hier die drei entpackten Dateien an die exakt passenden Speicheradressen (Offsets) hoch:
   * Offset `0x0000` ➔ Wähle die Datei `bootloader.bin`
   * Offset `0x8000` ➔ Wähle die Datei `partitions.bin`
   * Offset `0x10000` ➔ Wähle die Datei `firmware.bin`
5. Klicke auf **Program**. 

Sobald der Vorgang abgeschlossen ist, drücke den Reset-Knopf an der Seite des LILYGO Displays. Dein Gotchi startet nun, verbindet sich mit dem Sensor und zeigt dir die Daten an! 🌱

---

## 🛠️ Gebaut mit
* [PlatformIO](https://platformio.org/)
* [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) von Bodmer
* [NimBLE-Arduino](https://github.com/h2zero/NimBLE-Arduino)
```

# 🪴 Plant Sensor LilyGo S3 Gotchi

Dieses Projekt verwandelt ein **LILYGO T-Display-S3** in einen kleinen Monitor ("Gotchi"), der sich über Bluetooth mit einem **Xiaomi Mi Flora** Pflanzensensor verbindet und die aktuellen Messwerte der Pflanze direkt auf dem Display anzeigt.

Das Besondere: Dank GitHub Actions wird der Code automatisch in der Cloud kompiliert. Du benötigst keine lokale Entwicklungsumgebung wie Arduino IDE oder PlatformIO, um das Gerät zu flashen!

## 📦 Hardware-Voraussetzungen
* 1x LILYGO T-Display-S3 (mit ESP32-S3)
* 1x Xiaomi Mi Flora (HHCC Flower Care) Sensor
* USB-C Kabel

## 🚀 Installation & Nutzung

### Schritt 1: MAC-Adresse anpassen
Bevor du startest, muss der Code wissen, mit welchem Sensor er sich verbinden soll.
1. Finde die MAC-Adresse deines Mi Flora Sensors heraus (z.B. über eine BLE-Scanner App auf dem Smartphone).
2. Öffne in diesem Repository die Datei `src/main.cpp`.
3. Ändere in Zeile 7 die Adresse auf deine eigene:
   ```cpp
   static NimBLEAddress floraAddress("DEINE:MAC:ADRESSE:HIER");
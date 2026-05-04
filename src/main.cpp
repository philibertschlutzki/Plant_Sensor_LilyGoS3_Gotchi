#include <Arduino.h>
#include <TFT_eSPI.h>
#include <NimBLEDevice.h>

// --- DEINE KONFIGURATION ---
// Trage hier die MAC-Adresse deines Mi Flora Sensors ein
static NimBLEAddress floraAddress("5C:85:7E:B0:00:57");

TFT_eSPI tft = TFT_eSPI();

// Mi Flora spezifische Bluetooth UUIDs
static NimBLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_version_batt("00001a02-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_write_mode("00001a00-0000-1000-8000-00805f9b34fb");

// Struktur für die Sensordaten
struct SensorData {
    int battery = 0;
    float temperature = 0.0;
    int moisture = 0;
    int light = 0;
    int conductivity = 0;
};

SensorData sensorData;

bool readFloraData() {
    NimBLEClient* pClient = NimBLEDevice::createClient();
    
    pClient->setConnectTimeout(5);

    // Versuche zu verbinden
    if (!pClient->connect(floraAddress)) {
        Serial.println("Fehler: Konnte nicht mit Mi Flora verbinden.");
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    NimBLERemoteService* pService = pClient->getService(serviceUUID);
    if (pService == nullptr) {
        Serial.println("Fehler: Service UUID nicht gefunden.");
        pClient->disconnect();
        return false;
    }

    // 1. Batteriestatus lesen
    NimBLERemoteCharacteristic* pBattChar = pService->getCharacteristic(uuid_version_batt);
    if (pBattChar != nullptr && pBattChar->canRead()) {
        std::string value = pBattChar->readValue();
        if (value.length() >= 1) {
            sensorData.battery = (uint8_t)value[0];
        }
    } else {
        Serial.println("Warnung: Konnte Batterie Characteristic nicht lesen.");
    }

    // 2. Sensormodus aufwecken (Magic Bytes senden)
    NimBLERemoteCharacteristic* pModeChar = pService->getCharacteristic(uuid_write_mode);
    if (pModeChar != nullptr && pModeChar->canWrite()) {
        uint8_t magicBytes[2] = {0xA0, 0x1F};
        pModeChar->writeValue(magicBytes, 2, true);
    } else {
        Serial.println("Warnung: Konnte Magic Bytes nicht senden.");
    }

    // 3. Sensordaten lesen
    NimBLERemoteCharacteristic* pDataChar = pService->getCharacteristic(uuid_sensor_data);
    if (pDataChar != nullptr && pDataChar->canRead()) {
        std::string value = pDataChar->readValue();
        if (value.length() >= 16) {
            sensorData.temperature = ((uint8_t)value[0] + (uint8_t)value[1] * 256) / 10.0;
            sensorData.light = (uint8_t)value[3] + (uint8_t)value[4] * 256 + (uint8_t)value[5] * 65536 + (uint8_t)value[6] * 16777216;
            sensorData.moisture = (uint8_t)value[7];
            sensorData.conductivity = (uint8_t)value[8] + (uint8_t)value[9] * 256;
        } else {
            Serial.println("Warnung: Daten von Sensor sind zu kurz.");
        }
    } else {
        Serial.println("Fehler: Konnte Sensor Daten Characteristic nicht lesen.");
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    // Verbindung sauber trennen
    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    return true;
}

void updateDisplay(bool success) {
    tft.fillScreen(TFT_BLACK);
    
    if (success) {
        // Erfolgreich gelesen - Daten anzeigen
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Pflanzen Status", 10, 10, 4);
        
        char buffer[32];
        tft.setTextColor(TFT_WHITE, TFT_BLACK);

        snprintf(buffer, sizeof(buffer), "Temp:   %.1f C", sensorData.temperature);
        tft.drawString(buffer, 10, 50, 4);

        snprintf(buffer, sizeof(buffer), "Wasser: %d %%", sensorData.moisture);
        tft.drawString(buffer, 10, 80, 4);

        snprintf(buffer, sizeof(buffer), "Licht:  %d lx", sensorData.light);
        tft.drawString(buffer, 10, 110, 4);

        snprintf(buffer, sizeof(buffer), "Duenger:%d uS", sensorData.conductivity);
        tft.drawString(buffer, 10, 140, 4);
        
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        snprintf(buffer, sizeof(buffer), "Akku: %d %%", sensorData.battery);
        tft.drawString(buffer, 10, 180, 2);
    } else {
        // Fehler bei der Verbindung
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("Verbindung", 10, 60, 4);
        tft.drawString("fehlgeschlagen!", 10, 90, 4);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Neuer Versuch...", 10, 130, 2);
    }
}

void setup() {
    Serial.begin(115200);
    
    // T-Display S3: Stromversorgung für das Display aktivieren (Pin 15)
    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    
    // Backlight aktivieren (Pin 38)
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH);
    
    // Display initialisieren
    tft.init();
    tft.setRotation(1); // Querformat
    tft.fillScreen(TFT_BLACK);
    
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Gotchi startet...", 10, 80, 4);

    // Bluetooth initialisieren
    NimBLEDevice::init("LilyGo-Gotchi");

    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Suche Sensor...", 10, 80, 4);
    
    // Daten lesen
    bool success = readFloraData();
    
    // Display updaten
    updateDisplay(success);
    
    // 10 Sekunden warten, damit der Nutzer die Daten sehen kann
    delay(10000);

    // Display ausschalten
    digitalWrite(38, LOW);
    digitalWrite(15, LOW);

    // Deep Sleep für 10 Minuten (600.000.000 Mikrosekunden)
    esp_sleep_enable_timer_wakeup(600ULL * 1000000ULL);
    esp_deep_sleep_start();
}

void loop() {
    // Wird im Deep Sleep nicht erreicht
}

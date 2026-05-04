#include <Arduino.h>
#include <TFT_eSPI.h>
#include <NimBLEDevice.h>

// --- DEINE KONFIGURATION ---
// Trage hier die MAC-Adresse deines Mi Flora Sensors ein
static NimBLEAddress floraAddress("C4:7C:8D:XX:XX:XX"); 

TFT_eSPI tft = TFT_eSPI();

// Mi Flora spezifische Bluetooth UUIDs
static NimBLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_version_batt("00001a02-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_write_mode("00001a00-0000-1000-8000-00805f9b34fb");

// Variablen für die Sensordaten
int battery = 0;
float temperature = 0.0;
int moisture = 0;
int light = 0;
int conductivity = 0;

bool readFloraData() {
    NimBLEClient* pClient = NimBLEDevice::createClient();
    
    // Versuche zu verbinden
    if (!pClient->connect(floraAddress)) {
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    NimBLERemoteService* pService = pClient->getService(serviceUUID);
    if (pService == nullptr) {
        pClient->disconnect();
        return false;
    }

    // 1. Batteriestatus lesen
    NimBLERemoteCharacteristic* pBattChar = pService->getCharacteristic(uuid_version_batt);
    if (pBattChar != nullptr && pBattChar->canRead()) {
        std::string value = pBattChar->readValue();
        if (value.length() >= 1) {
            battery = value[0];
        }
    }

    // 2. Sensormodus aufwecken (Magic Bytes senden)
    NimBLERemoteCharacteristic* pModeChar = pService->getCharacteristic(uuid_write_mode);
    if (pModeChar != nullptr && pModeChar->canWrite()) {
        uint8_t magicBytes[2] = {0xA0, 0x1F};
        pModeChar->writeValue(magicBytes, 2, true);
    }

    // 3. Sensordaten lesen
    NimBLERemoteCharacteristic* pDataChar = pService->getCharacteristic(uuid_sensor_data);
    if (pDataChar != nullptr && pDataChar->canRead()) {
        std::string value = pDataChar->readValue();
        if (value.length() >= 16) {
            temperature = (value[0] + value[1] * 256) / 10.0;
            light = value[3] + value[4] * 256 + value[5] * 65536 + value[6] * 16777216;
            moisture = value[7];
            conductivity = value[8] + value[9] * 256;
        }
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
        
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Temp:   " + String(temperature, 1) + " C", 10, 50, 4);
        tft.drawString("Wasser: " + String(moisture) + " %", 10, 80, 4);
        tft.drawString("Licht:  " + String(light) + " lx", 10, 110, 4);
        tft.drawString("Duenger:" + String(conductivity) + " uS", 10, 140, 4);
        
        tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
        tft.drawString("Akku: " + String(battery) + " %", 10, 180, 2);
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
}

void loop() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Suche Sensor...", 10, 80, 4);
    
    // Daten lesen
    bool success = readFloraData();
    
    // Display updaten
    updateDisplay(success);
    
    // 30 Sekunden warten bis zur nächsten Aktualisierung. 
    // Später kannst du das auf z.B. 10 Minuten (600000 ms) erhöhen, um Batterie zu sparen.
    delay(30000); 
}
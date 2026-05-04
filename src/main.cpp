#include <Arduino.h>
#include <TFT_eSPI.h>
#include <NimBLEDevice.h>
#include "xbms.h"

TFT_eSPI tft = TFT_eSPI();

// Mi Flora specific Bluetooth UUIDs
static NimBLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_version_batt("00001a02-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_write_mode("00001a00-0000-1000-8000-00805f9b34fb");

struct PlantProfile {
    String name;
    String mac;
    float minTemp;
    float maxTemp;
    int minMoisture;
    int maxMoisture;
    int minLight;
    int maxLight;
    int minConductivity;
    int maxConductivity;
};

// 1. Monstera: Temp: 18.0-28.0, Feuchtigkeit: 20-60, Licht: 1000-4000, Duenger: 350-1000
// 2. Elefantenfuss: Temp: 15.0-30.0, Feuchtigkeit: 5-20, Licht: 2000-8000, Duenger: 150-500
// 3. Clusia: Temp: 18.0-28.0, Feuchtigkeit: 20-50, Licht: 1500-5000, Duenger: 300-800
PlantProfile profiles[3] = {
    {"Monstera", "XX:XX:XX:XX:XX:01", 18.0, 28.0, 20, 60, 1000, 4000, 350, 1000},
    {"Elefantenfuss", "XX:XX:XX:XX:XX:02", 15.0, 30.0, 5, 20, 2000, 8000, 150, 500},
    {"Clusia", "XX:XX:XX:XX:XX:03", 18.0, 28.0, 20, 50, 1500, 5000, 300, 800}
};

struct PlantData {
    int battery;
    float temperature;
    int moisture;
    int light;
    int conductivity;
    unsigned long lastUpdate;
};

PlantData cachedData[3] = {
    {0, 0.0, 0, 0, 0, 0},
    {0, 0.0, 0, 0, 0, 0},
    {0, 0.0, 0, 0, 0, 0}
};

// State variables
int currentPlantIndex = 0;
bool autoCycleMode = false;
unsigned long lastBlePoll = 0;
unsigned long lastAutoCycle = 0;
const unsigned long BLE_POLL_INTERVAL = 3600000UL; // 60 minutes
const unsigned long AUTO_CYCLE_INTERVAL = 10000UL; // 10 seconds

// Button state
const int BUTTON_PIN = 0;
int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long buttonPressTime = 0;
unsigned long debounceDelay = 50;

void drawUI() {
    tft.fillScreen(TFT_BLACK);

    PlantProfile p = profiles[currentPlantIndex];
    PlantData d = cachedData[currentPlantIndex];

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString(p.name, 160, 10, 4);

    char buffer[64];

    // Check limits
    bool tempOk = (d.temperature >= p.minTemp && d.temperature <= p.maxTemp);
    bool moistOk = (d.moisture >= p.minMoisture && d.moisture <= p.maxMoisture);
    bool lightOk = (d.light >= p.minLight && d.light <= p.maxLight);
    bool condOk = (d.conductivity >= p.minConductivity && d.conductivity <= p.maxConductivity);

    bool allOk = (tempOk && moistOk && lightOk && condOk);
    
    int yOffset = 50;

    // Temp
    snprintf(buffer, sizeof(buffer), "Temp: %.1f C", d.temperature);
    tft.setTextColor(tempOk ? TFT_WHITE : TFT_RED, TFT_BLACK);
    tft.drawString(buffer, 10, yOffset, 4);
    yOffset += 30;

    // Moisture
    snprintf(buffer, sizeof(buffer), "Wasser: %d %%", d.moisture);
    tft.setTextColor(moistOk ? TFT_WHITE : TFT_RED, TFT_BLACK);
    tft.drawString(buffer, 10, yOffset, 4);
    yOffset += 30;

    // Light
    snprintf(buffer, sizeof(buffer), "Licht: %d lx", d.light);
    tft.setTextColor(lightOk ? TFT_WHITE : TFT_RED, TFT_BLACK);
    tft.drawString(buffer, 10, yOffset, 4);
    yOffset += 30;

    // Conductivity
    snprintf(buffer, sizeof(buffer), "Duenger: %d uS", d.conductivity);
    tft.setTextColor(condOk ? TFT_WHITE : TFT_RED, TFT_BLACK);
    tft.drawString(buffer, 10, yOffset, 4);

    // Battery and Update Time
    tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
    snprintf(buffer, sizeof(buffer), "Akku: %d %%", d.battery);
    tft.drawString(buffer, 10, 180, 2);

    unsigned long minutesAgo = (millis() - d.lastUpdate) / 60000;
    snprintf(buffer, sizeof(buffer), "Upd: vor %lu m", minutesAgo);
    tft.drawString(buffer, 10, 200, 2);

    // Auto mode indicator
    if (autoCycleMode) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("AUTO", 260, 200, 2);
    }

    // Draw emotion bitmap (32x32)
    // Draw at right side
    if (allOk) {
        tft.drawXBitmap(260, 80, ghost_bits, 32, 32, TFT_GREEN);
    } else {
        tft.drawXBitmap(260, 80, skull_bits, 32, 32, TFT_RED);
    }
}

bool readFloraData(int index) {
    NimBLEAddress addr(profiles[index].mac.c_str());
    NimBLEClient* pClient = NimBLEDevice::createClient();
    pClient->setConnectTimeout(5);

    if (!pClient->connect(addr)) {
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    NimBLERemoteService* pService = pClient->getService(serviceUUID);
    if (pService == nullptr) {
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        return false;
    }

    // 1. Batterie
    NimBLERemoteCharacteristic* pBattChar = pService->getCharacteristic(uuid_version_batt);
    if (pBattChar != nullptr && pBattChar->canRead()) {
        std::string value = pBattChar->readValue();
        if (value.length() >= 1) {
            cachedData[index].battery = (uint8_t)value[0];
        }
    }

    // 2. Magic Bytes
    NimBLERemoteCharacteristic* pModeChar = pService->getCharacteristic(uuid_write_mode);
    if (pModeChar != nullptr && pModeChar->canWrite()) {
        uint8_t magicBytes[2] = {0xA0, 0x1F};
        pModeChar->writeValue(magicBytes, 2, true);
    }

    // 3. Sensor Daten
    NimBLERemoteCharacteristic* pDataChar = pService->getCharacteristic(uuid_sensor_data);
    if (pDataChar != nullptr && pDataChar->canRead()) {
        std::string value = pDataChar->readValue();
        if (value.length() >= 16) {
            cachedData[index].temperature = ((uint8_t)value[0] + (uint8_t)value[1] * 256) / 10.0;
            cachedData[index].light = (uint8_t)value[3] + (uint8_t)value[4] * 256 + (uint8_t)value[5] * 65536 + (uint8_t)value[6] * 16777216;
            cachedData[index].moisture = (uint8_t)value[7];
            cachedData[index].conductivity = (uint8_t)value[8] + (uint8_t)value[9] * 256;
            cachedData[index].lastUpdate = millis();
        }
    }

    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    return true;
}

void pollBLE() {
    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Lese Sensoren...", 10, 220, 2);
    
    for (int i = 0; i < 3; i++) {
        readFloraData(i);
    }

    lastBlePoll = millis();
    drawUI(); // Refresh UI after polling
}

void setup() {
    Serial.begin(115200);
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    
    pinMode(38, OUTPUT);
    digitalWrite(38, HIGH);
    
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Gotchi startet...", 10, 80, 4);

    NimBLEDevice::init("LilyGo-Gotchi");
    
    // Initial poll
    pollBLE();
    
    // If the initial poll failed (lastUpdate remains 0), we still draw UI
    drawUI();
}

void loop() {
    unsigned long currentMillis = millis();

    // BLE Polling (non-blocking in loop, but polling itself blocks for a few secs)
    // Avoid checking right after boot as lastBlePoll is set in setup()
    if (currentMillis - lastBlePoll >= BLE_POLL_INTERVAL) {
        pollBLE();
    }

    // Auto Cycle Logic
    if (autoCycleMode) {
        if (currentMillis - lastAutoCycle >= AUTO_CYCLE_INTERVAL) {
            currentPlantIndex = (currentPlantIndex + 1) % 3;
            drawUI();
            lastAutoCycle = currentMillis;
        }
    }

    // Button Logic
    int reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) {
        lastDebounceTime = currentMillis;
    }

    if ((currentMillis - lastDebounceTime) > debounceDelay) {
        if (reading != buttonState) {
            buttonState = reading;

            if (buttonState == LOW) {
                // Button pressed
                buttonPressTime = currentMillis;
            } else {
                // Button released
                unsigned long pressDuration = currentMillis - buttonPressTime;

                if (pressDuration < 500) {
                    // Short press
                    autoCycleMode = false;
                    currentPlantIndex = (currentPlantIndex + 1) % 3;
                    drawUI();
                } else {
                    // Long press
                    autoCycleMode = !autoCycleMode;
                    if (autoCycleMode) {
                        lastAutoCycle = currentMillis;
                    }
                    drawUI(); // Redraw to update AUTO text
                }
            }
        }
    }
    lastButtonState = reading;
}

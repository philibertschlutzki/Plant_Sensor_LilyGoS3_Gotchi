#include <Arduino.h>
#include <TFT_eSPI.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <ESPmDNS.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include "animations.h"
#include <esp_task_wdt.h>
#include <esp_wifi.h>
#include <ElegantOTA.h>

Preferences preferences;
SemaphoreHandle_t dataMutex;
AsyncWebServer server(80);
String ntfyTopic = "ntfy.sh/mein_gotchi_geheim_123"; // default fallback

volatile bool isBLEScanning = false;
volatile bool requestTestPush = false;
volatile bool requestUIDraw = false;
volatile bool isExporting = false;
volatile bool requestLogData = false;
bool pushSent_Moisture[3] = {false, false, false};
bool pushSent_Temp[3] = {false, false, false};
bool isOffline[3] = {false, false, false};
bool pushSent_Offline[3] = {false, false, false};

void drawUI();
void drawTamagotchiUI();
void drawTechnicalUI();
void logData();
void checkAndSendNotifications(bool testPush = false);
void pollBLE();
void loadConfig();

TFT_eSPI tft = TFT_eSPI();


// Mi Flora specific Bluetooth UUIDs
static NimBLEUUID serviceUUID("00001204-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_version_batt("00001a02-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_sensor_data("00001a01-0000-1000-8000-00805f9b34fb");
static NimBLEUUID uuid_write_mode("00001a00-0000-1000-8000-00805f9b34fb");

struct PlantProfile {
    bool active;
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
    int moistureOffset;
};

PlantProfile profiles[3];

// Global Settings
String nightModeStart = "22:00";
String nightModeEnd = "07:00";
int extBatPin = 10;
float extBatDivider = 2.0;

struct PlantData {
    int battery;
    float temperature;
    int moisture;
    int light;
    int conductivity;
    unsigned long lastUpdate;
};

struct LogEntry {
    uint32_t timestamp;
    uint8_t plantIdx;
    float temp;
    uint8_t moist;
    uint16_t light;
    uint16_t cond;
} __attribute__((packed));

const int MAX_LOG_ENTRIES = (50000 - sizeof(uint32_t)) / sizeof(LogEntry); // Max entries to keep file around ~50KB

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

// Dimming State
unsigned long lastInteractionTime = 0;

// Button state
const int BUTTON_PIN = 0;
const int BUTTON_PIN_MODE = 14;

int buttonState = HIGH;
int lastButtonState = HIGH;
unsigned long lastDebounceTime = 0;
unsigned long buttonPressTime = 0;

int buttonModeState = HIGH;
int lastButtonModeState = HIGH;
unsigned long lastDebounceModeTime = 0;
unsigned long buttonModePressTime = 0;

unsigned long debounceDelay = 50;

int uiMode = 0; // 0 = Tamagotchi, 1 = Technical, 2 = Battery
unsigned long lastAnimUpdate = 0;
int currentFrame = 0;

void drawBatteryUI();

void drawUI() {
    if (uiMode == 0) {
        drawTamagotchiUI();
    } else if (uiMode == 1) {
        drawTechnicalUI();
    } else {
        drawBatteryUI();
    }
}

void drawTamagotchiUI() {
    PlantProfile p = profiles[currentPlantIndex];
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    PlantData d = cachedData[currentPlantIndex];
    xSemaphoreGive(dataMutex);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString(p.name, 160, 10, 4);

    // Check limits
    bool tempLow = (d.temperature < p.minTemp);
    bool tempHigh = (d.temperature > p.maxTemp);
    bool tempOk = !tempLow && !tempHigh;

    bool moistLow = (d.moisture < p.minMoisture);
    bool moistHigh = (d.moisture > p.maxMoisture);
    bool moistOk = !moistLow && !moistHigh;

    bool lightOk = (d.light >= p.minLight && d.light <= p.maxLight);
    bool condOk = (d.conductivity >= p.minConductivity && d.conductivity <= p.maxConductivity);

    bool allOk = (tempOk && moistOk && lightOk && condOk);

    // Draw Avatar (Ghost)
    int ghostX = (320 - GHOST_WIDTH) / 2;
    int ghostY = 40;

    const uint16_t* frameToDraw = ghost_idle_frame1;

    // Select animation based on status
    if (isOffline[currentPlantIndex]) {
        tft.fillRect(ghostX, ghostY, GHOST_WIDTH, GHOST_HEIGHT, TFT_BLACK);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawCentreString("Sensor Offline", 160, ghostY + 30, 4);
    } else {
        if (allOk) {
            frameToDraw = (currentFrame == 0) ? ghost_happy_frame1 : ghost_happy_frame2;
        } else if (tempLow) {
            frameToDraw = (currentFrame == 0) ? ghost_freeze_frame1 : ghost_freeze_frame2;
        } else if (moistLow) {
            frameToDraw = (currentFrame == 0) ? ghost_sweat_frame1 : ghost_sweat_frame2;
        } else if (tempHigh || moistHigh || !lightOk || !condOk) {
            frameToDraw = (currentFrame == 0) ? ghost_sad_frame1 : ghost_sad_frame2;
        }

        tft.pushImage(ghostX, ghostY, GHOST_WIDTH, GHOST_HEIGHT, frameToDraw);
    }

    // Draw visual bars instead of numbers
    int barY = 120;

    // Moisture Bar
    tft.drawRect(60, barY, 200, 10, TFT_WHITE);
    int moistWidth = map(constrain(d.moisture, 0, 100), 0, 100, 0, 200);
    tft.fillRect(60, barY, moistWidth, 10, moistOk ? TFT_BLUE : TFT_RED);
    if (moistWidth < 200) tft.fillRect(60 + moistWidth, barY, 200 - moistWidth, 10, TFT_BLACK); // Clear trailing
    tft.drawString("Water", 10, barY - 2, 2);
    barY += 15;

    // Light Bar
    tft.drawRect(60, barY, 200, 10, TFT_WHITE);
    int lightWidth = map(constrain(d.light, 0, 10000), 0, 10000, 0, 200);
    tft.fillRect(60, barY, lightWidth, 10, lightOk ? TFT_YELLOW : TFT_RED);
    if (lightWidth < 200) tft.fillRect(60 + lightWidth, barY, 200 - lightWidth, 10, TFT_BLACK);
    tft.drawString("Light", 10, barY - 2, 2);
    barY += 15;

    // Temp Bar
    tft.drawRect(60, barY, 200, 10, TFT_WHITE);
    int tempWidth = map(constrain((int)d.temperature, 0, 50), 0, 50, 0, 200);
    tft.fillRect(60, barY, tempWidth, 10, tempOk ? TFT_ORANGE : TFT_RED);
    if (tempWidth < 200) tft.fillRect(60 + tempWidth, barY, 200 - tempWidth, 10, TFT_BLACK);
    tft.drawString("Temp", 10, barY - 2, 2);
    barY += 15;

    // Cond Bar
    tft.drawRect(60, barY, 200, 10, TFT_WHITE);
    int condWidth = map(constrain(d.conductivity, 0, 2000), 0, 2000, 0, 200);
    tft.fillRect(60, barY, condWidth, 10, condOk ? TFT_GREEN : TFT_RED);
    if (condWidth < 200) tft.fillRect(60 + condWidth, barY, 200 - condWidth, 10, TFT_BLACK);
    tft.drawString("Nutri", 10, barY - 2, 2);

    // Auto mode indicator
    if (autoCycleMode) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("AUTO", 260, 10, 2);
    }

    // Pagination
    int dotSpace = 15;
    int startX = 160 - dotSpace;
    for (int i=0; i<3; i++) {
        if (i == currentPlantIndex) {
            // Fill circle for active plant
            tft.fillCircle(startX + i*dotSpace, 210, 4, TFT_WHITE);
            // Indicate UI view below the active plant dot
            tft.drawRect(startX + i*dotSpace - 4, 218, 8, 2, TFT_WHITE); // Tamagotchi mode indicator
        } else {
            tft.drawCircle(startX + i*dotSpace, 210, 4, TFT_WHITE);
        }
    }
}

void drawTechnicalUI() {
    PlantProfile p = profiles[currentPlantIndex];
    xSemaphoreTake(dataMutex, portMAX_DELAY);
    PlantData d = cachedData[currentPlantIndex];
    xSemaphoreGive(dataMutex);

    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString(p.name, 160, 10, 4);

    char buffer[64];

    // Check limits
    bool tempOk = (d.temperature >= p.minTemp && d.temperature <= p.maxTemp);
    bool moistOk = (d.moisture >= p.minMoisture && d.moisture <= p.maxMoisture);
    bool lightOk = (d.light >= p.minLight && d.light <= p.maxLight);
    bool condOk = (d.conductivity >= p.minConductivity && d.conductivity <= p.maxConductivity);

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

    // Network Status
    if (WiFi.status() == WL_CONNECTED) {
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Web: gotchi.local", 140, 180, 2);
        String ipStr = "IP: " + WiFi.localIP().toString();
        tft.drawString(ipStr, 140, 200, 2);
    } else {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("WLAN: Getrennt", 140, 180, 2);
        tft.drawString("(Reconnect...)", 140, 200, 2);
    }

    // Auto mode indicator
    if (autoCycleMode) {
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("AUTO", 260, 10, 2);
    }

    // Pagination
    int dotSpace = 15;
    int startX = 160 - dotSpace;
    for (int i=0; i<3; i++) {
        if (i == currentPlantIndex) {
            // Fill circle for active plant
            tft.fillCircle(startX + i*dotSpace, 210, 4, TFT_WHITE);
            // Indicate UI view below the active plant dot
            tft.drawRect(startX + i*dotSpace - 4, 218, 2, 2, TFT_WHITE); // Tech mode indicator (dots)
            tft.drawRect(startX + i*dotSpace + 2, 218, 2, 2, TFT_WHITE);
        } else {
            tft.drawCircle(startX + i*dotSpace, 210, 4, TFT_WHITE);
        }
    }
}

void drawBatteryUI() {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawCentreString("Batterie Status", 160, 10, 4);

    char buffer[64];

    // Internal Battery
    float internalVoltage = (analogRead(4) / 4095.0) * 2.0 * 3.3;
    float internalPercent = constrain(map(internalVoltage * 100, 330, 420, 0, 100), 0, 100);
    snprintf(buffer, sizeof(buffer), "Intern: %.2fV (%.0f%%)", internalVoltage, internalPercent);
    tft.drawString(buffer, 10, 60, 4);

    // External Battery
    float extVoltage = (analogRead(extBatPin) / 4095.0) * 3.3 * extBatDivider;
    float extPercent = constrain(map(extVoltage * 100, 330, 420, 0, 100), 0, 100); // Assuming LiPo range, adapt if needed
    snprintf(buffer, sizeof(buffer), "Extern: %.2fV (%.0f%%)", extVoltage, extPercent);
    tft.drawString(buffer, 10, 100, 4);

    // Pagination
    int dotSpace = 15;
    int startX = 160 - dotSpace;
    for (int i=0; i<3; i++) {
        if (i == currentPlantIndex) {
            // Fill circle for active plant
            tft.fillCircle(startX + i*dotSpace, 210, 4, TFT_WHITE);
            // Indicate UI view below the active plant dot
            tft.drawRect(startX + i*dotSpace - 4, 218, 2, 2, TFT_WHITE);
            tft.drawRect(startX + i*dotSpace, 218, 2, 2, TFT_WHITE);
            tft.drawRect(startX + i*dotSpace + 4, 218, 2, 2, TFT_WHITE);
        } else {
            tft.drawCircle(startX + i*dotSpace, 210, 4, TFT_WHITE);
        }
    }
}

bool readFloraData(int index) {
    if (!profiles[index].active || profiles[index].mac.indexOf("XX:XX") != -1) {
        return false;
    }

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
            int raw_moisture = (uint8_t)value[7];
            int calibrated = raw_moisture + profiles[index].moistureOffset;

            xSemaphoreTake(dataMutex, portMAX_DELAY);
            cachedData[index].temperature = ((uint8_t)value[0] + (uint8_t)value[1] * 256) / 10.0;
            cachedData[index].light = (uint8_t)value[3] + (uint8_t)value[4] * 256 + (uint8_t)value[5] * 65536 + (uint8_t)value[6] * 16777216;
            cachedData[index].moisture = constrain(calibrated, 0, 100);
            cachedData[index].conductivity = (uint8_t)value[8] + (uint8_t)value[9] * 256;
            cachedData[index].lastUpdate = millis();
            isOffline[index] = false;
            pushSent_Offline[index] = false;
            xSemaphoreGive(dataMutex);
        }
    }

    pClient->disconnect();
    NimBLEDevice::deleteClient(pClient);
    return true;
}


void pollBLE() {
    isBLEScanning = true;

    // Optional: disconnect WiFi or pause webserver to protect BLE stack if needed,
    // but time-slicing via isBLEScanning for HTTP handlers should be enough.

    for (int i = 0; i < 3; i++) {
        readFloraData(i);
    }

    lastBlePoll = millis();

    // Check if offline
    for (int i=0; i<3; i++) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        PlantData d = cachedData[i];
        xSemaphoreGive(dataMutex);

        if (d.lastUpdate > 0 && millis() - d.lastUpdate > 21600000) {
            isOffline[i] = true;
            if (!pushSent_Offline[i] && profiles[i].active) {
                // We'll queue the offline notification by just calling checkAndSendNotifications which handles it.
            }
        }
    }

    requestUIDraw = true;
    requestLogData = true;

    isBLEScanning = false;
}

void bleTaskFunc(void* parameter) {
    esp_task_wdt_add(NULL);
    for (;;) {
        esp_task_wdt_reset();
        if (millis() - lastBlePoll >= BLE_POLL_INTERVAL) {
            pollBLE();
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void setup() {
    Serial.begin(115200);

    dataMutex = xSemaphoreCreateMutex();
    
    pinMode(BUTTON_PIN, INPUT_PULLUP);
    pinMode(BUTTON_PIN_MODE, INPUT_PULLUP);

    // Hard Factory Reset Check
    int resetCounter = 0;
    while(digitalRead(BUTTON_PIN) == LOW) {
        resetCounter++;
        Serial.printf("Holding reset... %d00ms\n", resetCounter);
        vTaskDelay(100 / portTICK_PERIOD_MS);
        if (resetCounter > 100) {
            Serial.println("Factory Reset Triggered!");
            LittleFS.begin(true);
            LittleFS.format();
            WiFiManager wm;
            wm.resetSettings();
            Preferences prefs;
            prefs.begin("gotchi", false);
            prefs.clear();
            ESP.restart();
        }
    }

    pinMode(15, OUTPUT);
    digitalWrite(15, HIGH);
    
    // PWM Dimming Setup for Backlight
    ledcSetup(0, 5000, 8);
    ledcAttachPin(38, 0);
    ledcWrite(0, 255);
    

    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Gotchi startet...", 10, 80, 4);

    // LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    } else {
        if (!LittleFS.exists("/log.bin")) {
            File f = LittleFS.open("/log.bin", "w");
            if (f) {
                uint32_t initialHead = 0;
                f.write((const uint8_t*)&initialHead, sizeof(initialHead));
                f.close();
            }
        }
        loadConfig();
    }

    // Hardware Watchdog
    esp_task_wdt_init(60, true);
    esp_task_wdt_add(NULL); // Add loopTask (Core 1) to WDT

    // Preferences & WiFiManager
    preferences.begin("gotchi", false);
    ntfyTopic = preferences.getString("ntfy_topic", ntfyTopic);

    WiFiManager wm;
    WiFiManagerParameter custom_ntfy_topic("ntfy", "ntfy.sh Topic", ntfyTopic.c_str(), 64);
    wm.addParameter(&custom_ntfy_topic);

    tft.drawString("WLAN: Gotchi-Setup...", 10, 100, 2);

    if (!wm.autoConnect("Gotchi-Setup")) {
        Serial.println("Failed to connect and hit timeout");
    } else {
        Serial.println("Connected to WiFi!");

        if (MDNS.begin("gotchi")) {
            MDNS.addService("http", "tcp", 80);
        }

        tft.drawString("WLAN: Verbunden!", 10, 120, 2);

        esp_wifi_set_ps(WIFI_PS_MIN_MODEM);

        // Save topic if changed
        if (String(custom_ntfy_topic.getValue()) != ntfyTopic && String(custom_ntfy_topic.getValue()).length() > 0) {
            ntfyTopic = custom_ntfy_topic.getValue();
            preferences.putString("ntfy_topic", ntfyTopic);
        }
    }

    // NTP
    configTzTime(preferences.getString("tz", "CET-1CEST,M3.5.0,M10.5.0/3").c_str(), "pool.ntp.org");

    // WebServer Endpoints
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    server.on("/api/live", HTTP_GET, [](AsyncWebServerRequest *request){
        if (isBLEScanning) {
            request->send(503, "text/plain", "Busy");
            return;
        }
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();
        for (int i=0; i<3; i++) {
            xSemaphoreTake(dataMutex, portMAX_DELAY);
            PlantData d = cachedData[i];
            xSemaphoreGive(dataMutex);

            JsonObject obj = array.add<JsonObject>();
            obj["battery"] = d.battery;
            obj["temperature"] = d.temperature;
            obj["moisture"] = d.moisture;
            obj["light"] = d.light;
            obj["conductivity"] = d.conductivity;
        }
        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request){
        if (isBLEScanning) {
            request->send(503, "text/plain", "Busy");
            return;
        }

        File f = LittleFS.open("/log.bin", "r");
        if (!f) {
            request->send(404, "text/plain", "No data");
            return;
        }

        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        // Skip head index
        f.seek(sizeof(uint32_t));

        while(f.available()) {
            LogEntry entry;
            if (f.read((uint8_t*)&entry, sizeof(LogEntry)) == sizeof(LogEntry)) {
                if (entry.timestamp > 0) {
                    JsonObject obj = array.add<JsonObject>();
                    obj["t"] = entry.timestamp;
                    obj["i"] = entry.plantIdx;
                    obj["m"] = entry.moist;
                }
            }
        }
        f.close();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/export", HTTP_GET, [](AsyncWebServerRequest *request){
        if (isBLEScanning) {
            request->send(503, "text/plain", "Busy");
            return;
        }

        AsyncWebServerResponse *response = request->beginChunkedResponse("text/csv", [](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
            static File f;
            if (index == 0) {
                f = LittleFS.open("/log.bin", "r");
                if (!f) return 0;
                // Skip head
                f.seek(sizeof(uint32_t));

                String header = "timestamp,plantIndex,temp,moisture,light,conductivity\n";
                if (maxLen > header.length()) {
                    memcpy(buffer, header.c_str(), header.length());
                    return header.length();
                }
            }

            if (!f || !f.available()) {
                if (f) f.close();
                return 0;
            }

            size_t bytesWritten = 0;
            char lineBuf[128];
            while (f.available() && maxLen - bytesWritten > sizeof(lineBuf)) {
                LogEntry entry;
                if (f.read((uint8_t*)&entry, sizeof(LogEntry)) == sizeof(LogEntry)) {
                    if (entry.timestamp > 0) {
                        int len = snprintf(lineBuf, sizeof(lineBuf), "%u,%u,%.1f,%u,%u,%u\n",
                                           entry.timestamp, entry.plantIdx, entry.temp, entry.moist, entry.light, entry.cond);
                        if (maxLen - bytesWritten >= len) {
                            memcpy(buffer + bytesWritten, lineBuf, len);
                            bytesWritten += len;
                        } else {
                            // Back up file pointer, we can't fit this
                            f.seek(f.position() - sizeof(LogEntry));
                            break;
                        }
                    }
                }
            }

            if (!f.available()) {
                f.close();
            }

            return bytesWritten;
        });

        response->addHeader("Content-Disposition", "attachment; filename=\"gotchi_history.csv\"");
        request->send(response);
    });

    server.on("/api/push_test", HTTP_POST, [](AsyncWebServerRequest *request){
        if (isBLEScanning) {
            request->send(503, "text/plain", "Busy");
            return;
        }
        requestTestPush = true;
        request->send(200, "text/plain", "Test queued");
    });

    server.on("/api/config", HTTP_GET, [](AsyncWebServerRequest *request){
        if (isBLEScanning) {
            request->send(503, "text/plain", "Busy");
            return;
        }
        File f = LittleFS.open("/config.json", "r");
        if (!f) {
            request->send(404, "text/plain", "Config not found");
            return;
        }
        String configData = f.readString();
        f.close();
        request->send(200, "application/json", configData);
    });

    server.on("/api/config", HTTP_POST, [](AsyncWebServerRequest *request){}, NULL,
      [](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
        if (isBLEScanning) {
            request->send(503, "text/plain", "Busy");
            return;
        }

        if (index == 0 && total == len) {
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, data, len);
            if (error) {
                request->send(400, "text/plain", "Invalid JSON");
                return;
            }

            File f = LittleFS.open("/config.json", "w");
            if (!f) {
                request->send(500, "text/plain", "Could not write file");
                return;
            }
            serializeJson(doc, f);
            f.close();

            loadConfig();

            request->send(200, "text/plain", "Config saved");
        } else {
            request->send(400, "text/plain", "Payload too large or chunked");
        }
    });

    ElegantOTA.begin(&server);

    server.begin();



    NimBLEDevice::init("LilyGo-Gotchi");
    
    // Delay first BLE scan by 10s
    lastBlePoll = millis() - BLE_POLL_INTERVAL + 10000;
    
    // If the initial poll failed (lastUpdate remains 0), we still draw UI
    drawUI();

    xTaskCreatePinnedToCore(
        bleTaskFunc,
        "bleTask",
        8192,
        NULL,
        1,
        NULL,
        0
    );
}

void loop() {
    esp_task_wdt_reset();
    unsigned long currentMillis = millis();

    static unsigned long lastWiFiCheck = 0;
    if (currentMillis - lastWiFiCheck >= 10000) {
        lastWiFiCheck = currentMillis;
        if (WiFi.status() != WL_CONNECTED) {
            WiFi.disconnect();
            WiFi.reconnect();
        }
    }

    if (requestTestPush) {
        requestTestPush = false;
        checkAndSendNotifications(true);
    }

    if (requestUIDraw) {
        requestUIDraw = false;
        drawUI();
    }

    if (requestLogData) {
        requestLogData = false;
        logData();
        checkAndSendNotifications();
    }

    // BLE Polling is now handled by bleTask


    // Dimming Logic
    unsigned long timeSinceInteraction = currentMillis - lastInteractionTime;
    if (timeSinceInteraction < 60000) {
        ledcWrite(0, 255);
    } else if (timeSinceInteraction >= 60000 && timeSinceInteraction < 120000) {
        // Fade from 255 to 50 over 60 seconds
        int fadeValue = map(timeSinceInteraction, 60000, 120000, 255, 50);
        ledcWrite(0, constrain(fadeValue, 50, 255));
    } else {
        ledcWrite(0, 0);
    }

    // Auto Cycle Logic
    if (autoCycleMode) {
        if (currentMillis - lastAutoCycle >= AUTO_CYCLE_INTERVAL) {
            currentPlantIndex = (currentPlantIndex + 1) % 3;
            drawUI();
            lastAutoCycle = currentMillis;
        }
    }

    // Animation Logic
    if (uiMode == 0 && (currentMillis - lastAnimUpdate > 500)) { // 500ms per frame
        currentFrame = (currentFrame + 1) % 2;
        lastAnimUpdate = currentMillis;

        // Instead of redrawing the whole UI (which causes flicker due to fillScreen),
        // we just update the avatar.
        PlantProfile p = profiles[currentPlantIndex];
        PlantData d = cachedData[currentPlantIndex];

        bool tempLow = (d.temperature < p.minTemp);
        bool tempHigh = (d.temperature > p.maxTemp);
        bool moistLow = (d.moisture < p.minMoisture);
        bool moistHigh = (d.moisture > p.maxMoisture);
        bool lightOk = (d.light >= p.minLight && d.light <= p.maxLight);
        bool condOk = (d.conductivity >= p.minConductivity && d.conductivity <= p.maxConductivity);
        bool allOk = (!tempLow && !tempHigh && !moistLow && !moistHigh && lightOk && condOk);

        int ghostX = (320 - GHOST_WIDTH) / 2;
        int ghostY = 40;
        const uint16_t* frameToDraw = ghost_idle_frame1;

        if (allOk) {
            frameToDraw = (currentFrame == 0) ? ghost_happy_frame1 : ghost_happy_frame2;
        } else if (tempLow) {
            frameToDraw = (currentFrame == 0) ? ghost_freeze_frame1 : ghost_freeze_frame2;
        } else if (moistLow) {
            frameToDraw = (currentFrame == 0) ? ghost_sweat_frame1 : ghost_sweat_frame2;
        } else if (tempHigh || moistHigh || !lightOk || !condOk) {
            frameToDraw = (currentFrame == 0) ? ghost_sad_frame1 : ghost_sad_frame2;
        }

        tft.pushImage(ghostX, ghostY, GHOST_WIDTH, GHOST_HEIGHT, frameToDraw);
    }

    // Button Logic GPIO 0 (Cycle)
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
                lastInteractionTime = currentMillis;
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

    // Button Logic GPIO 14 (Mode Toggle)
    int readingMode = digitalRead(BUTTON_PIN_MODE);
    if (readingMode != lastButtonModeState) {
        lastDebounceModeTime = currentMillis;
    }

    if ((currentMillis - lastDebounceModeTime) > debounceDelay) {
        if (readingMode != buttonModeState) {
            buttonModeState = readingMode;

            if (buttonModeState == LOW) {
                // Button pressed
                buttonModePressTime = currentMillis;
                lastInteractionTime = currentMillis;
            } else {
                // Button released
                unsigned long pressDuration = currentMillis - buttonModePressTime;

                if (pressDuration < 500) {
                    // Short press
                    uiMode = (uiMode + 1) % 3;
                    tft.fillScreen(TFT_BLACK);
                    drawUI();
                }
            }
        }
    }
    lastButtonModeState = readingMode;
}


void logData() {
    time_t now;
    time(&now);

    if (now < 1000000000) {
        return; // NTP not synced yet
    }

    File f = LittleFS.open("/log.bin", "r+");
    if (!f) {
        return;
    }

    uint32_t headIndex = 0;
    f.read((uint8_t*)&headIndex, sizeof(headIndex));

    for (int i=0; i<3; i++) {
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        PlantData d = cachedData[i];
        xSemaphoreGive(dataMutex);

        if (d.lastUpdate > 0) {
            LogEntry entry;
            entry.timestamp = (uint32_t)now;
            entry.plantIdx = i;
            entry.temp = d.temperature;
            entry.moist = d.moisture;
            entry.light = d.light;
            entry.cond = d.conductivity;

            f.seek(sizeof(uint32_t) + (headIndex * sizeof(LogEntry)));
            f.write((const uint8_t*)&entry, sizeof(LogEntry));

            headIndex++;
            if (headIndex >= MAX_LOG_ENTRIES) {
                headIndex = 0;
            }
        }
    }

    // Update head
    f.seek(0);
    f.write((const uint8_t*)&headIndex, sizeof(headIndex));
    f.close();
}

void checkAndSendNotifications(bool testPush) {
    if (WiFi.status() != WL_CONNECTED) return;
    if (ntfyTopic.indexOf("ntfy.sh/") == -1 && !ntfyTopic.startsWith("http")) {
        // Simple prepend if user just entered topic name
        ntfyTopic = "https://ntfy.sh/" + ntfyTopic;
    } else if (!ntfyTopic.startsWith("http")) {
        ntfyTopic = "https://" + ntfyTopic;
    }

    for (int i=0; i<3; i++) {
        PlantProfile p = profiles[i];
        xSemaphoreTake(dataMutex, portMAX_DELAY);
        PlantData d = cachedData[i];
        xSemaphoreGive(dataMutex);

        if (d.lastUpdate == 0) continue;

        bool moistLow = d.moisture < p.minMoisture;
        bool moistOk = d.moisture >= p.minMoisture && d.moisture <= p.maxMoisture;

        if (isOffline[i] && !pushSent_Offline[i]) {
            HTTPClient http;
            http.begin(ntfyTopic);
            http.addHeader("Title", String(p.name) + " ist Offline!");
            http.addHeader("Tags", "warning");
            String msg = String(p.name) + " hat sich seit über 6 Stunden nicht gemeldet.";
            http.POST(msg);
            http.end();
            pushSent_Offline[i] = true;
        }

        if ((moistLow && !pushSent_Moisture[i]) || testPush) {
            HTTPClient http;
            http.begin(ntfyTopic);
            http.addHeader("Title", String(p.name) + " braucht Wasser!");
            http.addHeader("Tags", "droplet");
            String msg = String(p.name) + " hat nur noch " + String(d.moisture) + "% Feuchtigkeit.";
            http.POST(msg);
            http.end();
            pushSent_Moisture[i] = true;

            // Wake up display on push, unless in night mode
            struct tm timeinfo;
            if (getLocalTime(&timeinfo)) {
                int currentMins = timeinfo.tm_hour * 60 + timeinfo.tm_min;
                int startHour = nightModeStart.substring(0, 2).toInt();
                int startMin = nightModeStart.substring(3, 5).toInt();
                int endHour = nightModeEnd.substring(0, 2).toInt();
                int endMin = nightModeEnd.substring(3, 5).toInt();

                int startMins = startHour * 60 + startMin;
                int endMins = endHour * 60 + endMin;

                bool inNightMode = false;
                if (startMins <= endMins) {
                    if (currentMins >= startMins && currentMins <= endMins) inNightMode = true;
                } else {
                    if (currentMins >= startMins || currentMins <= endMins) inNightMode = true;
                }

                if (!inNightMode) {
                    lastInteractionTime = millis();
                }
            } else {
                lastInteractionTime = millis(); // Fallback if no time
            }
        } else if (moistOk && pushSent_Moisture[i]) {
            pushSent_Moisture[i] = false;
        }
    }
}
void loadConfig() {
    File f = LittleFS.open("/config.json", "r");
    if (!f) {
        Serial.println("Config not found, creating default config.json");
        f = LittleFS.open("/config.json", "w");
        if (f) {
            f.print("[\n");
            f.print("  {\"active\":true,\"name\":\"Monstera\",\"mac\":\"C4:7C:8D:XX:XX:01\",\"minTemp\":18.0,\"maxTemp\":28.0,\"minMoisture\":20,\"maxMoisture\":60,\"minLight\":1000,\"maxLight\":4000,\"minConductivity\":350,\"maxConductivity\":1000},\n");
            f.print("  {\"active\":true,\"name\":\"Elefantenfuss\",\"mac\":\"C4:7C:8D:XX:XX:02\",\"minTemp\":15.0,\"maxTemp\":30.0,\"minMoisture\":5,\"maxMoisture\":20,\"minLight\":2000,\"maxLight\":8000,\"minConductivity\":150,\"maxConductivity\":500},\n");
            f.print("  {\"active\":true,\"name\":\"Clusia\",\"mac\":\"C4:7C:8D:XX:XX:03\",\"minTemp\":18.0,\"maxTemp\":28.0,\"minMoisture\":20,\"maxMoisture\":50,\"minLight\":1500,\"maxLight\":5000,\"minConductivity\":300,\"maxConductivity\":800}\n");
            f.print("]\n");
            f.close();
        }
        f = LittleFS.open("/config.json", "r");
    }

    if (f) {
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, f);
        if (error) {
            Serial.println(F("Failed to read config.json"));
        } else {
            JsonArray arr = doc.as<JsonArray>();
            for (int i = 0; i < 3; i++) {
                if (i < arr.size()) {
                    JsonObject obj = arr[i];
                    profiles[i].active = obj["active"] | false;
                    profiles[i].name = obj["name"].as<String>();
                    profiles[i].mac = obj["mac"].as<String>();
                    profiles[i].minTemp = obj["minTemp"] | 0.0;
                    profiles[i].maxTemp = obj["maxTemp"] | 50.0;
                    profiles[i].minMoisture = obj["minMoisture"] | 0;
                    profiles[i].maxMoisture = obj["maxMoisture"] | 100;
                    profiles[i].minLight = obj["minLight"] | 0;
                    profiles[i].maxLight = obj["maxLight"] | 10000;
                    profiles[i].minConductivity = obj["minConductivity"] | 0;
                    profiles[i].maxConductivity = obj["maxConductivity"] | 2000;
                }
            }
        }
        f.close();
    }
}

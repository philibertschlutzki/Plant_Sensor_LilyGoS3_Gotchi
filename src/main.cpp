#include <Arduino.h>
#include <TFT_eSPI.h>
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <Preferences.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"
#include "xbms.h"

Preferences preferences;
AsyncWebServer server(80);
String ntfyTopic = "ntfy.sh/mein_gotchi_geheim_123"; // default fallback

bool isBLEScanning = false;
bool requestTestPush = false;
bool pushSent_Moisture[3] = {false, false, false};
bool pushSent_Temp[3] = {false, false, false};



void drawUI();
void logData();
void checkAndSendNotifications(bool testPush = false);
void pollBLE();

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
    isBLEScanning = true;

    tft.fillRect(0, 220, 320, 20, TFT_BLACK);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("Lese Sensoren...", 10, 220, 2);
    
    // Optional: disconnect WiFi or pause webserver to protect BLE stack if needed,
    // but time-slicing via isBLEScanning for HTTP handlers should be enough.

    for (int i = 0; i < 3; i++) {
        readFloraData(i);
    }

    lastBlePoll = millis();
    drawUI(); // Refresh UI after polling

    logData();
    checkAndSendNotifications();

    isBLEScanning = false;
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

    // LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    } else {
        if (!LittleFS.exists("/log.csv")) {
            File f = LittleFS.open("/log.csv", "w");
            if (f) {
                f.println("timestamp,plantIndex,temp,moisture,light,conductivity");
                f.close();
            }
        }
    }

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
        tft.drawString("WLAN: Verbunden!", 10, 120, 2);

        // Save topic if changed
        if (String(custom_ntfy_topic.getValue()) != ntfyTopic && String(custom_ntfy_topic.getValue()).length() > 0) {
            ntfyTopic = custom_ntfy_topic.getValue();
            preferences.putString("ntfy_topic", ntfyTopic);
        }
    }

    // NTP
    configTzTime("CET-1CEST,M3.5.0,M10.5.0/3", "ch.pool.ntp.org");

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
            JsonObject obj = array.add<JsonObject>();
            obj["battery"] = cachedData[i].battery;
            obj["temperature"] = cachedData[i].temperature;
            obj["moisture"] = cachedData[i].moisture;
            obj["light"] = cachedData[i].light;
            obj["conductivity"] = cachedData[i].conductivity;
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

        File f = LittleFS.open("/log.csv", "r");
        if (!f) {
            request->send(404, "text/plain", "No data");
            return;
        }

        // Read CSV and convert to JSON
        JsonDocument doc;
        JsonArray array = doc.to<JsonArray>();

        f.readStringUntil('\n'); // skip header
        while(f.available()) {
            String line = f.readStringUntil('\n');
            if (line.length() > 0) {
                int firstComma = line.indexOf(',');
                int secondComma = line.indexOf(',', firstComma+1);
                int thirdComma = line.indexOf(',', secondComma+1);

                if (firstComma > 0 && secondComma > 0 && thirdComma > 0) {
                    JsonObject obj = array.add<JsonObject>();
                    obj["t"] = line.substring(0, firstComma).toInt();
                    obj["i"] = line.substring(firstComma+1, secondComma).toInt();
                    // We only chart moisture for now to save RAM
                    int moistureComma = line.indexOf(',', thirdComma+1);
                    obj["m"] = line.substring(thirdComma+1, moistureComma).toInt();
                }
            }
        }
        f.close();

        String response;
        serializeJson(doc, response);
        request->send(200, "application/json", response);
    });

    server.on("/api/push_test", HTTP_POST, [](AsyncWebServerRequest *request){
        if (isBLEScanning) {
            request->send(503, "text/plain", "Busy");
            return;
        }
        requestTestPush = true;
        request->send(200, "text/plain", "Test queued");
    });

    server.begin();



    NimBLEDevice::init("LilyGo-Gotchi");
    
    // Initial poll
    pollBLE();
    
    // If the initial poll failed (lastUpdate remains 0), we still draw UI
    drawUI();
}

void loop() {
    unsigned long currentMillis = millis();

    if (requestTestPush) {
        requestTestPush = false;
        checkAndSendNotifications(true);
    }


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


void logData() {
    time_t now;
    time(&now);

    // Check file size for ring buffer
    File f = LittleFS.open("/log.csv", "r");
    if (f && f.size() > 50000) { // Limit to ~50KB
        // Read lines, skip first ~20%, write back to temporary file
        File newF = LittleFS.open("/log_temp.csv", "w");
        if (newF) {
            newF.println("timestamp,plantIndex,temp,moisture,light,conductivity");

            f.readStringUntil('\n'); // skip old header

            int linesToSkip = 50; // Skip 50 entries
            while(linesToSkip > 0 && f.available()) {
                f.readStringUntil('\n');
                linesToSkip--;
            }

            while(f.available()) {
                newF.print(f.readStringUntil('\n') + "\n");
            }
            newF.close();
        }
        f.close();
        LittleFS.remove("/log.csv");
        LittleFS.rename("/log_temp.csv", "/log.csv");
    } else if (f) {
        f.close();
    }

    f = LittleFS.open("/log.csv", "a");
    if (f) {
        for(int i=0; i<3; i++) {
            PlantData d = cachedData[i];
            if (d.lastUpdate > 0) { // Only log if we have data
                char buffer[128];
                snprintf(buffer, sizeof(buffer), "%ld,%d,%.1f,%d,%d,%d",
                    (long)now, i, d.temperature, d.moisture, d.light, d.conductivity);
                f.println(buffer);
            }
        }
        f.close();
    }
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
        PlantData d = cachedData[i];

        if (d.lastUpdate == 0) continue;

        bool moistLow = d.moisture < p.minMoisture;
        bool moistOk = d.moisture >= p.minMoisture && d.moisture <= p.maxMoisture;

        if ((moistLow && !pushSent_Moisture[i]) || testPush) {
            HTTPClient http;
            http.begin(ntfyTopic);
            http.addHeader("Title", String(p.name) + " braucht Wasser!");
            http.addHeader("Tags", "droplet");
            String msg = String(p.name) + " hat nur noch " + String(d.moisture) + "% Feuchtigkeit.";
            http.POST(msg);
            http.end();
            pushSent_Moisture[i] = true;
        } else if (moistOk && pushSent_Moisture[i]) {
            pushSent_Moisture[i] = false;
        }
    }
}

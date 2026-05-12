#include "Renderer.h"
#include <LittleFS.h>

GotchiRenderer::GotchiRenderer() {
    sprite = nullptr;
    lastAnimUpdate = 0;
    lastParticleUpdate = 0;
    currentFrame = 0;
    nextBlinkTime = 0;
    isBlinking = false;
    blinkEndTime = 0;
    isWateringEcstasy = false;
    wateringEcstasyEndTime = 0;
    isSurprised = false;
    surprisedEndTime = 0;
    ecstasyYOffset = 0;
    ecstasyYDir = 1;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        particles[i].life = 0;
    }
}

void GotchiRenderer::init(TFT_eSPI* tft) {
    sprite = new TFT_eSprite(tft);
    sprite->createSprite(320, 170);
    sprite->setSwapBytes(true); // Since PROGMEM arrays are usually little-endian RGB565, or we may need to swap based on macro
    sprite->setTextDatum(TC_DATUM); // Set text alignment to center

    if (LittleFS.exists("/tamagotchi.vlw")) {
        sprite->loadFont("/tamagotchi.vlw", LittleFS);
    } else {
        Serial.println("Font /tamagotchi.vlw not found in LittleFS!");
    }
}

void GotchiRenderer::spawnParticle(ParticleType type, float x, float y) {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life == 0) {
            particles[i].x = x;
            particles[i].y = y;
            particles[i].type = type;

            if (type == PARTICLE_HEART) {
                particles[i].speedX = (random(10, 30) / 10.0f) * (random(0, 2) == 0 ? -1 : 1);
                particles[i].speedY = -random(10, 25) / 10.0f;
                particles[i].color = 0xF800; // RED
                particles[i].life = random(40, 80);
            } else if (type == PARTICLE_DUST) {
                particles[i].speedX = random(20, 50) / 10.0f; // Fast horizontal
                particles[i].speedY = (random(0, 20) - 10) / 20.0f;
                particles[i].color = 0xFD20; // ORANGE/BROWN
                particles[i].life = random(30, 60);
            } else if (type == PARTICLE_ZZZ) {
                particles[i].speedX = (random(0, 20) - 10) / 20.0f;
                particles[i].speedY = -random(5, 15) / 10.0f;
                particles[i].color = 0xFFFF; // WHITE
                particles[i].life = random(50, 100);
            }
            break;
        }
    }
}

void GotchiRenderer::updateParticles(unsigned long currentMillis) {
    if (currentMillis - lastParticleUpdate < 30) {
        return; // ~33 FPS for particles
    }
    lastParticleUpdate = currentMillis;

    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life > 0) {
            particles[i].x += particles[i].speedX;
            particles[i].y += particles[i].speedY;

            if (particles[i].type == PARTICLE_HEART) {
                particles[i].x += sin(particles[i].life * 0.2f) * 1.5f; // Sine wave wobble
            }

            particles[i].life--;
        }
    }
}

void GotchiRenderer::drawParticles() {
    for (int i = 0; i < MAX_PARTICLES; i++) {
        if (particles[i].life > 0) {
            if (particles[i].type == PARTICLE_HEART) {
                // Draw simple heart shape
                int px = (int)particles[i].x;
                int py = (int)particles[i].y;
                sprite->fillCircle(px - 2, py, 2, particles[i].color);
                sprite->fillCircle(px + 2, py, 2, particles[i].color);
                sprite->fillTriangle(px - 4, py + 1, px + 4, py + 1, px, py + 5, particles[i].color);
            } else if (particles[i].type == PARTICLE_DUST) {
                sprite->fillRect((int)particles[i].x, (int)particles[i].y, 2, 2, particles[i].color);
            } else if (particles[i].type == PARTICLE_ZZZ) {
                sprite->setTextColor(particles[i].color);
                sprite->drawString("z", (int)particles[i].x, (int)particles[i].y);
            }
        }
    }
}


void GotchiRenderer::triggerWakeupScare() {
    isSurprised = true;
    surprisedEndTime = millis() + 2000;
}

void GotchiRenderer::triggerWateringEcstasy() {
    isWateringEcstasy = true;
    wateringEcstasyEndTime = millis() + 60000;
}

void GotchiRenderer::drawTable() {
    sprite->drawLine(0, 130, 320, 130, TFT_DARKGREY);
    sprite->drawLine(0, 131, 320, 131, TFT_DARKGREY);
}

void GotchiRenderer::drawThirstBubble(int x, int y) {
    if ((millis() / 500) % 3 != 0) { // Pulsate 1000ms visible, 500ms invisible
        sprite->fillRoundRect(x, y, 40, 30, 8, TFT_WHITE);
        sprite->fillTriangle(x + 5, y + 25, x + 15, y + 25, x + 5, y + 35, TFT_WHITE);
        sprite->setTextColor(TFT_BLUE);
        sprite->drawString("?", x + 15, y + 5); // Fallback if no emoji
        // Draw drop icon
        sprite->pushImage(x + 5, y + 7, ICON_W, ICON_H, icon_drop_full);
    }
}

void GotchiRenderer::drawSunOverlay() {
    // Draw sunglasses over the avatar. Avatar is 64x64, drawn at (320-64)/2 = 128, 40
    sprite->pushImage(128, 40, AVATAR_W, AVATAR_H, (uint16_t*)overlay_sunglasses, 0x0000); // Assuming 0 is transparent
}

void GotchiRenderer::drawNeedIcon(int x, int y, int value, int minVal, int maxVal, const uint16_t* fullIcon, const uint16_t* emptyIcon, const uint16_t* grayIcon, bool isOffline) {
    int level = 0;
    if (value >= maxVal) level = 4;
    else if (value <= minVal) level = 0;
    else {
        level = map(value, minVal, maxVal, 1, 4);
    }

    for (int i = 0; i < 4; i++) {
        const uint16_t* iconToDraw = emptyIcon;
        if (isOffline) {
            iconToDraw = grayIcon;
        } else if (i < level) {
            iconToDraw = fullIcon;
        }
        sprite->pushImage(x + i * 20, y, ICON_W, ICON_H, iconToDraw);
    }
}

void GotchiRenderer::drawThermometer(int x, int y, float temp, float minTemp, float maxTemp) {
    if (temp < minTemp) {
        sprite->pushImage(x, y, ICON_W, ICON_H, icon_temp_blue);
    } else if (temp > maxTemp) {
        sprite->pushImage(x, y, ICON_W, ICON_H, icon_temp_red);
    }
}

void GotchiRenderer::drawNeedIndicators(const PlantProfile& profile, const PlantData& data, bool isOffline) {
    int startY = 135;
    // Moisture (Drops)
    sprite->drawString("Wasser", 10, startY);
    drawNeedIcon(70, startY, data.moisture, profile.minMoisture, profile.maxMoisture, icon_drop_full, icon_drop_empty, icon_drop_gray, isOffline);

    // Light (Suns)
    sprite->drawString("Licht", 170, startY);
    drawNeedIcon(230, startY, data.light, profile.minLight, profile.maxLight, icon_sun_full, icon_sun_empty, icon_sun_gray, isOffline);

    // Conductivity (Leaves)
    sprite->drawString("Dünger", 10, startY + 20);
    drawNeedIcon(70, startY + 20, data.conductivity, profile.minConductivity, profile.maxConductivity, icon_leaf_full, icon_leaf_empty, icon_leaf_gray, isOffline);
}

const uint16_t* GotchiRenderer::getAvatarFrame(const PlantProfile& profile, const PlantData& data, bool isOffline, bool inNightMode, int plantIdx, bool& allOk) {
    bool tempLow = (data.temperature < profile.minTemp);
    bool tempHigh = (data.temperature > profile.maxTemp);
    bool moistLow = (data.moisture < profile.minMoisture);
    bool moistHigh = (data.moisture > profile.maxMoisture);
    bool lightOk = (data.light >= profile.minLight && data.light <= profile.maxLight);
    bool condOk = (data.conductivity >= profile.minConductivity && data.conductivity <= profile.maxConductivity);
    allOk = (!tempLow && !tempHigh && !moistLow && !moistHigh && lightOk && condOk);

    const uint16_t* frame = nullptr;

    if (isOffline) {
        if (plantIdx == 0) frame = p0_offline;
        else if (plantIdx == 1) frame = p1_offline;
        else frame = p2_offline;
        return frame;
    }

    if (isSurprised) {
        if (plantIdx == 0) frame = p0_surprised;
        else if (plantIdx == 1) frame = p1_surprised;
        else frame = p2_surprised;
        return frame;
    }

    if (inNightMode) {
        if (plantIdx == 0) frame = p0_sleep;
        else if (plantIdx == 1) frame = p1_sleep;
        else frame = p2_sleep;
        return frame;
    }

    if (isBlinking) {
        if (plantIdx == 0) frame = p0_closed;
        else if (plantIdx == 1) frame = p1_closed;
        else frame = p2_closed;
        return frame;
    }

    if (allOk || isWateringEcstasy) {
        if (plantIdx == 0) frame = p0_happy;
        else if (plantIdx == 1) frame = p1_happy;
        else frame = p2_happy;
    } else if (tempLow) {
        if (plantIdx == 0) frame = p0_freeze;
        else if (plantIdx == 1) frame = p1_freeze;
        else frame = p2_freeze;
    } else {
        // sad covers moistLow, tempHigh, moistHigh, etc.
        if (plantIdx == 0) frame = p0_sad;
        else if (plantIdx == 1) frame = p1_sad;
        else frame = p2_sad;
    }

    return frame;
}

void GotchiRenderer::update(unsigned long currentMillis, const PlantProfile& profile, const PlantData& data, bool isOffline, bool inNightMode, int currentPlantIndex) {
    if (isSurprised && currentMillis > surprisedEndTime) {
        isSurprised = false;
    }

    if (isWateringEcstasy && currentMillis > wateringEcstasyEndTime) {
        isWateringEcstasy = false;
    }

    // Asynchronous blinking logic
    bool tempLow = (data.temperature < profile.minTemp);
    bool isFreeze = tempLow; // Simplified check for Freeze state

    // Only blink if not in freeze state (so Happy or Sad)
    if (!isBlinking && currentMillis > nextBlinkTime && !isSurprised && !inNightMode && !isFreeze) {
        isBlinking = true;
        // If low light, blink more often AND blink duration is drastically longer
        int blinkDuration = (data.light < 200) ? random(800, 2000) : 200;
        blinkEndTime = currentMillis + blinkDuration;

        int nextDelay = (data.light < 200) ? random(500, 1500) : random(2000, 6000);
        nextBlinkTime = blinkEndTime + nextDelay;
    }
    if (isBlinking && currentMillis > blinkEndTime) {
        isBlinking = false;
    }

    updateParticles(currentMillis);

    // Desert background check
    bool isDesert = (!isOffline && data.moisture < profile.minMoisture);
    uint16_t bgColor = isDesert ? 0x8B40 : TFT_BLACK; // 0x8B40 is ocher
    if (inNightMode) bgColor = 0x0010; // Dark blue

    sprite->fillSprite(bgColor);

    // Table
    drawTable();

    // Spawning particles based on environment
    if (currentMillis - lastAnimUpdate > 100) {
        lastAnimUpdate = currentMillis;

        if (isDesert && random(0, 100) < 30) {
            spawnParticle(PARTICLE_DUST, 0, random(50, 130));
        }
        if (inNightMode && random(0, 100) < 10) {
            spawnParticle(PARTICLE_ZZZ, random(100, 220), random(50, 100));
        }
        if (!isOffline && !inNightMode) {
            bool tempLow = (data.temperature < profile.minTemp);
            bool tempHigh = (data.temperature > profile.maxTemp);
            bool moistLow = (data.moisture < profile.minMoisture);
            bool moistHigh = (data.moisture > profile.maxMoisture);
            bool lightOk = (data.light >= profile.minLight && data.light <= profile.maxLight);
            bool condOk = (data.conductivity >= profile.minConductivity && data.conductivity <= profile.maxConductivity);
            bool allOk = (!tempLow && !tempHigh && !moistLow && !moistHigh && lightOk && condOk);

            if (isWateringEcstasy && random(0, 100) < 50) {
                spawnParticle(PARTICLE_HEART, random(100, 220), random(80, 120));

                // Fast Y wobble
                ecstasyYOffset += ecstasyYDir * 2;
                if (ecstasyYOffset > 3) ecstasyYDir = -1;
                if (ecstasyYOffset < -3) ecstasyYDir = 1;
            } else if (allOk && random(0, 100) < 10) {
                spawnParticle(PARTICLE_HEART, random(120, 200), random(80, 110));
                ecstasyYOffset = 0; // Reset
            } else {
                ecstasyYOffset = 0;
            }
        }
    }

    drawParticles();

    // Night mode moon
    if (inNightMode) {
        sprite->fillCircle(250, 40, 20, TFT_YELLOW);
        sprite->fillCircle(240, 35, 20, 0x0010); // Cutout to make crescent
    }

    // Avatar
    bool allOk = false;
    const uint16_t* avatarFrame = getAvatarFrame(profile, data, isOffline, inNightMode, currentPlantIndex, allOk);
    int avatarX = 128;
    int avatarY = 60 + ecstasyYOffset; // Resting on table somewhat (60+64 = 124, table is 130)

    if (avatarFrame != nullptr) {
        sprite->pushImage(avatarX, avatarY, AVATAR_W, AVATAR_H, avatarFrame);
    }

    if (data.light > 5000 && !inNightMode && !isOffline) {
        sprite->pushImage(avatarX, avatarY, AVATAR_W, AVATAR_H, (uint16_t*)overlay_sunglasses, 0x0000);
    }

    // Thermometer
    drawThermometer(avatarX + 70, avatarY + 20, data.temperature, profile.minTemp, profile.maxTemp);

    // Thirst bubble
    if (!isOffline && data.moisture < 10) {
        drawThirstBubble(avatarX - 45, avatarY);
    }

    // Offline warning
    if (isOffline) {
        sprite->setTextColor(TFT_RED);
        sprite->drawString("Sensor Offline", 160, 20);
    } else {
        sprite->setTextColor(TFT_WHITE);
        sprite->drawString(profile.name, 160, 10);
    }

    // Battery warning
    if (!isOffline && data.battery < 15) {
        sprite->pushImage(280, 10, ICON_W, ICON_H, icon_batt_warn);
    }

    drawNeedIndicators(profile, data, isOffline);

    sprite->pushSprite(0, 0);
}

#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include <TFT_eSPI.h>
#include "animations.h"

#include "types.h"

// Particle type definitions
enum ParticleType {
    PARTICLE_HEART,
    PARTICLE_DUST,
    PARTICLE_ZZZ
};

struct Particle {
    float x;
    float y;
    float speedX;
    float speedY;
    uint16_t color;
    uint8_t life;
    uint8_t type;
};

#define MAX_PARTICLES 30

class GotchiRenderer {
public:
    GotchiRenderer();

    void init(TFT_eSPI* tft);
    void update(unsigned long currentMillis, const PlantProfile& profile, const PlantData& data, bool isOffline, bool inNightMode, int currentPlantIndex);

    // Event triggers
    void triggerWakeupScare();
    void triggerWateringEcstasy();

private:
    TFT_eSprite* sprite;

    // Animation state
    unsigned long lastAnimUpdate;
    unsigned long lastParticleUpdate;
    int currentFrame;

    // Asynchronous blinking
    unsigned long nextBlinkTime;
    bool isBlinking;
    unsigned long blinkEndTime;

    // Watering ecstasy
    bool isWateringEcstasy;
    unsigned long wateringEcstasyEndTime;

    // Wakeup scare
    bool isSurprised;
    unsigned long surprisedEndTime;

    // Y-axis bouncing for ecstasy
    int ecstasyYOffset;
    int ecstasyYDir;

    // Particle system
    Particle particles[MAX_PARTICLES];
    void updateParticles(unsigned long currentMillis);
    void drawParticles();
    void spawnParticle(ParticleType type, float x, float y);

    // Need indicators
    void drawNeedIndicators(const PlantProfile& profile, const PlantData& data, bool isOffline);
    void drawNeedIcon(int x, int y, int value, int minVal, int maxVal, const uint16_t* fullIcon, const uint16_t* emptyIcon, const uint16_t* grayIcon, bool isOffline);
    void drawThermometer(int x, int y, float temp, float minTemp, float maxTemp);

    // Environment/Bubble
    void drawThirstBubble(int x, int y);
    void drawTable();
    void drawSunOverlay(int avatarX, int avatarY);

    // Helper to select the correct avatar frame
    const uint16_t* getAvatarFrame(const PlantProfile& profile, const PlantData& data, bool isOffline, bool inNightMode, int plantIdx, bool& allOk);
};

#endif

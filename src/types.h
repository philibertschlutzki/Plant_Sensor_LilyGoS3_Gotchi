#ifndef TYPES_H
#define TYPES_H

#include <Arduino.h>

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

struct PlantData {
    int battery;
    float temperature;
    int moisture;
    int light;
    int conductivity;
    unsigned long lastUpdate;
};

#endif

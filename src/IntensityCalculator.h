// IntensityCalculator.h
#pragma once

#include <Arduino.h>

class IntensityCalculator
{
private:
    static const uint8_t NUM_BANDS = 8;
    uint8_t intensity[NUM_BANDS];
    int32_t peak[NUM_BANDS];
    float amplitude;
    float sensitivity;

    uint8_t calculateWeightedIntensity(const uint8_t weights[]);

public:
    IntensityCalculator();
    void updateIntensity(double vReal[], int samples, float newAmplitude);
    uint8_t getWeightedIntensity(const uint8_t weights[]);
    uint8_t getMothershipLength(const uint8_t weights[]);
    int getNumToTwinkle();
    void setSensitivity(float newSensitivity);
    float getSensitivity();
};

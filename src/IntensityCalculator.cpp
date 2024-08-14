// IntensityCalculator.cpp
#include "IntensityCalculator.h"

IntensityCalculator::IntensityCalculator() : amplitude(600), sensitivity(0.2)
{
    memset(intensity, 0, sizeof(intensity));
    memset(peak, 0, sizeof(peak));
}

void IntensityCalculator::updateIntensity(double vReal[], int samples, float newAmplitude)
{
    amplitude = newAmplitude;

    for (uint8_t band = 0; band < NUM_BANDS; band++)
    {
        peak[band] = 0;
    }

    for (int i = 2; i < (samples / 2); i++)
    {
        if (vReal[i] > 2000)
        {
            uint8_t band = 0;
            if (i <= 2)
                band = 0;
            else if (i <= 5)
                band = 1;
            else if (i <= 7)
                band = 2;
            else if (i <= 15)
                band = 3;
            else if (i <= 30)
                band = 4;
            else if (i <= 53)
                band = 5;
            else if (i <= 106)
                band = 6;
            else
                band = 7;

            int dsize = (int)(vReal[i] * sensitivity / amplitude);
            if (dsize > peak[band])
            {
                peak[band] = dsize;
            }
        }
    }

    for (byte band = 0; band < NUM_BANDS; band++)
    {
        intensity[band] = map(peak[band], 0, amplitude, 0, 64);
    }
}

uint8_t IntensityCalculator::calculateWeightedIntensity(const uint8_t weights[])
{
    uint8_t weightedTotal = 0;
    uint8_t totalWeights = 0;

    for (int i = 0; i < NUM_BANDS; i++)
    {
        weightedTotal += intensity[i] * weights[i];
        totalWeights += weights[i];
    }

    return weightedTotal / totalWeights;
}

uint8_t IntensityCalculator::getWeightedIntensity(const uint8_t weights[])
{
    return calculateWeightedIntensity(weights);
}

uint8_t IntensityCalculator::getMothershipLength(const uint8_t weights[])
{
    uint8_t weightedAverage = calculateWeightedIntensity(weights);
    if (weightedAverage > 12)
    {
        return 10;
    }
    else if (weightedAverage > 7)
    {
        return map(weightedAverage, 7, 12, 2, 7);
    }
    else
    {
        return 1;
    }
}

int IntensityCalculator::getNumToTwinkle()
{
    const uint8_t weights[NUM_BANDS] = {2, 4, 1, 1, 1, 4, 4, 4};
    int weightedIntensity = calculateWeightedIntensity(weights);
    if (weightedIntensity >= 12)
    {
        return 1200;
    }
    else if (weightedIntensity >= 6)
    {
        return map(weightedIntensity, 6, 11, 100, 800);
    }
    else if (weightedIntensity >= 3)
    {
        return map(weightedIntensity, 3, 5, 5, 20);
    }
    else
    {
        return 1;
    }
}
void IntensityCalculator::setSensitivity(float newSensitivity)
{
    sensitivity = constrain(newSensitivity, 0.1, 10.0); // Limit sensitivity range
}

float IntensityCalculator::getSensitivity()
{
    return sensitivity;
}
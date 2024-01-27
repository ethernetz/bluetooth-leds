#include <Arduino.h>
#include <FastLED.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include "BluetoothA2DPSink.h"
#include <arduinoFFT.h>

// LED configuration
#define LED_PIN 27
#define NUM_LEDS 300
#define STARTING_BRIGHTNESS 20
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// Bluetooth service and characteristic UUIDs
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define HUE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BRIGHTNESS_CHARACTERISTIC_UUID "04cc261b-5870-4abc-9f08-ab1ab4b90d6e"
#define MODE_CHARACTERISTIC_UUID "0db05672-2268-4018-9662-255dc67c3473"
#define COLOR_MODE_CHARACTERISTIC_UUID "608cfac4-8f9c-4207-88f5-c4a3aec564cf"
BluetoothA2DPSink a2dp_sink;
BLECharacteristic *pHueCharacteristic;
BLECharacteristic *pBrightnessCharacteristic;
BLECharacteristic *pModeCharacteristic;
BLECharacteristic *pColorModeCharacteristic;

// FFT
#define NUM_BANDS 8
#define SAMPLES 512 // Must be a power of 2
#define SAMPLING_FREQUENCY 44100
arduinoFFT FFT = arduinoFFT();
unsigned int sampling_period_us;
unsigned long microseconds;
double vReal[SAMPLES];
double vImag[SAMPLES];
int32_t peak[] = {0, 0, 0, 0, 0, 0, 0, 0};
int16_t sample_l_int;
int16_t sample_r_int;
float amplitude = 600;
QueueHandle_t queue;
uint8_t intensity[NUM_BANDS];

// Modes
enum Mode
{
  MODE_SOLID,
  MODE_TWINKLE,
  MODE_MOVE,
  // Add more modes here
};
enum ColorMode
{
  MODE_PICK,
  MODE_CYCLE,
  // Add more modes here
};
int globalHueValue = 0;
Mode globalModeValue = MODE_MOVE;
ColorMode colorModeValue = MODE_CYCLE;

void audio_data_callback(const uint8_t *data, uint32_t length)
{
  int item = 0;
  // Only prepare new samples if the queue is empty
  if (uxQueueMessagesWaiting(queue) == 0)
  {
    // log_e("Queue is empty, adding new item");
    int byteOffset = 0;
    for (int i = 0; i < SAMPLES; i++)
    {
      sample_l_int = (int16_t)(((*(data + byteOffset + 1) << 8) | *(data + byteOffset)));
      sample_r_int = (int16_t)(((*(data + byteOffset + 3) << 8) | *(data + byteOffset + 2)));
      vReal[i] = (sample_l_int + sample_r_int) / 2.0f;
      vImag[i] = 0;
      byteOffset = byteOffset + 4;
    }

    // Tell the task in core 1 that the processing can start
    xQueueSend(queue, &item, portMAX_DELAY);
  }
}

void createBands(int i, int dsize)
{
  uint8_t band = 0;
  if (i <= 2)
  {
    band = 0; // 125Hz
  }
  else if (i <= 5)
  {
    band = 1; // 250Hz
  }
  else if (i <= 7)
  {
    band = 2; // 500Hz
  }
  else if (i <= 15)
  {
    band = 3; // 1000Hz
  }
  else if (i <= 30)
  {
    band = 4; // 2000Hz
  }
  else if (i <= 53)
  {
    band = 5; // 4000Hz
  }
  else if (i <= 106)
  {
    band = 6; // 8000Hz
  }
  else
  {
    band = 7;
  }
  int dmax = amplitude;
  if (dsize > dmax)
    dsize = dmax;
  if (dsize > peak[band])
  {
    peak[band] = dsize;
  }
}

void renderFFT(void *parameter)
{
  int item = 0;
  for (;;)
  {
    if (uxQueueMessagesWaiting(queue) > 0)
    {
      FFT.Windowing(vReal, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      FFT.Compute(vReal, vImag, SAMPLES, FFT_FORWARD);
      FFT.ComplexToMagnitude(vReal, vImag, SAMPLES);

      for (uint8_t band = 0; band < NUM_BANDS; band++)
      {
        peak[band] = 0;
      }

      for (int i = 2; i < (SAMPLES / 2); i++)
      {
        if (vReal[i] > 2000)
        {
          createBands(i, (int)vReal[i] / amplitude);
        }
      }

      xQueueReceive(queue, &item, 0);

      for (byte band = 0; band < NUM_BANDS; band++)
      {
        intensity[band] = map(peak[band], 0, amplitude, 0, 64);
        // Serial.printf("%3d | ", intensity[band]); // prints the intensity value with a width of 3
      }

      // Serial.println("");
    }
  }
}

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.println("Device connected");
  }

  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("Device disconnected");
    BLEDevice::startAdvertising();
    Serial.println("Restarted advertising");
  }
};

class HueCharacteristicCallbacks : public BLECharacteristicCallbacks
{
public:
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    // Get the value from the characteristic
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      // Convert the first byte of the value to an int and assign to globalHueValue
      globalHueValue = static_cast<int>(value[0]);

      // Print the new hue value
      Serial.print("New Hue Value: ");
      Serial.println(globalHueValue);
    }
  }
};

class BrightnessCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    // Get the value from the characteristic
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      // Convert the first byte of the value to an int
      int brightnessValue = static_cast<int>(value[0]);

      // Print the new brightness value
      Serial.print("New Brightness Value: ");
      Serial.println(brightnessValue);

      FastLED.setBrightness(brightnessValue);
    }
  }
};

class ModeCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    // Get the value from the characteristic
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      // Convert the first byte of the value to an int and cast to Mode
      Mode newModeValue = static_cast<Mode>(value[0]);

      // If the mode has changed, clear all LEDs
      if (newModeValue != globalModeValue)
      {
        fill_solid(leds, NUM_LEDS, CRGB::Black);
      }

      // Update the global mode value
      globalModeValue = newModeValue;

      // Print the new mode value
      Serial.print("New mode value: ");
      Serial.println(static_cast<int>(globalModeValue));
    }
  }
};

class ColorModeCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    // Get the value from the characteristic
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      // Convert the first byte of the value to an int and cast to Mode
      ColorMode newColorModeValue = static_cast<ColorMode>(value[0]);

      // Update the global mode value
      colorModeValue = newColorModeValue;

      // Print the new mode value
      Serial.print("New color mode value: ");
      Serial.println(static_cast<int>(colorModeValue));
    }
  }
};

// Setup function
void setup()
{
  Serial.begin(115200);
  Serial.println("Elephant on the loose");

  queue = xQueueCreate(1, sizeof(int));
  if (queue == NULL)
  {
    log_i("Error creating the A2DP->FFT queue");
  }

  xTaskCreatePinnedToCore(
      renderFFT,      // Function that should be called
      "FFT Renderer", // Name of the task (for debugging)
      10000,          // Stack size (bytes)
      NULL,           // Parameter to pass
      1,              // Task priority
      NULL,           // Task handle
      1               // Core you want to run the task on (0 or 1)
  );

  // Initialize BLE Device
  BLEDevice::init("Elephant controller");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pHueCharacteristic = pService->createCharacteristic(
      HUE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE_NR);
  pHueCharacteristic->setCallbacks(new HueCharacteristicCallbacks());
  pBrightnessCharacteristic = pService->createCharacteristic(
      BRIGHTNESS_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE_NR);
  pBrightnessCharacteristic->setCallbacks(new BrightnessCharacteristicCallbacks());
  pModeCharacteristic = pService->createCharacteristic(
      MODE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE_NR);
  pModeCharacteristic->setCallbacks(new ModeCharacteristicCallbacks());
  pColorModeCharacteristic = pService->createCharacteristic(
      COLOR_MODE_CHARACTERISTIC_UUID, BLECharacteristic::PROPERTY_WRITE_NR);
  pColorModeCharacteristic->setCallbacks(new ColorModeCharacteristicCallbacks());
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  // Setup a2dp
  sampling_period_us = round(1000000 * (1.0 / SAMPLING_FREQUENCY));
  static const i2s_config_t i2s_config = {
      .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
      .sample_rate = 44100,                         // corrected by info from bluetooth
      .bits_per_sample = (i2s_bits_per_sample_t)16, /* the DAC module will only take the 8bits from MSB */
      .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
      .communication_format = I2S_COMM_FORMAT_I2S_MSB,
      .intr_alloc_flags = 0, // default interrupt priority
      .dma_buf_count = 8,
      .dma_buf_len = 64,
      .use_apll = false};

  a2dp_sink.set_i2s_config(i2s_config);
  a2dp_sink.set_stream_reader(audio_data_callback);
  a2dp_sink.start("Elephant music");

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(STARTING_BRIGHTNESS);
}

// Solid mode function
void solidMode()
{
  fill_solid(leds, NUM_LEDS, CHSV(globalHueValue, 255, 255));
}

uint8_t calculateWeightedIntensity(const uint8_t weights[])
{
  uint8_t weightedTotal = 0;
  uint8_t totalWeights = 0; // To store the sum of all weights

  for (int i = 0; i < NUM_BANDS; i++)
  {
    weightedTotal += intensity[i] * weights[i];
    totalWeights += weights[i]; // Add each weight to the total
  }

  // Calculate the weighted average using the total of weights
  uint8_t weightedAverage = weightedTotal / totalWeights;
  return weightedAverage;
}

// Twinkle mode function
void twinkleMode()
{
  static uint16_t nextLed = 0; // Changed from uint8_t to uint16_t

  int numToTwinkle;
  if (a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED)
  {
    const uint8_t evenWeights[NUM_BANDS] = {2, 4, 1, 1, 1, 4, 4, 4};
    int weightedIntensity = calculateWeightedIntensity(evenWeights);
    numToTwinkle = 1;
  }
  else
  {
    numToTwinkle = 1;
  }

  for (int i = 0; i < numToTwinkle; i++)
  {
    nextLed = random(NUM_LEDS);
    leds[nextLed] = CHSV(globalHueValue, 255, 255);
  }

  // Gradually fade out all LEDs over 2 seconds
  fadeToBlackBy(leds, NUM_LEDS, 2);
}

uint8_t calculateMothershipLength(const uint8_t weights[])
{
  // Calculate the weighted average of the intensity
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

// Move mode function
void moveMode()
{
  static int pos = 0;
  static bool direction = true; // true for forward, false for backward
  int mothershipEvenSideLength;
  int mothershipOddSideLength;
  if (a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED)
  {
    const uint8_t evenWeights[NUM_BANDS] = {2, 4, 1, 1, 1, 4, 4, 4};
    mothershipEvenSideLength = calculateMothershipLength(evenWeights);

    const uint8_t oddWeights[NUM_BANDS] = {4, 1, 1, 4, 1, 1, 4, 4};
    mothershipOddSideLength = calculateMothershipLength(oddWeights);

    if (mothershipEvenSideLength + mothershipOddSideLength >= 14)
    {
      pos += 7;
    }
    fadeToBlackBy(leds, NUM_LEDS, 20);
  }
  else
  {
    mothershipEvenSideLength = 3;
    mothershipOddSideLength = 3;
    fadeToBlackBy(leds, NUM_LEDS, 15);
  }

  // Gradually fade all LEDs

  // Create strings of mothershipLength LEDs every 15 LEDs
  for (int i = pos, count = 0; i < NUM_LEDS; i += 15, count++)
  {
    int mothershipSideLength = (count % 2 == 0) ? mothershipEvenSideLength : mothershipOddSideLength;
    int middle = i; // Corrected to use `i` directly
    for (int j = middle - mothershipSideLength; j <= middle + mothershipSideLength; j++)
    {
      int correctedIndex = j;
      if (correctedIndex < 0)
      {
        correctedIndex += NUM_LEDS; // Adjust for negative indices
      }
      correctedIndex %= NUM_LEDS;
      leds[correctedIndex] = CHSV(globalHueValue, 255, 255);
    }
  }

  // Move the position every second
  EVERY_N_SECONDS(2)
  {
    pos = direction ? (pos + 1) % 15 : (pos - 1 + 15) % 15;
  }

  // Switch direction every 15 seconds
  EVERY_N_SECONDS(15)
  {
    if (a2dp_sink.get_audio_state() == ESP_A2D_AUDIO_STATE_STARTED)
    {
      direction = !direction;
    }
    else
    {
      if (random(4) == 0) // 1/4 chance to switch directions
      {
        direction = !direction;
      }
    }
  }
}

// Main loop
void loop()
{
  switch (globalModeValue)
  {
  case MODE_SOLID:
    solidMode();
    break;
  case MODE_TWINKLE:
    twinkleMode();
    break;
  case MODE_MOVE:
    moveMode();
    break;
    // Add more cases here for additional modes
  }

  EVERY_N_MILLISECONDS(100)
  {
    if (colorModeValue == MODE_CYCLE)
    {
      globalHueValue = (globalHueValue + 1) % 256;
    }
  }

  FastLED.show();
}
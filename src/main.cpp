#include <Arduino.h>
#include <FastLED.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include "BluetoothA2DPSink.h"

// LED configuration
#define LED_PIN 27
#define NUM_LEDS 300
#define STARTING_BRIGHTNESS 5
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

// Bluetooth service and characteristic UUIDs
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define HUE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"
#define BRIGHTNESS_CHARACTERISTIC_UUID "04cc261b-5870-4abc-9f08-ab1ab4b90d6e"
#define MODE_CHARACTERISTIC_UUID "0db05672-2268-4018-9662-255dc67c3473"

// Define modes
enum Mode
{
  MODE_SOLID,
  MODE_TWINKLE,
  MODE_MOVE,
  // Add more modes here
};

// Global variables
BluetoothA2DPSink a2dp_sink;
BLECharacteristic *pHueCharacteristic;
BLECharacteristic *pBrightnessCharacteristic;
BLECharacteristic *pModeCharacteristic;
int globalHueValue = 0;
Mode globalModeValue = MODE_MOVE;

// Callbacks
void data_received_callback()
{
  Serial.println("Data packet received");
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

// Setup function
void setup()
{
  Serial.begin(115200);
  Serial.println("Elephant on the loose");

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
  pService->start();
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06); // functions that help with iPhone connections issue
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  // static const i2s_config_t i2s_config = {
  //     .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX | I2S_MODE_DAC_BUILT_IN),
  //     .sample_rate = 44100,                         // corrected by info from bluetooth
  //     .bits_per_sample = (i2s_bits_per_sample_t)16, /* the DAC module will only take the 8bits from MSB */
  //     .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
  //     .communication_format = I2S_COMM_FORMAT_I2S_MSB,
  //     .intr_alloc_flags = 0, // default interrupt priority
  //     .dma_buf_count = 8,
  //     .dma_buf_len = 64,
  //     .use_apll = false};

  // a2dp_sink.set_i2s_config(i2s_config);
  // a2dp_sink.start("Elephant music");
  // a2dp_sink.set_on_data_received(data_received_callback);

  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(STARTING_BRIGHTNESS);
}

// Solid mode function
void solidMode()
{
  fill_solid(leds, NUM_LEDS, CHSV(globalHueValue, 255, 255));
}

// Twinkle mode function
void twinkleMode()
{
  static uint16_t nextLed = 0; // Changed from uint8_t to uint16_t

  // Every 13.3 milliseconds, turn on a new LED
  EVERY_N_MILLISECONDS(13)
  {
    // Find a random LED that is off
    do
    {
      nextLed = random(NUM_LEDS);
    } while (leds[nextLed] != CRGB::Black);

    // Turn on the LED
    leds[nextLed] = CHSV(globalHueValue, 255, 255);
  }

  // Gradually fade out all LEDs over 2 seconds
  fadeToBlackBy(leds, NUM_LEDS, 2);
}

// Move mode function
void moveMode()
{
  static int pos = 0;

  // Gradually fade all LEDs
  fadeToBlackBy(leds, NUM_LEDS, 10);

  // Create strings of 7 LEDs spaced 8 LEDs apart
  for (int i = pos; i < NUM_LEDS; i += 15)
  {
    for (int j = i; j < i + 7; j++)
    {
      leds[j % NUM_LEDS] = CHSV(globalHueValue, 255, 255);
    }
  }

  // Move the position every second
  EVERY_N_SECONDS(1)
  {
    pos = (pos + 1) % 15; // Move the position
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
  FastLED.show();
}
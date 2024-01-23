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

// Global variables
BluetoothA2DPSink a2dp_sink;
BLECharacteristic *pHueCharacteristic;
BLECharacteristic *pBrightnessCharacteristic;
int globalHueValue = 0;

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

// Main loop
void loop()
{
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CHSV(globalHueValue, 255, 255);
  }
  FastLED.show();
}

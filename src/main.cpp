#include <Arduino.h>
#include "BluetoothA2DPSink.h"

#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>

#include <FastLED.h>

BluetoothA2DPSink a2dp_sink;
BLECharacteristic *pHueCharacteristic;

#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define HUE_CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

#define LED_PIN 27   // Define the pin connected to the data line of the LEDs
#define NUM_LEDS 300 // Number of LEDs in the strip
#define BRIGHTNESS 5 // Set brightness (0-255)
#define LED_TYPE WS2812B
#define COLOR_ORDER GRB

CRGB leds[NUM_LEDS];

void data_received_callback()
{
  Serial.println("Data packet received");
}

class MyServerCallbacks : public BLEServerCallbacks
{
  void onConnect(BLEServer *pServer)
  {
    Serial.println("Device connected");
  };

  void onDisconnect(BLEServer *pServer)
  {
    Serial.println("Device disconnected");
    BLEDevice::startAdvertising(); // Restart advertising after disconnection
    Serial.println("Restarted advertising");
  }
};

int globalHueValue = 0;
class HueCharacteristicCallbacks : public BLECharacteristicCallbacks
{
  // Static variable to store the timestamp of the last callback call
  static unsigned long lastCallbackTime;

  void onWrite(BLECharacteristic *pCharacteristic) override
  {
    unsigned long currentCallbackTime = micros(); // Get current time
    unsigned long executionStartTime = currentCallbackTime;

    // If it's not the first callback, calculate the time since the last callback
    if (lastCallbackTime != 0)
    {
      unsigned long timeSinceLastCallback = currentCallbackTime - lastCallbackTime;
      Serial.print("Time Since Last Callback (microseconds): ");
      Serial.println(timeSinceLastCallback);
    }

    // Get the value from the characteristic
    std::string value = pCharacteristic->getValue();

    if (value.length() > 0)
    {
      // Convert the first byte of the value to an int
      globalHueValue = static_cast<int>(value[0]);
    }

    // Record the execution end time
    unsigned long executionEndTime = micros();
    unsigned long executionTime = executionEndTime - executionStartTime;

    // Print the execution time
    Serial.print("Execution Time (microseconds): ");
    Serial.println(executionTime);

    // Update the timestamp of the last callback call
    lastCallbackTime = executionEndTime;
  }
};

// Initialize the static variable
unsigned long HueCharacteristicCallbacks::lastCallbackTime = 0;

void setup()
{
  Serial.begin(115200);
  Serial.println("Elephant on the loose");

  BLEDevice::init("Elephant controller");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks()); // Register the connection callbacks
  BLEService *pService = pServer->createService(SERVICE_UUID);
  pHueCharacteristic = pService->createCharacteristic(
      HUE_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_WRITE_NR);
  pHueCharacteristic->setCallbacks(new HueCharacteristicCallbacks());

  // Create another characteristic with Write No Response property
  // BLECharacteristic *pWriteNoResponseCharacteristic = pService->createCharacteristic(
  //     NEW_CHARACTERISTIC_UUID,
  //     BLECharacteristic::PROPERTY_WRITE_NR);

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
  FastLED.setBrightness(BRIGHTNESS);
}

void loop()
{
  // Update all LEDs to the current hue value
  for (int i = 0; i < NUM_LEDS; i++)
  {
    leds[i] = CHSV(globalHueValue, 255, 255); // Saturated and bright color
  }

  // Show the LEDs
  FastLED.show();
}

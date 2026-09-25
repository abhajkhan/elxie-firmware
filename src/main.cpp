#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

// Define hardware pins matching your configuration
#define PIN_BUZZER 13
#define PIN_STRIP 10
#define NUM_STRIP_PIXELS 30

Adafruit_NeoPixel testStrip(NUM_STRIP_PIXELS, PIN_STRIP, NEO_GRB + NEO_KHZ800);

void setup()
{
  // Initialize standard hardware CDC USB Serial
  Serial.begin(115200);
  delay(2000);

  Serial.println("=========================================");
  Serial.println("   ELXIE HARDWARE COMMUNICATION TEST    ");
  Serial.println("=========================================");

  // Test the buzzer with a quick initialization beep
  pinMode(PIN_BUZZER, OUTPUT);
  tone(PIN_BUZZER, 2000, 100);

  // Initialize NeoPixel strip
  testStrip.begin();
  testStrip.clear();
  testStrip.show();
  Serial.println("[SUCCESS] Peripherals initialized.");
}

void loop()
{
  Serial.printf("[RUNNING] Uptime: %lu ms | Free Heap: %u bytes\n", millis(), esp_get_free_heap_size());

  // Cycle the strip red
  for (int i = 0; i < NUM_STRIP_PIXELS; i++)
  {
    testStrip.setPixelColor(i, testStrip.Color(50, 0, 0)); // dim red
  }
  testStrip.show();
  delay(1000);

  // Cycle the strip green
  for (int i = 0; i < NUM_STRIP_PIXELS; i++)
  {
    testStrip.setPixelColor(i, testStrip.Color(0, 50, 0)); // dim green
  }
  testStrip.show();
  delay(1000);
}

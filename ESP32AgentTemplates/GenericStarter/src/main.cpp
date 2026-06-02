#include <Arduino.h>

static unsigned long lastPrintMs = 0;
static bool ledState = false;

static void updateBuiltinLed() {
#ifdef LED_BUILTIN
  digitalWrite(LED_BUILTIN, ledState ? HIGH : LOW);
#endif
}

void setup() {
#ifdef LED_BUILTIN
  pinMode(LED_BUILTIN, OUTPUT);
#endif

  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("Whisplay ESP32 Agent generic starter");
  Serial.println("Edit src/main.cpp or switch the PlatformIO board target for your hardware.");
  updateBuiltinLed();
}

void loop() {
  const unsigned long now = millis();
  if (now - lastPrintMs >= 1000) {
    lastPrintMs = now;
    ledState = !ledState;
    updateBuiltinLed();
    Serial.printf("Heartbeat: %lu ms\n", now);
  }
  delay(10);
}

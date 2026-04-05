#include <Arduino.h>
#include "blink.h"

#ifdef CONFIG_IDF_TARGET_ESP32
#define LED_PIN   22
#define LED_LEVEL 0
#elif defined(CONFIG_IDF_TARGET_ESP32S2)
#define LED_PIN   15
#define LED_LEVEL 1
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#define LED_PIN   15
#define LED_LEVEL 1
#elif defined(CONFIG_IDF_TARGET_ESP32C2)
#define LED_PIN   8
#define LED_LEVEL 0
#elif defined(CONFIG_IDF_TARGET_ESP32C3)
#define LED_PIN   8
#define LED_LEVEL 0
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
#define LED_PIN   15
#define LED_LEVEL 0
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
#define LED_PIN   8
#define LED_LEVEL 0
#endif

#define LEDC_BRIGHT   1023

static const char* const MODES[] = {
  "", "OFF", "ON", "TOGGLE", "1HZ", "2HZ", "4HZ", "8HZ", "FADEIN", "FADEOUT", "BREATH"
};

void setup() {
  Serial.begin(115200);

#if MAX_BLINKS > 1
  ESP_ERROR_CHECK(blink_init());
  assert(blink_add(LED_PIN, LED_LEVEL) == 0);
#else
  ESP_ERROR_CHECK(blink_init(LED_PIN, LED_LEVEL));
#endif
}

void loop() {
  static uint8_t mode = BLINK_ON;

  Serial.printf("Blink %s\r\n", MODES[mode]);
#if MAX_BLINKS > 1
  blink_update(0, mode, LEDC_BRIGHT);
#else
  blink_update(mode, LEDC_BRIGHT);
#endif
  if (++mode >= BLINK_MAX)
    mode = BLINK_OFF;
  delay(5000);
}

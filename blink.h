#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <hal/ledc_types.h>
#include <esp_err.h>

//#define MAX_BLINKS  LEDC_CHANNEL_MAX
#define MAX_BLINKS  1

enum { BLINK_UNDEFINED, BLINK_OFF, BLINK_ON, BLINK_TOGGLE, BLINK_1HZ, BLINK_2HZ, BLINK_4HZ, BLINK_8HZ, BLINK_FADEIN, BLINK_FADEOUT, BLINK_BREATH, BLINK_MAX };

#if MAX_BLINKS > 1
esp_err_t blink_init();
int8_t blink_add(uint8_t pin, uint8_t level);
void blink_update(uint8_t index, uint8_t mode, uint16_t bright);
#else
esp_err_t blink_init(uint8_t pin, uint8_t level);
void blink_update(uint8_t mode, uint16_t bright);
#endif

#ifdef __cplusplus
}
#endif

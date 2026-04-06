#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <inttypes.h>
#include <soc/ledc_reg.h>
#include <esp_err.h>

//#define MAX_BLINKS  SOC_LEDC_CHANNEL_NUM
#define MAX_BLINKS  1

enum { BLINK_UNDEFINED, BLINK_OFF, BLINK_ON, BLINK_TOGGLE, BLINK_PULSE1HZ, BLINK_PULSE2HZ, BLINK_PULSE4HZ, BLINK_PULSE8HZ, BLINK_TOGGLE1HZ, BLINK_TOGGLE2HZ, BLINK_TOGGLE4HZ, BLINK_TOGGLE8HZ, BLINK_FADEIN, BLINK_FADEOUT, BLINK_BREATH, BLINK_MAX };

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

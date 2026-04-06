#include <driver/ledc.h>
#include <esp_attr.h>
#include "blink.h"

#if MAX_BLINKS > SOC_LEDC_CHANNEL_NUM
#error "Too many blinks!"
#endif

#define LEDC_TIMER      ((ledc_timer_t)(LEDC_TIMER_MAX - 1))

#define LEDC_MAX_BRIGHT 1023
#define LEDC_PULSE      25 // 25 ms.
#define LEDC_FADETIME   (LEDC_MAX_BRIGHT * 2) // ~2 sec.

typedef struct __attribute__((__packed__)) blink_struct {
  uint16_t mode : 6;
  uint16_t bright : 10;
} blink_t;

#if MAX_BLINKS > 1
static blink_t blinks[MAX_BLINKS] = { 0 };
#else
static blink_t blink = { 0 };
#endif

static inline uint32_t ledc_norm_duty(uint32_t duty) {
  return (duty == LEDC_MAX_BRIGHT) ? (LEDC_MAX_BRIGHT + 1) : duty;
}

static inline void ledc_fade(uint32_t channel, uint32_t start, bool increase, uint16_t steps, uint16_t cycles, uint16_t scale) {
//  ledc_set_fade(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, start, (ledc_duty_direction_t)increase, steps, cycles, scale);
//  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
#if defined(CONFIG_IDF_TARGET_ESP32C2)
  channel *= 0x14;
  WRITE_PERI_REG(LEDC_CH0_DUTY_REG + channel, start << 4);
  WRITE_PERI_REG(LEDC_CH0_CONF1_REG + channel, (1 << LEDC_DUTY_START_CH0_S) | (increase << LEDC_DUTY_INC_CH0_S) | (steps << LEDC_DUTY_NUM_CH0_S) | (cycles << LEDC_DUTY_CYCLE_CH0_S) | (scale << LEDC_DUTY_SCALE_CH0_S));
  SET_PERI_REG_MASK(LEDC_CH0_CONF0_REG + channel, LEDC_PARA_UP_CH0 | LEDC_SIG_OUT_EN_CH0);
#elif defined(CONFIG_IDF_TARGET_ESP32C5)
  WRITE_PERI_REG(LEDC_CH0_DUTY_REG + 0x14 * channel, start << 4);
  WRITE_PERI_REG(LEDC_CH0_CONF1_REG + 0x14 * channel, (1 << LEDC_DUTY_START_CH0_S));
//  *(volatile uint32_t*)(LEDC_CH0_GAMMA_RANGE0_REG + 64 * channel) = (steps << LEDC_CH0_GAMMA_RANGE0_DUTY_NUM_S) | (scale << LEDC_CH0_GAMMA_RANGE0_SCALE_S) | (cycles << LEDC_CH0_GAMMA_RANGE0_DUTY_CYCLE_S) | (increase << LEDC_CH0_GAMMA_RANGE0_DUTY_INC_S);
  WRITE_PERI_REG(LEDC_CH0_GAMMA_RANGE0_REG + 64 * channel, (steps << LEDC_CH0_GAMMA_RANGE0_DUTY_NUM_S) | (scale << LEDC_CH0_GAMMA_RANGE0_SCALE_S) | (cycles << LEDC_CH0_GAMMA_RANGE0_DUTY_CYCLE_S) | (increase << LEDC_CH0_GAMMA_RANGE0_DUTY_INC_S));
  WRITE_PERI_REG(LEDC_CH0_GAMMA_CONF_REG + 0x04 * channel, (1 << LEDC_CH0_GAMMA_ENTRY_NUM_S));
  SET_PERI_REG_MASK(LEDC_CH0_CONF0_REG + 0x14 * channel, LEDC_PARA_UP_CH0 | LEDC_SIG_OUT_EN_CH0);
#elif defined(CONFIG_IDF_TARGET_ESP32C6)
  WRITE_PERI_REG(LEDC_CH0_DUTY_REG + 0x14 * channel, start << 4);
  WRITE_PERI_REG(LEDC_CH0_CONF1_REG + 0x14 * channel, (1 << LEDC_DUTY_START_CH0_S));
  WRITE_PERI_REG(LEDC_CH0_GAMMA_WR_REG + 0x10 * channel, (steps << LEDC_CH0_GAMMA_DUTY_NUM_S) | (scale << LEDC_CH0_GAMMA_SCALE_S) | (cycles << LEDC_CH0_GAMMA_DUTY_CYCLE_S) | (increase << LEDC_CH0_GAMMA_DUTY_INC_S));
  WRITE_PERI_REG(LEDC_CH0_GAMMA_WR_ADDR_REG + 0x10 * channel, 0);
  WRITE_PERI_REG(LEDC_CH0_GAMMA_CONF_REG + 0x04 * channel, (1 << LEDC_CH0_GAMMA_ENTRY_NUM_S));
  SET_PERI_REG_MASK(LEDC_CH0_CONF0_REG + 0x14 * channel, LEDC_PARA_UP_CH0 | LEDC_SIG_OUT_EN_CH0);
#else
  channel *= 0x14;
  WRITE_PERI_REG(LEDC_LSCH0_DUTY_REG + channel, start << 4);
  WRITE_PERI_REG(LEDC_LSCH0_CONF1_REG + channel, (1 << LEDC_DUTY_START_LSCH0_S) | (increase << LEDC_DUTY_INC_LSCH0_S) | (steps << LEDC_DUTY_NUM_LSCH0_S) | (cycles << LEDC_DUTY_CYCLE_LSCH0_S) | (scale << LEDC_DUTY_SCALE_LSCH0_S));
  SET_PERI_REG_MASK(LEDC_LSCH0_CONF0_REG + channel, LEDC_PARA_UP_LSCH0 | LEDC_SIG_OUT_EN_LSCH0);
#endif
}

static inline void ledc_write(uint32_t channel, uint32_t duty) {
//  ledc_set_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel, ledc_norm_duty(duty));
//  ledc_update_duty(LEDC_LOW_SPEED_MODE, (ledc_channel_t)channel);
  ledc_fade(channel, ledc_norm_duty(duty), true, 1, 1, 0);
}

#if MAX_BLINKS > 1
static void IRAM_ATTR ledc_isr(void *arg) {
  uint32_t st = READ_PERI_REG(LEDC_INT_ST_REG);
  blink_t *blinks = (blink_t*)arg;

  for (uint8_t channel = 0; channel < MAX_BLINKS; ++channel) {
#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6)
    if ((st & (1 << (LEDC_DUTY_CHNG_END_CH0_INT_ST_S + channel))) && (blinks[channel].mode != BLINK_UNDEFINED)) {
      uint32_t duty = READ_PERI_REG(LEDC_CH0_DUTY_R_REG + 0x14 * channel) >> 4;
#else
    if ((st & (1 << (LEDC_DUTY_CHNG_END_LSCH0_INT_ST_S + channel))) && (blinks[channel].mode != BLINK_UNDEFINED)) {
      uint32_t duty = READ_PERI_REG(LEDC_LSCH0_DUTY_R_REG + 0x14 * channel) >> 4;
#endif

      if (blinks[channel].mode == BLINK_ON) {
        if (duty != blinks[channel].bright)
          ledc_write(channel, blinks[channel].bright);
      } else if (blinks[channel].mode >= BLINK_PULSE1HZ) {
        if (blinks[channel].mode <= BLINK_PULSE8HZ) { // BLINK_PULSExHZ
          if (duty)
            ledc_fade(channel, blinks[channel].bright, false, 1, LEDC_PULSE, blinks[channel].bright);
          else
            ledc_fade(channel, 0, true, 1, 1000 / (1 << (blinks[channel].mode - BLINK_PULSE1HZ)) - LEDC_PULSE, blinks[channel].bright);
        } else if (blinks[channel].mode <= BLINK_TOGGLE8HZ) { // BLINK_TOGGLExHZ
          if (duty)
            ledc_fade(channel, blinks[channel].bright, false, 1, 500 / (1 << (blinks[channel].mode - BLINK_TOGGLE1HZ)), blinks[channel].bright);
          else
            ledc_fade(channel, 0, true, 1, 500 / (1 << (blinks[channel].mode - BLINK_TOGGLE1HZ)), blinks[channel].bright);
        } else if (blinks[channel].mode == BLINK_FADEIN) {
          ledc_fade(channel, 0, true, blinks[channel].bright, LEDC_FADETIME / blinks[channel].bright, 1);
        } else if (blinks[channel].mode == BLINK_FADEOUT) {
          ledc_fade(channel, blinks[channel].bright, false, blinks[channel].bright, LEDC_FADETIME / blinks[channel].bright, 1);
        } else if (blinks[channel].mode == BLINK_BREATH) {
          if (duty)
            ledc_fade(channel, blinks[channel].bright, false, blinks[channel].bright, LEDC_FADETIME / blinks[channel].bright, 1);
          else
            ledc_fade(channel, 0, true, blinks[channel].bright, LEDC_FADETIME / blinks[channel].bright, 1);
        }
      }
    }
  }
  WRITE_PERI_REG(LEDC_INT_CLR_REG, st);
}

esp_err_t blink_init() {
  const ledc_timer_config_t ledc_timer_cfg = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_10_BIT,
    .timer_num = LEDC_TIMER,
    .freq_hz = 1000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  esp_err_t result;

  result = ledc_timer_config(&ledc_timer_cfg);
  if (result == ESP_OK) {
    result = ledc_isr_register(&ledc_isr, (void*)blinks, ESP_INTR_FLAG_IRAM, NULL);
/*
    if (result == ESP_OK) {
      for (uint8_t i = 0; i < MAX_BLINKS; ++i) {
        blinks[i].mode = BLINK_UNDEFINED;
      }
    }
*/
  }
  return result;
}

int8_t blink_add(uint8_t pin, uint8_t level) {
  uint8_t result;

  for (result = 0; result < MAX_BLINKS; ++result) {
    if (blinks[result].mode == BLINK_UNDEFINED)
      break;
  }
  if (result < MAX_BLINKS) {
    ledc_channel_config_t ledc_channel_cfg = {
      .gpio_num = pin,
      .speed_mode = LEDC_LOW_SPEED_MODE,
      .channel = (ledc_channel_t)result,
      .intr_type = LEDC_INTR_FADE_END,
      .timer_sel = LEDC_TIMER,
      .duty = 0,
      .hpoint = 0,
      .flags = {
        .output_invert = ! level
      }
    };

    if (ledc_channel_config(&ledc_channel_cfg) == ESP_OK) {
      blinks[result].mode = BLINK_OFF;
      return result;
    }
  }
  return -1;
}

void blink_update(uint8_t index, uint8_t mode, uint16_t bright) {
  if ((index < MAX_BLINKS) && (mode > BLINK_UNDEFINED) && (mode < BLINK_MAX) && (blinks[index].mode != BLINK_UNDEFINED)) {
    if (mode == BLINK_TOGGLE) {
      if (blinks[index].mode == BLINK_OFF)
        mode = BLINK_ON;
      else
        mode = BLINK_OFF;
    }
    if (bright > LEDC_MAX_BRIGHT)
      bright = LEDC_MAX_BRIGHT;
    blinks[index].mode = mode;
    blinks[index].bright = bright;
    if (mode == BLINK_OFF) {
      ledc_stop(LEDC_LOW_SPEED_MODE, (ledc_channel_t)index, 0);
//      ledc_write(index, 0);
    } else if (mode == BLINK_ON) {
      ledc_write(index, bright);
    } else {
      if ((mode >= BLINK_PULSE1HZ) && (mode <= BLINK_PULSE8HZ)) {
        ledc_fade(index, bright, false, 1, LEDC_PULSE, bright);
      } else if ((mode >= BLINK_TOGGLE1HZ) && (mode <= BLINK_TOGGLE8HZ)) {
        ledc_fade(index, bright, false, 1, 500 / (1 << (mode - BLINK_TOGGLE1HZ)), bright);
      } else if ((mode == BLINK_FADEIN) || (mode == BLINK_BREATH)) {
        ledc_fade(index, 0, true, bright, LEDC_FADETIME / bright, 1);
      } else if (mode == BLINK_FADEOUT) {
        ledc_fade(index, bright, false, bright, LEDC_FADETIME / bright, 1);
      }
    }
  }
}

#else
static void IRAM_ATTR ledc_isr(void *arg) {
  uint32_t st = READ_PERI_REG(LEDC_INT_ST_REG);
  blink_t *blink = (blink_t*)arg;

#if defined(CONFIG_IDF_TARGET_ESP32C2) || defined(CONFIG_IDF_TARGET_ESP32C5) || defined(CONFIG_IDF_TARGET_ESP32C6)
  if ((st & (1 << LEDC_DUTY_CHNG_END_CH0_INT_ST_S)) && (blink->mode != BLINK_UNDEFINED)) {
    uint32_t duty = READ_PERI_REG(LEDC_CH0_DUTY_R_REG) >> 4;
#else
  if ((st & (1 << LEDC_DUTY_CHNG_END_LSCH0_INT_ST_S)) && (blink->mode != BLINK_UNDEFINED)) {
    uint32_t duty = READ_PERI_REG(LEDC_LSCH0_DUTY_R_REG) >> 4;
#endif

    if (blink->mode == BLINK_ON) {
      if (duty != blink->bright)
        ledc_write(0, blink->bright);
    } else if (blink->mode >= BLINK_PULSE1HZ) {
      if (blink->mode <= BLINK_PULSE8HZ) { // BLINK_PULSExHZ
        if (duty)
          ledc_fade(0, blink->bright, false, 1, LEDC_PULSE, blink->bright);
        else
          ledc_fade(0, 0, true, 1, 1000 / (1 << (blink->mode - BLINK_PULSE1HZ)) - LEDC_PULSE, blink->bright);
      } else if (blink->mode <= BLINK_TOGGLE8HZ) { // BLINK_TOGGLExHZ
        if (duty)
          ledc_fade(0, blink->bright, false, 1, 500 / (1 << (blink->mode - BLINK_TOGGLE1HZ)), blink->bright);
        else
          ledc_fade(0, 0, true, 1, 500 / (1 << (blink->mode - BLINK_TOGGLE1HZ)), blink->bright);
      } else if (blink->mode == BLINK_FADEIN) {
        ledc_fade(0, 0, true, blink->bright, LEDC_FADETIME / blink->bright, 1);
      } else if (blink->mode == BLINK_FADEOUT) {
        ledc_fade(0, blink->bright, false, blink->bright, LEDC_FADETIME / blink->bright, 1);
      } else if (blink->mode == BLINK_BREATH) {
        if (duty)
          ledc_fade(0, blink->bright, false, blink->bright, LEDC_FADETIME / blink->bright, 1);
        else
          ledc_fade(0, 0, true, blink->bright, LEDC_FADETIME / blink->bright, 1);
      }
    }
  }
  WRITE_PERI_REG(LEDC_INT_CLR_REG, st);
}

esp_err_t blink_init(uint8_t pin, uint8_t level) {
  const ledc_timer_config_t ledc_timer_cfg = {
    .speed_mode = LEDC_LOW_SPEED_MODE,
    .duty_resolution = LEDC_TIMER_10_BIT,
    .timer_num = LEDC_TIMER,
    .freq_hz = 1000,
    .clk_cfg = LEDC_AUTO_CLK
  };
  esp_err_t result;

  result = ledc_timer_config(&ledc_timer_cfg);
  if (result == ESP_OK) {
    result = ledc_isr_register(&ledc_isr, (void*)&blink, ESP_INTR_FLAG_IRAM, NULL);
    if (result == ESP_OK) {
      ledc_channel_config_t ledc_channel_cfg = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL_0,
        .intr_type = LEDC_INTR_FADE_END,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
        .flags = {
          .output_invert = ! level
        }
      };

      result = ledc_channel_config(&ledc_channel_cfg);
      if (result == ESP_OK) {
        blink.mode = BLINK_OFF;
      }
    }
  }
  return result;
}

void blink_update(uint8_t mode, uint16_t bright) {
  if ((mode > BLINK_UNDEFINED) && (mode < BLINK_MAX) && (blink.mode != BLINK_UNDEFINED)) {
    if (mode == BLINK_TOGGLE) {
      if (blink.mode == BLINK_OFF)
        mode = BLINK_ON;
      else
        mode = BLINK_OFF;
    }
    if (bright > LEDC_MAX_BRIGHT)
      bright = LEDC_MAX_BRIGHT;
    blink.mode = mode;
    blink.bright = bright;
    if (mode == BLINK_OFF) {
      ledc_stop(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_0, 0);
//      ledc_write(0, 0);
    } else if (mode == BLINK_ON) {
      ledc_write(0, bright);
    } else {
      if ((mode >= BLINK_PULSE1HZ) && (mode <= BLINK_PULSE8HZ)) {
        ledc_fade(0, bright, false, 1, LEDC_PULSE, bright);
      } else if ((mode >= BLINK_TOGGLE1HZ) && (mode <= BLINK_TOGGLE8HZ)) {
        ledc_fade(0, bright, false, 1, 500 / (1 << (mode - BLINK_TOGGLE1HZ)), bright);
      } else if ((mode == BLINK_FADEIN) || (mode == BLINK_BREATH)) {
        ledc_fade(0, 0, true, bright, LEDC_FADETIME / bright, 1);
      } else if (mode == BLINK_FADEOUT) {
        ledc_fade(0, bright, false, bright, LEDC_FADETIME / bright, 1);
      }
    }
  }
}
#endif

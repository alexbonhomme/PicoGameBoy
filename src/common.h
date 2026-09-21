#pragma once

#include <Arduino.h>
#if ENABLE_LCD
#include <TFT_eSPI.h>
#endif
#include <SdFat.h>
#include "gb.h"

#define INPUT_GPIO 1
#define INPUT_PCF8574 2
#define ENABLE_INPUT 1
//#define ENABLE_EXT_PSRAM 1


/* Joypad Pins. */
//#define USE_JOYPAD_I2C_IO_EXPANDER
#if ENABLE_INPUT == INPUT_PCF8574
// Use PCF8574 for Joypad. Only required if an LCD with 16-bit parallel bus is used,
// as Pico does not have enough pins for all peripherals. With 8-bit parallel or SPI LCDs this should not be necessary.
#define PCF8574_ADDR 0x20
#define PCF8574_SDA 20
#define PCF8574_SCL 21
// pins below are on the IO expander
#define PIN_UP		0
#define PIN_DOWN	1 
#define PIN_LEFT	2
#define PIN_RIGHT	3
#define PIN_A		5
#define PIN_B		4
#define PIN_SELECT	6
#define PIN_START	7
#elif ENABLE_INPUT == INPUT_GPIO
// PicoTracker through-hole buttons. Active low, internal pull-ups.
// SW5 RT (GP15) is unused.
#define PIN_UP		 11  // SW4 UP
#define PIN_DOWN	  9  // SW2 DOWN
#define PIN_LEFT	  8  // SW1 LEFT
#define PIN_RIGHT	 10  // SW3 RIGHT
#define PIN_A		  14  // SW7 A
#define PIN_B		  13  // SW6 B
#define PIN_SELECT	 12  // SW8 LT
#define PIN_START	 16  // SW9 PLAY
#endif

#if ENABLE_SOUND
// GY-PCM5102: DIN, BCK, LRCK. BCK and LRCK must be consecutive.
#define I2S_DIN_PIN 17
#define I2S_BCLK_LRC_PIN_BASE 18
// LRCK = 19
#endif

#if ENABLE_SDCARD
// The socket is wired for SDIO. SPI mode uses CMD/CLK/D0/D3; D1 and D2 stay pulled up.
#define SD_SPI SPI
#define SD_CS_PIN   7   // SD_D3
#define SD_SCK_PIN  2   // SD_CLK
#define SD_MOSI_PIN 3   // SD_SI / CMD
#define SD_MISO_PIN 4   // SD_D0
#define SD_D1_PIN   5
#define SD_D2_PIN   6

extern uint8_t _FS_start;
extern uint8_t _FS_end;
#define MAX_ROM_SIZE (&_FS_end - &_FS_start)
#define MAX_ROM_SIZE_MB 1024*1024*1.5
#endif

#if ENABLE_EXT_PSRAM
#define PSRAM_SPI SPI1
#define PSRAM_CS_PIN   0
#define PSRAM_SCK_PIN  14
#define PSRAM_MOSI_PIN 15
#define PSRAM_MISO_PIN 12
#endif

// display is rotated, so TFT_WIDTH/HEIGHT cannot be used
#define DISPLAY_WIDTH TFT_HEIGHT
#define DISPLAY_HEIGHT TFT_WIDTH

#define FILES_PER_PAGE 13
#define FONT_HEIGHT 16
#define ERROR_TEXT_OFFSET FONT_HEIGHT
#define FONT_ID 2

enum class ScalingMode {
  NORMAL = 0,
  STRETCH,
  STRETCH_KEEP_ASPECT,
  COUNT
};

extern volatile ScalingMode scalingMode; 

extern uint_fast32_t frames;
extern TFT_eSPI tft;

/* Multicore command structure. */
union core_cmd {
  struct
  {
    /* Does nothing. */
#define CORE_CMD_NOP 0
    /* Set line "data" on the LCD. Pixel data is in pixels_buffer. */
#define CORE_CMD_LCD_LINE 1
    /* Control idle mode on the LCD. Limits colours to 2 bits. */
#define CORE_CMD_IDLE_SET 2
    /* Set a specific pixel. For debugging. */
#define CORE_CMD_SET_PIXEL 3
    uint8_t cmd;
    uint8_t unused1;
    uint8_t unused2;
    uint8_t data;
  };
  uint32_t full;
};

enum GameType {
  GameType_GB = 0,
  GameType_NES
};


void reset(uint32_t sleepMs = 0);

void error(String message);

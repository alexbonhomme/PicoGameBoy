#include "gbinput.h"

#include "lcd_core.h"

#define PALETTE_COUNT 15

GBInput::GBInput() {
}
void GBInput::afterHandleJoypadCallback() {
  gb.direct.joypad_bits.up = !PRESSED_KEY(ButtonID::BTN_UP);
  gb.direct.joypad_bits.down = !PRESSED_KEY(ButtonID::BTN_DOWN);
  gb.direct.joypad_bits.left = !PRESSED_KEY(ButtonID::BTN_LEFT);
  gb.direct.joypad_bits.right = !PRESSED_KEY(ButtonID::BTN_RIGHT);
  gb.direct.joypad_bits.a = !PRESSED_KEY(ButtonID::BTN_A);
  gb.direct.joypad_bits.b = !PRESSED_KEY(ButtonID::BTN_B);
  gb.direct.joypad_bits.select = !PRESSED_KEY(ButtonID::BTN_SELECT);
  gb.direct.joypad_bits.start = !PRESSED_KEY(ButtonID::BTN_START);

  if (PRESSED_KEY(ButtonID ::BTN_SELECT)) {
    if (srv.inputService.isButtonPressedFirstTime(ButtonID::BTN_START)) {
      gameMenu.openMenu();
    }
    if (srv.inputService.isButtonPressedFirstTime(ButtonID::BTN_B)) {
      /* select + B: change scaling mode */
      scalingMode = (ScalingMode)(((int)scalingMode + 1) % ((int)ScalingMode::COUNT));
      union core_cmd cmd;
      cmd.cmd = CORE_CMD_IDLE_SET;
      multicore_fifo_push_blocking(cmd.full);
      Serial.printf("I Scaling mode: = %d\n", scalingMode);
      delay(100);
    }
  }
}
void GBInput::initJoypad() {
  srv.inputService.setAfterHandleJoypadCallback(
      std::bind(&GBInput::afterHandleJoypadCallback, this));

  gameMenu.setApplyColorSchemeCallback(std::bind(&GBInput::applyColorSchemeCallback, this));
  gameMenu.setSaveRealtimeGameCallback(std::bind(&GBInput::saveRealtimeGameCallback, this));
  gameMenu.setLoadRealtimeGameCallback(std::bind(&GBInput::loadRealtimeGameCallback, this));
  gameMenu.setSaveRamCallback(std::bind(&GBInput::saveRamCallback, this));
  gameMenu.setLoadRamCallback(std::bind(&GBInput::loadRamCallback, this));
  gameMenu.setRestartGameCallback(std::bind(&GBInput::restartGameCallback, this));
}

void GBInput::updatePalette() {
  if (palette_selected == 0) {
    char rom_title[16];
    auto_assign_palette(palette, gb_colour_hash(&gb), gb_get_rom_name(&gb, rom_title));
  } else {
    manual_assign_palette(palette, palette_selected - 1);
  }
}

void GBInput::nextPalette() {
  palette_selected = (palette_selected + 1) % PALETTE_COUNT;
  updatePalette();
}

void GBInput::startEmulator() {
  initJoypad();
  scalingMode = ScalingMode::STRETCH_KEEP_ASPECT;
  Serial.println("Init GB context ...");
  initGbContext();

  /* Automatically assign a colour palette to the game */
  char rom_title[16];
  auto_assign_palette(palette, gb_colour_hash(&gb), gb_get_rom_name(&gb, rom_title));

#if ENABLE_LCD
  gb_init_lcd(&gb, &lcd_draw_line_8bits);

  /* Start Core1, which processes requests to the LCD. */
  Serial.println("Starting Core1 ...");
  multicore_launch_core1(core1_init);
#endif

#if ENABLE_SOUND
  // Initialize audio emulation
  Serial.println("Starting audio ...");
  audio_init();
  srv.soundService.setAudioCallback(audio_callback);
#endif

#if ENABLE_SDCARD
  Serial.println("Load save file ...");
  srv.cardService.read_cart_ram_file(&gb);
#endif
  mainLoop();
}

void GBInput::mainLoop() {
  while (true) {
    gb.gb_frame = 0;

    do {
      //__gb_step_cpu(&gb);
      gb_run_frame(&gb);
      tight_loop_contents();
    } while (HEDLEY_LIKELY(gb.gb_frame == 0));
    frames++;

    handleJoypad();
    handleSerial();

#if ENABLE_SDCARD
    // Pokemon writes the party to cartridge RAM, then stops. Flush once that
    // burst ends so a power cycle still finds the save.
    if (gb_cart_ram_should_flush(100, 3000)) {
      flushCartRam();
    }
#endif

#if ENABLE_SOUND
    srv.soundService.handleSoundLoop();
#endif
  }
}

void GBInput::prevPalette() {
  palette_selected = (palette_selected + PALETTE_COUNT - 1) % PALETTE_COUNT;
  updatePalette();
}

void GBInput::applyColorSchemeCallback() {
  nextPalette();
}
void GBInput::saveRealtimeGameCallback() {
  srv.cardService.save_state(&gb);
}
void GBInput::loadRealtimeGameCallback() {
  srv.cardService.load_state(&gb);
}
bool GBInput::flushCartRam() {
#if ENABLE_SDCARD
  if (srv.cardService.write_cart_ram_file(&gb)) {
    gb_cart_ram_clear_dirty();
    return true;
  }
  gb_cart_ram_note_flush_failed();
#endif
  return false;
}

bool GBInput::saveRamCallback() {
  return flushCartRam();
}
void GBInput::loadRamCallback() {
  srv.cardService.read_cart_ram_file(&gb);
}
void GBInput::restartGameCallback() {
  gb_reset();
}

#pragma once

#define PEANUT_GB_HEADER_ONLY
#include "peanut_gb.h"

#define GBCOLOR_HEADER_ONLY
#include "gbcolors.h"

extern struct gb_s gb;
extern palette_t palette; // Colour palette
extern const uint8_t* RS_rom;

#if ENABLE_RP2040_PSRAM
extern uint8_t* psram_rom;
#endif

#define GB_RAM_SIZE 32768
extern uint8_t RS_ram[GB_RAM_SIZE];

void initGbContext();
void gb_reset();

// Cartridge RAM is battery-backed on a real cart. Mark writes so the SD
// card copy can be updated after the game finishes a save.
void gb_cart_ram_clear_dirty();
void gb_cart_ram_note_flush_failed();
bool gb_cart_ram_should_flush(uint32_t quiet_ms, uint32_t max_delay_ms);

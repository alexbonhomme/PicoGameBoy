#pragma once

#if ENABLE_SDCARD
#include "SdFat.h"
#include "baseservice.h"
#include "gb.h"
#include "hardware/flash.h"

#if ENABLE_EXT_PSRAM
#include "psram.h"
#endif
#define RAM_SAVENAME_LENGTH 16 + 7

#define MAX_PATH_LENGTH 256
// 状态文件头部结构
struct gb_save_state_s {

  /* Cartridge information:
   * Memory Bank Controller (MBC) type. */
  int8_t mbc;
  /* Whether the MBC has internal RAM. */
  uint8_t cart_ram;
  /* Number of ROM banks in cartridge. */
  uint16_t num_rom_banks_mask;
  /* Number of RAM banks in cartridge. Ignore for MBC2. */
  uint8_t num_ram_banks;

  uint16_t selected_rom_bank;
  /* WRAM and VRAM bank selection not available. */
  uint8_t cart_ram_bank;
  uint8_t enable_cart_ram;
  /* Cartridge ROM/RAM mode select. */
  uint8_t cart_mode_select;

  union cart_rtc rtc_latched, rtc_real;

  struct cpu_registers_s cpu_reg;
  struct count_s counter;

  uint8_t wram[WRAM_SIZE];
  uint8_t vram[VRAM_SIZE];
  uint8_t oam[OAM_SIZE];
  uint8_t hram_io[HRAM_IO_SIZE];

#if PEANUT_FULL_GBC_SUPPORT
  /* Game Boy Color Mode*/
  struct {
    uint8_t cgbMode;
    uint8_t doubleSpeed;
    uint8_t doubleSpeedPrep;
    uint8_t wramBank;
    uint16_t wramBankOffset;
    uint8_t vramBank;
    uint16_t vramBankOffset;
    uint16_t fixPalette[0x40]; // BG then OAM palettes fixed for the screen
    uint8_t OAMPalette[0x40];
    uint8_t BGPalette[0x40];
    uint8_t OAMPaletteID;
    uint8_t BGPaletteID;
    uint8_t OAMPaletteInc;
    uint8_t BGPaletteInc;
    uint8_t dmaActive;
    uint8_t dmaMode;
    uint8_t dmaSize;
    uint16_t dmaSource;
    uint16_t dmaDest;
  } cgb;
#endif
};

struct FileListConfig {
  GameType type = GameType_GB;
  const char* dir;
  const char* fileExt[3];
};

class CardService {
public:
  void initSDCard();
  /**
   * set file filters and dirs
   */
  void setConfig(const FileListConfig& config);
  /**
   * The ROM selector displays pages of up to FILES_PER_PAGE rom files
   * allowing the user to select which rom file to start
   * Copy your *.gb rom files to the root directory of the SD card
   */
  void rom_file_selector();

  GameType getSelectedFileType();

  /**
   * Load a save file from the SD card
   */
  void read_cart_ram_file(struct gb_s* gb);

  /**
   * Write cartridge RAM and the MBC3 clock to the SD card.
   * Returns false when the card write fails.
   */
  bool write_cart_ram_file(struct gb_s* gb);
  const char* lastCartSavePath() const { return _lastSavePath; }

  bool onNextPageCallback();
  bool onPrevPageCallback();
  void afterFileSelectedCallback();
  void onSelectKeyPressedCallback();

  void save_state(struct gb_s* gb);
  void load_state(struct gb_s* gb);

  bool readFile(const char* path, const MemoryRegion* regions, size_t region_count);
  bool saveFile(const char* path, const MemoryRegion* regions, size_t region_count);
#if ENABLE_RP2040_PSRAM
  /**
   * load a rom into oboard psram
   */
  void load_cart_rom_file_to_PSRAM(char* filename);
#endif

#if ENABLE_EXT_PSRAM
  /**
   * load a rom into oboard psram and rom
   */
  void load_cart_rom_file_to_rom_and_PSRAM(char* filename);
#endif

#if !ENABLE_RP2040_PSRAM && !ENABLE_EXT_PSRAM
  /**
   * Load a .gb rom file in flash from the SD card
   */
  void load_cart_rom_file(char* filename);
#endif
  bool write_rom_sector_to_flash(FsFile& file, uint8_t* buffer, uint32_t offset);

private:
  bool initSDCard_hardware();
  /**
   * read file names from sd card
   */
  uint16_t read_file_page_from_card(char filename[FILES_PER_PAGE][MAX_PATH_LENGTH], uint16_t num_page);
  /**
   * Function used by the rom file selector to display one page of .gb rom files
   */
  uint16_t rom_file_selector_display_page(uint16_t num_page);

  void close_rom_file(FsFile& file);
  void open_rom_file(FsFile& file, char* filename);

protected:
  SdFs sd;

  uint16_t num_page = 0;
  uint16_t num_files;
  uint16_t total_pages = 1;
  bool is_real_time_savestate_loaded = false;

  FileListConfig _currentConfig;

  FileListConfig _gbConfig;
  FileListConfig _nesConfig;

  char _romFileName[MAX_PATH_LENGTH] = {};
  char _lastSavePath[MAX_PATH_LENGTH] = {};

  bool sav_base_name(struct gb_s* gb, char* name, size_t name_len);
  bool read_cart_file_at(struct gb_s* gb, const char* path, uint32_t save_size);
  bool write_sav_file(const char* path, struct gb_s* gb, uint32_t save_size);
};

#endif
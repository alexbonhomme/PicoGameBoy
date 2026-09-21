#include "tfcardservice.h"

#if ENABLE_SDCARD
#include "allmenus.h"

struct gb_save_state_s gb_realtime_save
#ifdef ENABLE_RP2040_PSRAM
    PSRAM
#endif
    ;

void CardService::setConfig(const FileListConfig& config) {
  _currentConfig = config;
}
void CardService::initSDCard() {
  Serial.println("Initialize SD-Card ...");

  if (!initSDCard_hardware()) {
    tft.setCursor(0, ERROR_TEXT_OFFSET, FONT_ID);
    tft.setTextColor(TFT_RED);
    sd.printSdError(&tft);
    sd.printSdError(&Serial);
    reset(5000);
  }
  if (sd.vol()->fatType() == 0) { // vol() and fatType() do not access the sd-card
    error("Can't find a valid FAT16/FAT32/exFAT partition");
  }

  Serial.printf("SD-Card initialized: FAT-Type=%d\r\n", sd.vol()->fatType());
  sd.mkdir("/SAVES");
  sd.mkdir("/rtsav");

  _gbConfig.type = GameType_GB;
  _gbConfig.dir = "/gb/";
  _gbConfig.fileExt[0] = ".gb";
  _gbConfig.fileExt[1] = ".gbc";
  _gbConfig.fileExt[2] = nullptr; // 标记结束
  _nesConfig.type = GameType_NES;
  _nesConfig.dir = "/nes/";
  _nesConfig.fileExt[0] = ".nes";
  _nesConfig.fileExt[1] = nullptr;
  _nesConfig.fileExt[2] = nullptr; // 标记结束
  _currentConfig = _gbConfig;
}

bool CardService::initSDCard_hardware() {
  // DAT1/DAT2 must stay high so the card accepts SPI mode on an SDIO socket.
  gpio_init(SD_D1_PIN);
  gpio_set_dir(SD_D1_PIN, GPIO_IN);
  gpio_pull_up(SD_D1_PIN);
  gpio_init(SD_D2_PIN);
  gpio_set_dir(SD_D2_PIN, GPIO_IN);
  gpio_pull_up(SD_D2_PIN);

  SD_SPI.setMISO(SD_MISO_PIN);
  SD_SPI.setMOSI(SD_MOSI_PIN);
  SD_SPI.setSCK(SD_SCK_PIN);
  // Display uses PIO, so this SPI port is not shared.
  bool success = sd.begin(SdSpiConfig(SD_CS_PIN, DEDICATED_SPI, SD_SCK_MHZ(50), &SD_SPI));
  return success;
}

uint16_t CardService::read_file_page_from_card(char filename[FILES_PER_PAGE][MAX_PATH_LENGTH], uint16_t num_page) {
  FsFile dir;
  FsFile file;

  /* clear the filenames array */
  for (uint8_t ifile = 0; ifile < FILES_PER_PAGE; ifile++) {
    strcpy(filename[ifile], "");
  }

  if (!dir.open(_currentConfig.dir)) {
    error("Failed to open root dir");
  }

  /* search *.gb files */
  uint16_t num_files = 0;
  uint16_t num_file_offset = num_page * FILES_PER_PAGE;
  char currentFilename[MAX_PATH_LENGTH];
  while (file.openNext(&dir, O_RDONLY)) {
    file.getName(currentFilename, sizeof(currentFilename));

    auto currentFilenameStr = String(currentFilename);
    currentFilenameStr.toLowerCase();
    bool shouldContinue = true;
    for (uint8_t index = 0; index < sizeof(_currentConfig.fileExt); index++) {
      if (_currentConfig.fileExt[index]) {
        if (currentFilenameStr.endsWith(_currentConfig.fileExt[index])) {
          shouldContinue = false;
          break;
        }
      }
    }
    if (shouldContinue) {
      continue;
    }

    if (num_files < num_file_offset) {
      // skip the first N pages
    } else {
      // store the filenames of this page
      strcpy(filename[num_files % FILES_PER_PAGE], currentFilename);
    }

    num_files++;
    file.close();

    if (num_files >= num_file_offset + FILES_PER_PAGE) {
      break;
    }
  }

  dir.close();
  if (num_files == 0) {
    return 0;
  }
  if (num_files % FILES_PER_PAGE == 0) {
    return FILES_PER_PAGE;
  }
  return num_files % FILES_PER_PAGE;
}

uint16_t CardService::rom_file_selector_display_page(uint16_t num_page) {
  char filename[FILES_PER_PAGE][MAX_PATH_LENGTH];
  uint16_t num_files = read_file_page_from_card(filename, num_page);

  for (uint8_t ifile = 0; ifile < num_files; ifile++) {
    fileListMenu.setTextAtIndex(filename[ifile], ifile);
  }
  return num_files;
}

void CardService::close_rom_file(FsFile& file) {
  if (!file.close()) {
    Serial.printf("E f_close error\r\n");
  }
}

void CardService::open_rom_file(FsFile& file, char* filename) {
  if (!file.open(filename, O_RDONLY)) {
    error("Failed to open ROM: " + String(filename));
  }
}

void CardService::rom_file_selector() {
  /* display the first page with up to FILES_PER_PAGE rom files */
  num_files = rom_file_selector_display_page(num_page);

  fileListMenu.setOnNextPageCallback(std::bind(&CardService::onNextPageCallback, this));
  fileListMenu.setOnPrevPageCallback(std::bind(&CardService::onPrevPageCallback, this));
  fileListMenu.setAfterFileSelectedCallback(std::bind(&CardService::afterFileSelectedCallback, this));
  fileListMenu.setOnSelectKeyPressedCallback(std::bind(&CardService::onSelectKeyPressedCallback, this));
  fileListMenu.openMenu();
}

GameType CardService::getSelectedFileType() {
  return _currentConfig.type;
}

bool CardService::readFile(const char* path, const MemoryRegion* regions, size_t region_count) {
  FsFile file;
  if (!file.open(path, O_RDONLY)) {
    Serial.printf("E f_open(%s) error\r\n", path);
    return false;
  } else {
    // 迭代 MemoryRegion 数组，逐个写入每个区域
    for (size_t i = 0; i < region_count; i++) {
      file.read(regions[i].address, regions[i].size);
    }
  }
  return true;
}

bool CardService::saveFile(const char* path, const MemoryRegion* regions, size_t region_count) {
  FsFile file;
  if (!file.open(path, O_WRONLY | O_CREAT)) {
    Serial.printf("E saveFile(%s) error\r\n", path);
    return false;
  }

  // 迭代 MemoryRegion 数组，逐个写入每个区域
  for (size_t i = 0; i < region_count; i++) {
    file.write(regions[i].address, regions[i].size);
  }

  if (!file.close()) {
    Serial.printf("E saveFile f_close error\r\n");
  }
  return true;
}

void CardService::load_state(gb_s* gb) {
  Serial.println("I load_state ...");
  if (!is_real_time_savestate_loaded) {
#ifdef ENABLE_SDCARD
    Serial.println("I no savestate in ram, try loading file ...");
    char filename[RAM_SAVENAME_LENGTH];
    FsFile file;

    gb_get_rom_name(gb, filename);

    String f = String("rtsav/");
    f.concat(filename);
    f.toCharArray(filename, RAM_SAVENAME_LENGTH);
    if (!file.open(filename, O_RDONLY)) {
      Serial.printf("E f_open(%s) error\r\n", filename);
      return;
    } else {

      file.read((uint8_t*)&gb_realtime_save, sizeof(gb_realtime_save));
      is_real_time_savestate_loaded = true;

      if (!file.close()) {
        Serial.printf("E f_close error\r\n");
      }
    }
#else
    Serial.println("I no savestate in ram");
    return;
#endif
  }
  gb->mbc = gb_realtime_save.mbc;
  gb->cart_ram = gb_realtime_save.cart_ram;
  gb->num_rom_banks_mask = gb_realtime_save.num_rom_banks_mask;
  gb->num_ram_banks = gb_realtime_save.num_ram_banks;
  gb->selected_rom_bank = gb_realtime_save.selected_rom_bank;
  gb->cart_ram_bank = gb_realtime_save.cart_ram_bank;
  gb->enable_cart_ram = gb_realtime_save.enable_cart_ram;
  gb->cart_mode_select = gb_realtime_save.cart_mode_select;
  gb->rtc_latched = gb_realtime_save.rtc_latched;
  gb->rtc_real = gb_realtime_save.rtc_real;
  gb->cpu_reg = gb_realtime_save.cpu_reg;

  gb->counter = gb_realtime_save.counter;
  memcpy(gb->wram, gb_realtime_save.wram, WRAM_SIZE);
  memcpy(gb->vram, gb_realtime_save.vram, VRAM_SIZE);
  memcpy(gb->oam, gb_realtime_save.oam, OAM_SIZE);
  memcpy(gb->hram_io, gb_realtime_save.hram_io, HRAM_IO_SIZE);
#if PEANUT_FULL_GBC_SUPPORT
  gb->cgb.cgbMode = gb_realtime_save.cgb.cgbMode;
  gb->cgb.doubleSpeed = gb_realtime_save.cgb.doubleSpeed;
  gb->cgb.doubleSpeedPrep = gb_realtime_save.cgb.doubleSpeedPrep;
  gb->cgb.wramBank = gb_realtime_save.cgb.wramBank;
  gb->cgb.wramBankOffset = gb_realtime_save.cgb.wramBankOffset;
  gb->cgb.vramBank = gb_realtime_save.cgb.vramBank;
  gb->cgb.vramBankOffset = gb_realtime_save.cgb.vramBankOffset;
  gb->cgb.OAMPaletteID = gb_realtime_save.cgb.OAMPaletteID;
  gb->cgb.BGPaletteID = gb_realtime_save.cgb.BGPaletteID;
  gb->cgb.OAMPaletteInc = gb_realtime_save.cgb.OAMPaletteInc;
  gb->cgb.BGPaletteInc = gb_realtime_save.cgb.BGPaletteInc;
  gb->cgb.dmaActive = gb_realtime_save.cgb.dmaActive;
  gb->cgb.dmaMode = gb_realtime_save.cgb.dmaMode;
  gb->cgb.dmaSize = gb_realtime_save.cgb.dmaSize;
  gb->cgb.dmaSource = gb_realtime_save.cgb.dmaSource;
  gb->cgb.dmaDest = gb_realtime_save.cgb.dmaDest;
  memcpy(gb->cgb.fixPalette, gb_realtime_save.cgb.fixPalette, sizeof(gb_realtime_save.cgb.fixPalette));
  memcpy(gb->cgb.OAMPalette, gb_realtime_save.cgb.OAMPalette, sizeof(gb_realtime_save.cgb.OAMPalette));
  memcpy(gb->cgb.BGPalette, gb_realtime_save.cgb.BGPalette, sizeof(gb_realtime_save.cgb.BGPalette));

#endif
  Serial.println("I load_state loaded");
}

#if ENABLE_RP2040_PSRAM
void CardService::load_cart_rom_file_to_PSRAM(char* filename) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, FONT_ID);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Loading ROM: ");

  uint8_t buffer[FLASH_SECTOR_SIZE];

  FsFile file;
  Serial.printf("psram: opening file '%s'\r\n", filename);
  open_rom_file(file, filename);

  uint32_t fileSize = file.size();
  Serial.printf("psram: file opened, size = %lu\r\n", fileSize);

  uint32_t offset = 0;

  if (rp2040.getPSRAMSize() == 0) {
    Serial.println("PSRAM not found or not initialized! Check platformio.ini board settings.");
  }

  psram_rom = (uint8_t*)pmalloc(fileSize);
  while (true) {
    tft.print("#");
    if (offset >= fileSize) {
      Serial.println("\nReached end of file");
      break;
    }

    int nread = file.read(buffer, FLASH_SECTOR_SIZE);
    if (nread < 0) {
      error("Failed to read file!");
    }
    if (nread == 0) {
      break;
    }

    memcpy(psram_rom + offset, buffer, (size_t)nread);

    uint8_t retry = 0;
    while (0 != memcmp(psram_rom + offset, buffer, (size_t)nread)) {
      retry++;
      if (retry >= 5) {
        Serial.printf("E copy psram error @ %07x\r\n", offset);

        for (int i = 0; i < nread; i++) {
          if (i % 16 == 0) {
            Serial.printf("\r\n%07x ", (offset + i));
          }
          Serial.printf("%02x", buffer[i]);
          Serial.printf(buffer[i] == psram_rom[offset + i] ? " " : "-");
          Serial.printf("%02x   ", psram_rom[offset + i]);
        }

        error("E copy psram error");
      }
      Serial.printf("I redo copy psram @ %07x\r\n", offset);
      memcpy(psram_rom + offset, buffer, (size_t)nread);
    }

    /* Next sector */
    offset += FLASH_SECTOR_SIZE;
  }

  Serial.printf("I load_cart_rom_file_to_PSRAM(%s) COMPLETE\r\n", filename);
  close_rom_file(file);
}
#endif

#if ENABLE_EXT_PSRAM
void CardService::load_cart_rom_file_to_rom_and_PSRAM(char* filename) {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, FONT_ID);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Loading ROM: ");

  uint8_t buffer[FLASH_SECTOR_SIZE];

  FsFile file;
  Serial.printf("psram: opening file '%s'\r\n", filename);
  open_rom_file(file, filename);

  uint32_t fileSize = file.size();
  Serial.printf("psram: file opened, size = %lu\r\n", fileSize);

  Serial.printf("I Program target region...\r\n");

  uint32_t offset = 0;
  bool in_psram_section = false;

  while (true) {
    tft.print("#");

    // Check if we've crossed into PSRAM section
    if (!in_psram_section && offset >= MAX_ROM_SIZE_MB) {
      Serial.println("\nSwitching to PSRAM section");
      in_psram_section = true;
    }
    if (offset >= fileSize) {
      Serial.println("\nReached end of file");
      break;
    }

    // write to flash
    if (!in_psram_section) {
      // Use the working write_rom_sector_to_flash function
      if (!write_rom_sector_to_flash(file, buffer, offset)) {
        error("Flash programming failed");
      }
    } else {
      // write to PSRAM
      if (!psram_init()) {
        error("PSRAM init failed");
      } else {
        int nread = file.read(buffer, FLASH_SECTOR_SIZE);
        if (nread < 0) {
          error("Failed to read file!");
        }
        if (nread == 0) {
          break;
        }

        if (!psram_write(offset, buffer, (size_t)nread)) {
          Serial.println("psram: psram_write failed");
          return;
        }
      }
    }
    /* Next sector */
    offset += FLASH_SECTOR_SIZE;
  }

  Serial.printf("I load_cart_rom_file(%s) COMPLETE\r\n", filename);
  close_rom_file(file);
}
#endif

#if !ENABLE_RP2040_PSRAM && !ENABLE_EXT_PSRAM
void CardService::load_cart_rom_file(char* filename) {
  uint8_t buffer[FLASH_SECTOR_SIZE];

  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0, FONT_ID);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.println("Loading ROM: ");

  FsFile file;
  open_rom_file(file, filename);

  Serial.printf("I Program target region...\r\n");

  uint32_t offset = 0;
  while (write_rom_sector_to_flash(file, buffer, offset)) {
    tft.print("#");

    /* Next sector */
    offset += FLASH_SECTOR_SIZE;
    if (offset >= MAX_ROM_SIZE) {
      Serial.printf("I MAX_ROM_SIZE = (%d)\r\n", MAX_ROM_SIZE);
      Serial.printf("I offset = (%d)\r\n", offset);
      break;
    }
  }

  close_rom_file(file);

  Serial.printf("I load_cart_rom_file(%s) COMPLETE\r\n", filename);
}

#endif

bool CardService::write_rom_sector_to_flash(FsFile& file, uint8_t* buffer, uint32_t offset) {
  int nread = file.read(buffer, FLASH_SECTOR_SIZE);
  if (nread < 0) {
    error("Failed to read file!");
  }

  if (nread == 0) {
    return false;
  }

  uint32_t flash_offset = ((uint32_t)&RS_rom[offset]) - XIP_BASE;

  uint32_t ints = save_and_disable_interrupts();
  flash_range_erase(flash_offset, FLASH_SECTOR_SIZE);
  flash_range_program(flash_offset, buffer, FLASH_SECTOR_SIZE);
  restore_interrupts(ints);

  /* Read back target region and check programming */
  if (memcmp(&RS_rom[offset], buffer, FLASH_SECTOR_SIZE) != 0) {
    //@REEMscope.close();
    error("Programming failed - Flash mismatch");
  }

  return true;
}
void CardService::save_state(gb_s* gb) {
  Serial.println("I save_state ...");
  gb_realtime_save.mbc = gb->mbc;
  gb_realtime_save.cart_ram = gb->cart_ram;
  gb_realtime_save.num_rom_banks_mask = gb->num_rom_banks_mask;
  gb_realtime_save.num_ram_banks = gb->num_ram_banks;
  gb_realtime_save.selected_rom_bank = gb->selected_rom_bank;
  gb_realtime_save.cart_ram_bank = gb->cart_ram_bank;
  gb_realtime_save.enable_cart_ram = gb->enable_cart_ram;
  gb_realtime_save.cart_mode_select = gb->cart_mode_select;
  gb_realtime_save.rtc_latched = gb->rtc_latched;
  gb_realtime_save.rtc_real = gb->rtc_real;
  gb_realtime_save.cpu_reg = gb->cpu_reg;

  gb_realtime_save.counter = gb->counter;
  memcpy(gb_realtime_save.wram, gb->wram, WRAM_SIZE);
  memcpy(gb_realtime_save.vram, gb->vram, VRAM_SIZE);
  memcpy(gb_realtime_save.oam, gb->oam, OAM_SIZE);
  memcpy(gb_realtime_save.hram_io, gb->hram_io, HRAM_IO_SIZE);

#if PEANUT_FULL_GBC_SUPPORT
  gb_realtime_save.cgb.cgbMode = gb->cgb.cgbMode;
  gb_realtime_save.cgb.doubleSpeed = gb->cgb.doubleSpeed;
  gb_realtime_save.cgb.doubleSpeedPrep = gb->cgb.doubleSpeedPrep;
  gb_realtime_save.cgb.wramBank = gb->cgb.wramBank;
  gb_realtime_save.cgb.wramBankOffset = gb->cgb.wramBankOffset;
  gb_realtime_save.cgb.vramBank = gb->cgb.vramBank;
  gb_realtime_save.cgb.vramBankOffset = gb->cgb.vramBankOffset;
  gb_realtime_save.cgb.OAMPaletteID = gb->cgb.OAMPaletteID;
  gb_realtime_save.cgb.BGPaletteID = gb->cgb.BGPaletteID;
  gb_realtime_save.cgb.OAMPaletteInc = gb->cgb.OAMPaletteInc;
  gb_realtime_save.cgb.BGPaletteInc = gb->cgb.BGPaletteInc;
  gb_realtime_save.cgb.dmaActive = gb->cgb.dmaActive;
  gb_realtime_save.cgb.dmaMode = gb->cgb.dmaMode;
  gb_realtime_save.cgb.dmaSize = gb->cgb.dmaSize;
  gb_realtime_save.cgb.dmaSource = gb->cgb.dmaSource;
  gb_realtime_save.cgb.dmaDest = gb->cgb.dmaDest;
  memcpy(gb_realtime_save.cgb.fixPalette, gb->cgb.fixPalette, sizeof(gb->cgb.fixPalette));
  memcpy(gb_realtime_save.cgb.OAMPalette, gb->cgb.OAMPalette, sizeof(gb->cgb.OAMPalette));
  memcpy(gb_realtime_save.cgb.BGPalette, gb->cgb.BGPalette, sizeof(gb->cgb.BGPalette));

#endif

  Serial.println("I save_state done");
  is_real_time_savestate_loaded = true;

  char filename[RAM_SAVENAME_LENGTH];
  uint_fast32_t save_size;
  FsFile file;

  gb_get_rom_name(gb, filename);

  String f = String("rtsav/");
  f.concat(filename);
  f.toCharArray(filename, RAM_SAVENAME_LENGTH);
  Serial.printf("I f_open(%s) \r\n", filename);

  save_size = sizeof(gb_realtime_save);
  if (save_size > 0) {
    if (!file.open(filename, O_WRONLY | O_CREAT)) {
      Serial.printf("E f_open(%s) error\r\n", filename);
      return;
    }

    file.write((uint8_t*)&gb_realtime_save, sizeof(gb_realtime_save));

    if (!file.close()) {
      Serial.printf("E f_close error\r\n");
    }
    Serial.println("I save_state written");
  }
}

bool CardService::onPrevPageCallback() {
  if (num_page <= 0) {
    return false;
  }
  fileListMenu.clearMenu();
  /* select the previous page */
  num_page--;
  num_files = rom_file_selector_display_page(num_page);
  return true;
}

bool CardService::onNextPageCallback() {
  /* select the next page */
  if (num_files < FILES_PER_PAGE) {
    /* no more pages */
    return false;
  }
  fileListMenu.clearMenu();
  num_page++;
  num_files = rom_file_selector_display_page(num_page);
  if (num_files == 0) {
    num_page--;
    num_files = rom_file_selector_display_page(num_page);
  }
  return true;
}
void CardService::afterFileSelectedCallback() {
  /* copy the rom from the SD card to flash or PSRAM and start the game */
  const char* selected = fileListMenu.getSelectedText();
  if (selected) {
    strncpy(_romFileName, selected, sizeof(_romFileName) - 1);
    _romFileName[sizeof(_romFileName) - 1] = '\0';
  }
  char filenameWithPath[MAX_PATH_LENGTH];
  snprintf(filenameWithPath, sizeof(filenameWithPath), "%s%s", _currentConfig.dir, _romFileName);

#if ENABLE_RP2040_PSRAM
  load_cart_rom_file_to_PSRAM(filenameWithPath);
#elif ENABLE_EXT_PSRAM
  load_cart_rom_file_to_rom_and_PSRAM(filenameWithPath);
#else
  load_cart_rom_file(filenameWithPath);
#endif
}

void CardService::onSelectKeyPressedCallback() {
  if (_currentConfig.type == GameType::GameType_GB) {
    _currentConfig = _nesConfig;
  } else {
    _currentConfig = _gbConfig;
  }
  fileListMenu.setGameType(_currentConfig.type);
  num_page = 0;
  total_pages = 1;
  num_files = rom_file_selector_display_page(num_page);
}

// Appended after cartridge RAM so Gold/Silver/Crystal keep the clock across reboot.
static const uint32_t CART_RTC_MAGIC = 0x31525443u;

struct __attribute__((packed)) CartRtcSave {
  uint32_t magic;
  uint8_t rtc_real[5];
  uint8_t rtc_latched[5];
  uint32_t rtc_count;
};

static bool lfn_char_ok(unsigned char c) {
  return c >= 0x20 && c < 0x80 && c != '"' && c != '*' && c != '/' && c != ':' && c != '<' && c != '>' && c != '?' && c != '\\' && c != '|';
}

bool CardService::sav_base_name(gb_s* gb, char* name, size_t name_len) {
  char raw[MAX_PATH_LENGTH];
  raw[0] = '\0';
  if (_romFileName[0] != '\0') {
    strncpy(raw, _romFileName, sizeof(raw) - 1);
    raw[sizeof(raw) - 1] = '\0';
  } else if (gb) {
    gb_get_rom_name(gb, raw);
  }
  if (raw[0] == '\0') {
    return false;
  }

  char* slash = strrchr(raw, '/');
  char* leaf = slash ? slash + 1 : raw;
  char* dot = strrchr(leaf, '.');
  if (dot && dot != leaf) {
    *dot = '\0';
  }

  size_t out = 0;
  for (char* p = leaf; *p && out + 1 < name_len; ++p) {
    unsigned char c = (unsigned char)*p;
    name[out++] = lfn_char_ok(c) ? (char)c : '_';
  }
  name[out] = '\0';
  return out > 0;
}

static uint32_t cart_ram_bytes(gb_s* gb) {
  uint32_t save_size = gb_get_save_size(gb);
  if (save_size > GB_RAM_SIZE) {
    save_size = GB_RAM_SIZE;
  }
  return save_size;
}

bool CardService::read_cart_file_at(gb_s* gb, const char* path, uint32_t save_size) {
  FsFile file;
  if (!file.open(path, O_RDONLY)) {
    return false;
  }

  memset(RS_ram, 0, save_size);
  uint64_t file_size = file.fileSize();
  uint32_t ram_bytes = file_size < save_size ? (uint32_t)file_size : save_size;
  int read_bytes = file.read(RS_ram, ram_bytes);

  if (file_size >= save_size + sizeof(CartRtcSave)) {
    CartRtcSave rtc;
    file.seekSet(save_size);
    if (file.read(&rtc, sizeof(rtc)) == (int)sizeof(rtc) && rtc.magic == CART_RTC_MAGIC) {
      memcpy(gb->rtc_real.bytes, rtc.rtc_real, sizeof(rtc.rtc_real));
      memcpy(gb->rtc_latched.bytes, rtc.rtc_latched, sizeof(rtc.rtc_latched));
      gb->counter.rtc_count = rtc.rtc_count;
    }
  }

  if (!file.close()) {
    Serial.printf("E f_close error\r\n");
  }
  Serial.printf("I read_cart_ram_file(%s) COMPLETE (%d bytes)\r\n", path, read_bytes);
  return read_bytes > 0;
}

void CardService::read_cart_ram_file(gb_s* gb) {
  char base[64];
  char path[MAX_PATH_LENGTH];
  uint32_t save_size = cart_ram_bytes(gb);
  if (save_size == 0 || !sav_base_name(gb, base, sizeof(base))) {
    return;
  }

  snprintf(path, sizeof(path), "/SAVES/%s.sav", base);
  if (read_cart_file_at(gb, path, save_size)) {
    return;
  }

  char title[17];
  gb_get_rom_name(gb, title);
  snprintf(path, sizeof(path), "/SAVES/%s", title);
  if (!read_cart_file_at(gb, path, save_size)) {
    Serial.printf("I no cart ram save for %s\r\n", base);
  }
}

bool CardService::write_sav_file(const char* path, gb_s* gb, uint32_t save_size) {
  FsFile file = sd.open(path, O_WRONLY | O_CREAT | O_TRUNC);
  if (!file) {
    Serial.printf("E f_open(%s) error\r\n", path);
    return false;
  }

  CartRtcSave rtc;
  rtc.magic = CART_RTC_MAGIC;
  memcpy(rtc.rtc_real, gb->rtc_real.bytes, sizeof(rtc.rtc_real));
  memcpy(rtc.rtc_latched, gb->rtc_latched.bytes, sizeof(rtc.rtc_latched));
  rtc.rtc_count = gb->counter.rtc_count;

  size_t wrote_ram = file.write(RS_ram, save_size);
  size_t wrote_rtc = file.write(reinterpret_cast<const uint8_t*>(&rtc), sizeof(rtc));
  bool synced = file.sync();
  bool closed = file.close();
  if (wrote_ram != save_size || wrote_rtc != sizeof(rtc) || !synced || !closed) {
    Serial.printf("E write_sav_file(%s) failed\r\n", path);
    return false;
  }
  Serial.printf("I write_sav_file(%s) COMPLETE (%lu bytes)\r\n", path, save_size);
  return true;
}

bool CardService::write_cart_ram_file(gb_s* gb) {
  char base[64];
  uint32_t save_size = cart_ram_bytes(gb);
  _lastSavePath[0] = '\0';

  if (!sav_base_name(gb, base, sizeof(base))) {
    strncpy(_lastSavePath, "no rom name", sizeof(_lastSavePath) - 1);
    return false;
  }
  if (save_size == 0) {
    strncpy(_lastSavePath, "header has no RAM", sizeof(_lastSavePath) - 1);
    Serial.println("E cart header reports no RAM");
    return false;
  }

  // mkdir returns false when the folder already exists.
  if (!sd.exists("/SAVES")) {
    sd.mkdir("/SAVES");
  }
  snprintf(_lastSavePath, sizeof(_lastSavePath), "/SAVES/%s.sav", base);
  return write_sav_file(_lastSavePath, gb, save_size);
}
#endif
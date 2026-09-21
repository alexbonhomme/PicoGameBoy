#pragma once
#include <stdint.h>
#include "emulator/emulator.h"

class GBInput : public Emulator{
public:
  GBInput();
  void initJoypad() override;
  void nextPalette() override;
  void startEmulator() override;
  void mainLoop() override;

private:
  void afterHandleJoypadCallback() override;
  void updatePalette() override;
  void prevPalette() override;
  
  void applyColorSchemeCallback() override;
  void saveRealtimeGameCallback() override;
  void loadRealtimeGameCallback() override;
  bool saveRamCallback() override;
  void loadRamCallback() override;
  bool flushCartRam();
  void restartGameCallback() override;


protected:
  uint8_t palette_selected = 0;
};

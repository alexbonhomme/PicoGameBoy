#pragma once
#include "allservices.h"
#include "allmenus.h"

class Emulator {
public:
  Emulator();
  virtual void initJoypad();
  void handleJoypad();
  void handleSerial();
  virtual void nextPalette();
  virtual void shutdown();
  virtual void startEmulator();
  virtual void mainLoop();

private:
  virtual void afterHandleJoypadCallback();
  virtual void updatePalette();
  virtual void prevPalette();
  
  virtual void applyColorSchemeCallback();
  virtual void saveRealtimeGameCallback();
  virtual void loadRealtimeGameCallback();
  virtual bool saveRamCallback();
  virtual void loadRamCallback();
  virtual void restartGameCallback();

};
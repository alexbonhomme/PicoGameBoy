#include "emulator.h"

Emulator::Emulator() {
}

void Emulator::initJoypad() {
}

void Emulator::handleJoypad() {
  srv.inputService.handleJoypad();
}

void Emulator::handleSerial() {
  srv.inputService.handleSerial();
}

void Emulator::nextPalette() {
}

void Emulator::shutdown() {
  srv.inputService.unsetAfterHandleJoypadCallback();
  gameMenu.setApplyColorSchemeCallback(nullptr);
  gameMenu.setSaveRealtimeGameCallback(nullptr);
  gameMenu.setLoadRealtimeGameCallback(nullptr);
  gameMenu.setSaveRamCallback(nullptr);
  gameMenu.setLoadRamCallback(nullptr);
  gameMenu.setRestartGameCallback(nullptr);
}

void Emulator::startEmulator() {
}

void Emulator::mainLoop() {
}

void Emulator::afterHandleJoypadCallback() {
}

void Emulator::updatePalette() {
}

void Emulator::prevPalette() {
}

void Emulator::applyColorSchemeCallback() {
}

void Emulator::saveRealtimeGameCallback() {
}

bool Emulator::saveRamCallback() {
  return false;
}

void Emulator::loadRamCallback() {
}

void Emulator::restartGameCallback() {
}

void Emulator::loadRealtimeGameCallback() {
}

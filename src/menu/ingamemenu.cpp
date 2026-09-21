#include "ingamemenu.h"

#include "common.h"

#include <stdint.h>

GameMenu::GameMenu() : Menu() {
  setWidth(DISPLAY_WIDTH);
  setHeight(DISPLAY_HEIGHT);
}

// 菜单项枚举
enum MenuItem {
  MENU_VOLUME = 0,
  MENU_SAVE,
  MENU_LOAD,
  MENU_SAVERAM,
  MENU_LOADRAM,
  MENU_COLOR_SCHEME,
  MENU_BACK_TO_GAME_LIST,
  MENU_RESTARTGAME,
  COUNT // 用于方便计算菜单项数量
};

uint8_t MENU_ITEMS = static_cast<int>(MenuItem::COUNT);
// 显示音量值
void GameMenu::setVolumeItem() {
  char vol_text[20];
  uint8_t volume = srv.soundService.getVolume();
  if (volume == 16) {
    snprintf(vol_text, sizeof(vol_text), " Volume:    OFF ");
  } else {
    snprintf(vol_text, sizeof(vol_text), " Volume: %2d/16 ", 16 - volume);
  }
  setTextAtIndex(vol_text, MENU_VOLUME);
}

// 应用配色方案
void GameMenu::applyColorScheme() {
  if (_applyColorSchemeCallback) {
    _applyColorSchemeCallback();
  }
}

// 处理菜单项选择
void GameMenu::handleMenuSelection() {
  switch (currentMenuSelection) {
  case MENU_COLOR_SCHEME:
    applyColorScheme();
    break;

  case MENU_SAVE:
    // 执行保存游戏操作
    saveRealtimeGame();
    break;

  case MENU_LOAD:
    // 执行读取游戏操作
    loadRealtimeGame();
    break;

  case MENU_SAVERAM:
    saveRam();
    break;

  case MENU_LOADRAM:
    loadRam();
    break;

  case MENU_BACK_TO_GAME_LIST:
    // 系统重启
    rebootSystem();
    break;

  case MENU_RESTARTGAME:
    // 系统重启
    restartGame();
    return;
    break;

  default:
    // 其他项不需要特殊处理
    break;
  }
}

void GameMenu::setApplyColorSchemeCallback(std::function<void()> applyColorSchemeCallback) {
  _applyColorSchemeCallback = applyColorSchemeCallback;
}

void GameMenu::setSaveRealtimeGameCallback(std::function<void()> saveRealtimeGameCallback) {
  _saveRealtimeGameCallback = saveRealtimeGameCallback;
}

void GameMenu::setLoadRealtimeGameCallback(std::function<void()> loadRealtimeGameCallback) {
  _loadRealtimeGameCallback = loadRealtimeGameCallback;
}

void GameMenu::setSaveRamCallback(std::function<bool()> saveRamCallback) {
  _saveRamCallback = saveRamCallback;
}

void GameMenu::setLoadRamCallback(std::function<void()> loadRamCallback) {
  _loadRamCallback = loadRamCallback;
}

void GameMenu::setRestartGameCallback(std::function<void()> restartGameCallback) {
  _restartGameCallback = restartGameCallback;
}

// 保存游戏（需要您实现具体逻辑）
void GameMenu::saveRealtimeGame() {
  // 显示保存提示
  if (_saveRealtimeGameCallback) {
    _saveRealtimeGameCallback();
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.drawString("Game Saved!", _menuX + 45, _menuY + _menuHeight - 50, FONT_ID);
    delay(1000);
  }
}

// 读取游戏（需要您实现具体逻辑）
void GameMenu::loadRealtimeGame() {
  // 显示读取提示
  if (_loadRealtimeGameCallback) {
    _loadRealtimeGameCallback();
    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.drawString("Game Loaded!", _menuX + 45, _menuY + _menuHeight - 50, FONT_ID);
    delay(1000);
  }
}

void GameMenu::saveRam() {
  bool saved = _saveRamCallback && _saveRamCallback();
  tft.setTextColor(saved ? TFT_GREEN : TFT_RED, TFT_DARKGREY);
  tft.drawString(saved ? "RAM Saved!" : "RAM save failed", _menuX + 20, _menuY + _menuHeight - 70, FONT_ID);
#if ENABLE_SDCARD
  tft.setTextColor(TFT_WHITE, TFT_DARKGREY);
  tft.drawString(srv.cardService.lastCartSavePath(), _menuX + 8, _menuY + _menuHeight - 45, FONT_ID);
#endif
  delay(1500);
}
void GameMenu::loadRam() {
  if (_loadRamCallback) {
    _loadRamCallback();

    tft.setTextColor(TFT_GREEN, TFT_DARKGREY);
    tft.drawString("RAM Loaded!", _menuX + 45, _menuY + _menuHeight - 50, FONT_ID);
    delay(1000);
  }
}

// 返回主菜单（需要您实现具体逻辑）
void GameMenu::returnToMainMenu() {
  // closeMenu();
  //  这里添加返回主菜单的代码
}
void GameMenu::restartGame() {
  if (_restartGameCallback) {
    _restartGameCallback();
  }
}

// 系统重启（需要您实现具体逻辑）
void GameMenu::rebootSystem() {
  rp2040.reboot();
}

// 关闭菜单
void GameMenu::onCloseMenu() {
  Menu::onCloseMenu();
}

void GameMenu::openMenu() {
  menuActive = true;
  setTitle("IN GAME MENU");
  setVolumeItem();
  setTextAtIndex(" Save Realtime Game ", MENU_SAVE);
  setTextAtIndex(" Load Realtime Game ", MENU_LOAD);
  setTextAtIndex(" Save RAM ", MENU_SAVERAM);
  setTextAtIndex(" Load RAM ", MENU_LOADRAM);
  setTextAtIndex(" Next Color Palette ", MENU_COLOR_SCHEME);
  setTextAtIndex(" Back to Game List ", MENU_BACK_TO_GAME_LIST);
  setTextAtIndex(" Restart Game ", MENU_RESTARTGAME);

  Menu::openMenu();
}

bool GameMenu::onKeyDown() {
  if (PRESSED_KEY(ButtonID::BTN_SELECT) && PRESSED_KEY(ButtonID::BTN_B)) {
    rp2040.rebootToBootloader();
  }
  // up
  if (PRESSED_KEY(ButtonID::BTN_UP)) {
    currentMenuSelection = (currentMenuSelection == 0) ? MENU_ITEMS - 1 : currentMenuSelection - 1;
    drawMenuItems();
  }
  if (PRESSED_KEY(ButtonID::BTN_DOWN)) {
    currentMenuSelection = (currentMenuSelection == MENU_ITEMS - 1) ? 0 : currentMenuSelection + 1;
    drawMenuItems();
  }
  if (PRESSED_KEY(ButtonID::BTN_LEFT)) {
    if (currentMenuSelection == MENU_VOLUME) {
      srv.soundService.decreaseVolume();
      setVolumeItem();
      drawMenuItem(_lines[MENU_VOLUME], MENU_VOLUME);
    }
  }
  if (PRESSED_KEY(ButtonID::BTN_RIGHT)) {
    if (currentMenuSelection == MENU_VOLUME) {
      srv.soundService.increaseVolume();
      setVolumeItem();
      drawMenuItem(_lines[MENU_VOLUME], MENU_VOLUME);
    }
  }
  if (PRESSED_KEY(ButtonID::BTN_A)) {
    uint8_t item = currentMenuSelection;
    handleMenuSelection();
    if (item == MENU_SAVERAM) {
      return false;
    }
    return true;
  }
  if (PRESSED_KEY(ButtonID::BTN_B)) {
    return true;
  }
  return false;
}

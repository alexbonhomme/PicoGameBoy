#pragma once
#include "menu.h"

#include <stdint.h>

class GameMenu : public Menu {
public:
  GameMenu();
  /**
   * true: break loop
   */
  bool onKeyDown() override;
  void onCloseMenu() override;
  void openMenu() override;
  void handleMenuSelection() override;
  void setApplyColorSchemeCallback(std::function<void()> applyColorSchemeCallback);
  void setSaveRealtimeGameCallback(std::function<void()> saveRealtimeGameCallback);
  void setLoadRealtimeGameCallback(std::function<void()> loadRealtimeGameCallback);
  void setSaveRamCallback(std::function<bool()> saveRamCallback);
  void setLoadRamCallback(std::function<void()> loadRamCallback);
  void setRestartGameCallback(std::function<void()> restartGameCallback);

private:
  // 私有辅助函数
  void applyColorScheme();
  void saveRealtimeGame();
  void loadRealtimeGame();
  void saveRam();
  void loadRam();
  void restartGame();
  void setVolumeItem();
  void returnToMainMenu();
  void rebootSystem();

protected:
  std::function<void()> _applyColorSchemeCallback;
  std::function<void()> _saveRealtimeGameCallback;
  std::function<void()> _loadRealtimeGameCallback;
  std::function<bool()> _saveRamCallback;
  std::function<void()> _loadRamCallback;
  std::function<void()> _restartGameCallback;
};
#pragma once

#include <string>

#include <rex/ui/imgui_dialog.h>
#include "pdz_ammo_visibility.h"
#include "pdz_startup_hint.h"

class PDZOverlayDialog final : public rex::ui::ImGuiDialog {
 public:
  explicit PDZOverlayDialog(rex::ui::ImGuiDrawer* drawer);
  ~PDZOverlayDialog() override;

 protected:
  void OnDraw(ImGuiIO& io) override;
  void OnPrepareFrame(ImGuiIO& io) override;

 private:
  std::string settings_save_message_;
  bool settings_save_failed_ = false;
  bool settings_dirty_ = false;
  double settings_retry_after_ = 0;
  bool show_menu_=false,controller_blocked_=false,focus_menu_=false;
  bool cancel_owned_=false,render_toggle_=false;
  uint16_t previous_pad_buttons_=0;
  int selected_tab_=0,requested_tab_=-1;
  float previous_ui_scale_=0;
  double look_notice_until_=0;
  bool look_notice_enabled_=false;
  double display_notice_until_=0;
  std::string display_notice_;
  bool restart_requested_=false;
  std::string lifecycle_error_;
  void SetControllerBlocked(bool blocked);
  void SaveSettings();
  void DrawLeaveConfirmation();
  pdz::ui::StartupHint startup_hint_;
  pdz::hud::AmmoVisibility ammo_visibility_;
};

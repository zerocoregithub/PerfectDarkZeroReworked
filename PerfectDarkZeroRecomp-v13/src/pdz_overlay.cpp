#include "pdz_overlay.h"
#include "pdz_display_mode.h"
#include "pdz_restart.h"

#include "pdz_qol.h"
#include "pdz_p9p_model.h"
#include "pdz_input.h"
#include "pdz_slider.h"
#include "pdz_shoulder_swap.h"
#include "pdz_settings_file.h"
#include "pdz_setting_value.h"
#include "pdz_settings_layout.h"
#include "pdz_presets.h"
#include "pdz_ui_mouse.h"
#include "pdz_classic_hud.h"
#include "pdz_hud_bridge.h"
#include "pdz_numeric_font.h"
#include "pdz_classic_scope.h"
#include "pdz_classic_aim_math.h"
#include <cstdio>

#include <algorithm>
#include <atomic>
#include <filesystem>
#include <string>
#include <vector>
#include <utility>
#include <iomanip>
#include <sstream>

#include <imgui.h>
#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/input/mnk/mnk_input_driver.h>
#include <rex/input/input_system.h>
#include <rex/runtime.h>
#include <rex/ui/window.h>

bool PDZ_ShouldDrawCustomCrosshair();
REXCVAR_DEFINE_DOUBLE(pdz_overlay_scale,0.,"Display","Overlay scale; zero is automatic").range(0.,2.5);
namespace {
constexpr const char* settings_tabs[]={"Presets","Movement","Weapons","Aiming","HUD","Display","Mouse/keys","Controller","Gyro","Menus","Experimental","System"};
rex::input::InputSystem* OverlayInputSystem(){auto* runtime=rex::Runtime::instance();return runtime?dynamic_cast<rex::input::InputSystem*>(runtime->input_system()):nullptr;}
}

PDZOverlayDialog::PDZOverlayDialog(rex::ui::ImGuiDrawer* drawer)
    : ImGuiDialog(drawer) {}
PDZOverlayDialog::~PDZOverlayDialog(){SetControllerBlocked(false);}
void PDZOverlayDialog::SetControllerBlocked(bool blocked){
  if(controller_blocked_==blocked)return;
  if(blocked){float x=0,y=0;rex::input::mnk::ConsumeRelativeMouseMotion(&x,&y);rex::input::mnk::SetMouseLookActive(false);}
  if(auto* input=OverlayInputSystem()){
    if(blocked)input->AddUIInputBlocker();else input->RemoveUIInputBlocker();
  }
  controller_blocked_=blocked;PDZ_SetOverlayControllerCapture(blocked);
}
void PDZOverlayDialog::OnPrepareFrame(ImGuiIO& io){
  // Rendering owns overlay navigation/capture. Do not turn hidden-overlay draws
  // into another hardware-poll loop: the guest already supplies those snapshots.
  // UI polling continues while the guest is neutralised. Hidden overlays use
  // the physical snapshots produced by normal guest polling, avoiding two
  // concurrent callers mutating InputSystem's device assignment.
  if(controller_blocked_)if(auto* input=OverlayInputSystem()){rex::input::X_INPUT_STATE state{};input->GetStateForUI(0,&state);}
  if(PDZ_ConsumeOverlayToggle()){show_menu_=!show_menu_;focus_menu_=show_menu_;}
  SetControllerBlocked(show_menu_);
  if(show_menu_)pdz::ui_mouse_captured.store(true,std::memory_order_release);
  const auto pad=PDZ_OverlayController();
  const uint16_t pressed=pad.buttons&~previous_pad_buttons_;
  previous_pad_buttons_=pad.buttons;
  cancel_owned_=ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)||PDZ_BindingCaptureActive();
  if(show_menu_&&!cancel_owned_){
    const int delta=((pressed&0x200)!=0)-((pressed&0x100)!=0);
    if(delta){requested_tab_=(selected_tab_+delta+int(std::size(settings_tabs)))%int(std::size(settings_tabs));focus_menu_=true;}
  }
  render_toggle_=PDZ_ConsumeRenderToggle()&&!io.WantTextInput&&!PDZ_BindingCaptureActive();
  if(show_menu_)io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;
  else io.ConfigFlags&=~(ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard);
  pdz::ui::FeedOverlayPad(io,pad,show_menu_);
}

static toml::table CaptureSettings() {
        // Persistence allowlist, not the entire CVar registry. Built-in presets
        // and user slots share this snapshot; input adds its schema separately.
        // Keep this in sync with any new setting and its startup loading policy.
        toml::table values;
        values.insert("fullscreen",rex::cvar::Query<bool>("fullscreen"));
        values.insert("fullscreen_exclusive",rex::cvar::Query<bool>("fullscreen_exclusive"));
        values.insert("resolution",rex::cvar::Query<std::string>("resolution"));
        values.insert("pdz_smaa",rex::cvar::Query<bool>("pdz_smaa"));
        if (rex::cvar::GetFlagInfo("pdz_hdr_output")) {
          values.insert("pdz_hdr_output",rex::cvar::Query<bool>("pdz_hdr_output"));
          for (const char* flag : {"pdz_hdr_paper_white", "pdz_hdr_ui_white"})
            values.insert(flag, rex::cvar::Query<double>(flag));
        }
        if(rex::cvar::GetFlagInfo("pdz_disable_motion_blur"))
          values.insert("pdz_disable_motion_blur",rex::cvar::Query<bool>("pdz_disable_motion_blur"));
        for(auto flag:{"resolution_scale","draw_resolution_scale_x","draw_resolution_scale_y"})values.insert(flag,rex::cvar::Query<int32_t>(flag));
        PDZ_SaveInputSettings(values);
        values.insert_or_assign("pdz_direct_mouse",rex::cvar::Query<bool>("pdz_direct_mouse"));
        values.insert_or_assign("pdz_weapon_bob_amount",rex::cvar::Query<double>("pdz_weapon_bob_amount"));
        values.insert("pdz_stylized_lighting",rex::cvar::Query<bool>("pdz_stylized_lighting"));
        values.insert("pdz_overlay_scale",rex::cvar::Query<double>("pdz_overlay_scale"));
        values.insert("pdz_tp_enable",rex::cvar::Query<bool>("pdz_tp_enable"));
        values.insert("pdz_skip_intro",rex::cvar::Query<bool>("pdz_skip_intro"));
        for(const char* flag:{"pdz_tp_offset_x","pdz_tp_offset_y","pdz_tp_offset_z"})
          values.insert(flag,rex::cvar::Query<double>(flag));
        values.insert("pdz_weapon_accuracy",rex::cvar::Query<bool>("pdz_weapon_accuracy"));
        for (const char* name : {"pdz_classic_scope", "pdz_sway_during_ads", "pdz_free_aim_ads_only", "pdz_auto_aim", "pdz_classic_free_aim", "pdz_classic_sway_aim", "pdz_classic_weapon_sway", "pdz_dpad_zoom", "pdz_host_vsync", "pdz_timing_log",
                                "pdz_scene_only_grading", "pdz_modern_movement", "pdz_classic_movement", "pdz_classic_npc_durability", "pdz_dodge_roll",
                                "pdz_unarmed_run_boost", "pdz_hide_unarmed_hands", "pdz_p9p_hide_attachments", "pdz_hold_y_holster", "pdz_cover_camera_stabilization",
                                "pdz_hide_original_crosshair", "pdz_classic_hud",
                                "pdz_hide_interaction_prompts", "pdz_classic_hud_hide_objective", "pdz_classic_hud_crosshair", "pdz_half_native_hud", "pdz_ammo_auto_hide", "pdz_auto_aim_range_boost", "pdz_reload_interact"})
          values.insert(name, rex::cvar::Query<bool>(name));
        for (const char* name : {"pdz_frame_limit", "pdz_crosshair_red",
                                "pdz_crosshair_green", "pdz_crosshair_blue",
                                "pdz_crosshair_opacity"})
          values.insert(name, rex::cvar::Query<int32_t>(name));
          for (const char* name : {"pdz_cursor_visual_smoothing", "pdz_zoom_transition", "pdz_cursor_sway_deadzone", "pdz_crosshair_edge_deadzone", "pdz_classic_weapon_sway_amount", "pdz_npc_health_scale", "pdz_weapon_action_speed", "pdz_movement_response", "pdz_display_gamma", "pdz_display_contrast",
                                "pdz_display_saturation", "pdz_display_tint",
                                "pdz_horizontal_fov", "pdz_mouse_sensitivity",
                                "pdz_mouse_zoom_compensation",
                                "pdz_mouse_zoom_sensitivity_scale",
                                "pdz_crosshair_size", "pdz_crosshair_gap",
                                "pdz_crosshair_thickness", "pdz_crosshair_outline", "pdz_ammo_hide_delay", "pdz_ammo_hud_scale", "pdz_auto_aim_close_boost"})
          values.insert(name, rex::cvar::Query<double>(name));
        values.insert("pdz_crosshair_style",
                      rex::cvar::Query<std::string>("pdz_crosshair_style"));
        // Shared preset schema keeps persistence in sync with newly added options.
        for(const auto& v:pdz::settings::kPresetValues) {
          const auto* info=rex::cvar::GetFlagInfo(v.name);
          if(!info) continue;
          switch(info->type) {
            case rex::cvar::FlagType::Boolean: values.insert_or_assign(v.name,rex::cvar::Query<bool>(v.name)); break;
            case rex::cvar::FlagType::Int32: values.insert_or_assign(v.name,rex::cvar::Query<int32_t>(v.name)); break;
            case rex::cvar::FlagType::Double: values.insert_or_assign(v.name,rex::cvar::Query<double>(v.name)); break;
            case rex::cvar::FlagType::String: values.insert_or_assign(v.name,rex::cvar::Query<std::string>(v.name)); break;
            default: break;
          }
        }
        values.insert_or_assign("pdz_settings_preset",rex::cvar::Query<std::string>("pdz_settings_preset"));
        return values;
}

// Only known captured options may be restored; validate first and roll back on
// rejection, so corrupt/incompatible slots cannot partially apply a setup.
static bool ApplySettings(const toml::table& values,std::string& error) {
  const auto known=CaptureSettings();
  std::vector<std::pair<std::string,std::string>> changes,previous;
  for(const auto& [key,node]:values) {
    const std::string name(key.str());
    if(name=="pdz_settings_preset"||!known.contains(name)) continue;
    std::string value;
    if(!pdz::settings::RuntimeValue(node,value)) {error="Invalid value for "+name;return false;}
    changes.emplace_back(name,value);
  }
  if(changes.empty()){error="Preset contains no recognized settings.";return false;}
  for(const auto& [name,value]:changes) {
    previous.emplace_back(name,rex::cvar::GetFlagByName(name));
    if(!rex::cvar::SetFlagByName(name,value)) {
      for(const auto& [old,original]:previous) rex::cvar::SetFlagByName(old,original);
      error="Cannot apply "+name+"; prior settings restored.";return false;
    }
  }
  error.clear();return true;
}

void PDZOverlayDialog::SaveSettings() {
        const auto values=CaptureSettings();
        const auto config = pdz::SettingsDirectory(
            rex::filesystem::GetExecutableFolder()) / "enhancements.toml";
        std::string error;
        settings_save_failed_ = !pdz::SaveEnhancementFile(config, values, error);
        settings_save_message_ = settings_save_failed_
            ? "Save failed: " + error : "All changes saved automatically.";
        if (settings_save_failed_) REXLOG_ERROR("PDZ settings save: {}", error);
        else REXLOG_INFO("PDZ enhancements saved to {}", config.string());
  settings_dirty_=settings_save_failed_;
  settings_retry_after_=ImGui::GetTime()+2.0;

}

void PDZOverlayDialog::DrawLeaveConfirmation() {
  if(ImGui::BeginPopupModal("Leave game?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(restart_requested_?"Restart the game?":"Exit the game?");
    ImGui::TextWrapped("Settings will be saved. Unsaved mission progress will be lost.");
    if(ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::SetItemDefaultFocus();
    ImGui::SameLine();
    if(ImGui::Button(restart_requested_?"Confirm Restart":"Confirm Exit")) {
      SaveSettings();
      auto* runtime=rex::Runtime::instance();
      auto* window=runtime?runtime->display_window():nullptr;
      if(settings_save_failed_) lifecycle_error_="Settings could not be saved. Please cancel and retry saving.";
      else if(!window) lifecycle_error_="Game window is unavailable.";
      else if(!restart_requested_||pdz::StartRestartHelper(lifecycle_error_)) {
        ImGui::CloseCurrentPopup();
        window->RequestClose();
      }
    }
    if(!lifecycle_error_.empty()) ImGui::TextWrapped("%s",lifecycle_error_.c_str());
    ImGui::EndPopup();
  }
}

void PDZOverlayDialog::OnDraw(ImGuiIO& io) {
  // Gameplay-facing refreshes currently live here even when F8 is closed.
  // Skipping OnDraw as a UI-only optimization would also skip live FOV updates
  // and the experimental held-model identity refresh. Split ownership first.
  PDZ_UpdateP9PModel();
  PDZ_UpdateLiveFov();

  bool& show_menu=show_menu_;
  bool leave_requested=false;
  if (ImGui::IsKeyPressed(ImGuiKey_F8, false)){show_menu=!show_menu;focus_menu_=show_menu;}
  if(show_menu&&!cancel_owned_&&ImGui::IsKeyPressed(ImGuiKey_GamepadFaceRight,false))show_menu=false;
  SetControllerBlocked(show_menu);
  if(show_menu)pdz::ui_mouse_captured.store(true,std::memory_order_release);
  if(!show_menu && io.MouseWheel!=0) PDZ_QueueZoomWheel(io.MouseWheel);

  auto mark_unsaved = [this]() {
    settings_dirty_=true;
    settings_retry_after_=0;
    settings_save_failed_=false;
    settings_save_message_="Saving automatically when adjustment finishes...";
    rex::cvar::SetFlagByName("pdz_settings_preset","custom");
  };
  auto set_bool = [&mark_unsaved](const char* name, bool value) {
    if (rex::cvar::SetFlagByName(name, value ? "true" : "false")) mark_unsaved();
  };
  auto set_double = [&mark_unsaved](const char* name, float value) {
    if (rex::cvar::SetFlagByName(name, std::to_string(value))) mark_unsaved();
  };
  auto set_int = [&mark_unsaved](const char* name, int value) {
    if (rex::cvar::SetFlagByName(name, std::to_string(value))) mark_unsaved();
  };

  const auto display_mode=[] {return pdz::ui::DisplayMode(rex::cvar::Query<bool>("fullscreen"),rex::cvar::Query<bool>("fullscreen_exclusive"));};
  const auto apply_display_mode=[&](int mode) {
    // Apply exclusive preference before entering fullscreen; upstream callbacks
    // marshal window changes onto the UI thread and restore windowed geometry.
    set_bool("fullscreen_exclusive",mode==2);
    set_bool("fullscreen",mode!=0);
    display_notice_=std::string("Display: ")+pdz::ui::DisplayModeLabel(mode);
    display_notice_until_=ImGui::GetTime()+2.;
  };
  if(!io.WantTextInput&&!ImGui::IsAnyItemActive()&&!PDZ_BindingCaptureActive()&&
     !io.KeyCtrl&&!io.KeyAlt&&!io.KeySuper&&ImGui::IsKeyPressed(ImGuiKey_F11,false))
    apply_display_mode(pdz::ui::NextDisplayMode(display_mode()));

  // No auto-repeat or accidental toggles during typing/bind capture.
  const bool perspective_request=PDZ_ConsumePerspectiveToggle();
  if(perspective_request&&!show_menu&&!PDZ_GameMenuActive()&&!PDZ_LocktopusInputActive())
    set_bool("pdz_tp_enable",!rex::cvar::Query<bool>("pdz_tp_enable"));
  const bool shoulder_request=PDZ_ConsumeShoulderToggle()||
      (!io.WantTextInput&&!io.WantCaptureKeyboard&&!ImGui::IsAnyItemActive()&&
       !PDZ_BindingCaptureActive()&&!io.KeyCtrl&&!io.KeyAlt&&!io.KeySuper&&
       ImGui::IsKeyPressed(ImGuiKey_F6,false));
  if(shoulder_request&&rex::cvar::Query<bool>("pdz_tp_enable")&&!show_menu&&
     !PDZ_GameMenuActive()&&!PDZ_LocktopusInputActive()) {
    const double offset=rex::cvar::Query<double>("pdz_tp_offset_x");
    if(std::isfinite(offset))set_double("pdz_tp_offset_x",float(pdz::input::OppositeShoulder(offset)));
  }
  if(render_toggle_||(!io.WantTextInput && !io.WantCaptureKeyboard && !ImGui::IsAnyItemActive() &&
     !io.KeyCtrl && !io.KeyAlt && !io.KeySuper &&
     ImGui::IsKeyPressed(ImGuiKey_Semicolon,false))) {
    const bool enabled=!rex::cvar::Query<bool>("pdz_stylized_lighting");
    set_bool("pdz_stylized_lighting",enabled);
    look_notice_enabled_=enabled;
    look_notice_until_=ImGui::GetTime()+2.0;
  }

  if (ImGui::GetTime()<std::max(look_notice_until_,display_notice_until_)) {
    const char* label=display_notice_until_>look_notice_until_?display_notice_.c_str():look_notice_enabled_?"GameCube Look: On":"GameCube Look: Off";
    const float scale=pdz::ui::OverlayScale(float(rex::cvar::Query<double>("pdz_overlay_scale")),io.DisplaySize.y);
    const float size=ImGui::GetFontSize()*scale;
    const ImVec2 text=ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,0,label);
    const ImVec2 p((io.DisplaySize.x-text.x)*.5f,24*scale);
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(p.x-14*scale,p.y-8*scale),ImVec2(p.x+text.x+14*scale,p.y+text.y+8*scale),IM_COL32(0,0,0,210),5*scale);
    draw->AddText(ImGui::GetFont(),size,p,IM_COL32(255,255,255,255),label);
  }
  if(startup_hint_.Visible(ImGui::GetTime(),io.DisplaySize.x>0&&io.DisplaySize.y>0)) {
    const char* hint="Settings: F8\nController: View + Menu / Select + Start (press together)";
    const float scale=pdz::ui::OverlayScale(float(rex::cvar::Query<double>("pdz_overlay_scale")),io.DisplaySize.y);
    const float size=ImGui::GetFontSize()*scale;
    const float width=std::max(1.f,io.DisplaySize.x-48.f*scale);
    const auto text=ImGui::GetFont()->CalcTextSizeA(size,FLT_MAX,width,hint);
    const ImVec2 pos((io.DisplaySize.x-text.x)*.5f,std::max(8.f,io.DisplaySize.y-text.y-28.f*scale));
    auto* draw=ImGui::GetForegroundDrawList();
    draw->AddRectFilled(ImVec2(pos.x-12.f*scale,pos.y-8.f*scale),
        ImVec2(pos.x+text.x+12.f*scale,pos.y+text.y+8.f*scale),IM_COL32(0,0,0,210),5.f*scale);
    draw->AddText(ImGui::GetFont(),size,pos,IM_COL32(255,255,255,255),hint,nullptr,width);
  }
  if (show_menu) {
    const float scale=pdz::ui::OverlayScale(float(rex::cvar::Query<double>("pdz_overlay_scale")),io.DisplaySize.y);
    const ImVec2 maximum(std::max(320.f,io.DisplaySize.x-32.f),std::max(240.f,io.DisplaySize.y-32.f));
    const bool resize=std::abs(previous_ui_scale_-scale)>.01f;
    if(resize||focus_menu_)ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x*.5f,io.DisplaySize.y*.5f),ImGuiCond_Always,ImVec2(.5f,.5f));
    ImGui::SetNextWindowSize(ImVec2(std::min(740.f*scale,maximum.x),std::min(640.f*scale,maximum.y)),resize?ImGuiCond_Always:ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSizeConstraints(ImVec2(std::min(600.f*scale,maximum.x),std::min(450.f*scale,maximum.y)),maximum);
    previous_ui_scale_=scale;
    const auto saved_style=ImGui::GetStyle();auto& style=ImGui::GetStyle();style.ScaleAllSizes(scale);
    style.FramePadding.y=8.f*scale;style.ItemSpacing.y=8.f*scale;
    style.Colors[ImGuiCol_NavCursor]=ImVec4(1.f,.85f,.15f,1.f);
    if(focus_menu_)ImGui::SetNextWindowFocus();
    if (ImGui::Begin("Perfect Dark Zero settings##pdz", &show_menu,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse)) {
      ImGui::SetWindowFontScale(scale);
      ImGui::TextUnformatted("F8 / View + Menu: settings");
      ImGui::SameLine();ImGui::TextDisabled("(?)");
      if(ImGui::IsItemHovered())ImGui::SetTooltip("D-pad / left stick: navigate\nBottom face: select or edit; right face: back or close\nLB / RB: change tab\nSliders: select, adjust left/right, select to finish\nSemicolon / Quick Access: GameCube look\nChanges apply live and save automatically.");
      ImGui::SameLine();
      if(ImGui::SmallButton("Exit Game##quick_exit")) {
        restart_requested_=false;lifecycle_error_.clear();leave_requested=true;
      }
      // Keep the save/status footer reachable while each page scrolls independently.
      const auto begin_tab=[&](const char* label) {
        int index=0;while(index<int(std::size(settings_tabs))&&std::string_view(settings_tabs[index])!=label)++index;
        const auto flags=requested_tab_==index?ImGuiTabItemFlags_SetSelected:ImGuiTabItemFlags_None;
        if(!ImGui::BeginTabItem(label,nullptr,flags))return false;
        selected_tab_=index;
        pdz::ui::BeginSettingsPage(label);
        if(focus_menu_&&(requested_tab_<0||requested_tab_==index)){pdz::ui::FocusNavigationHere();focus_menu_=false;}
        ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x*.5f);
        ImGui::PushTextWrapPos(0.f);
        return true;
      };
      const auto end_tab=[]() {
        ImGui::PopTextWrapPos();ImGui::PopItemWidth();ImGui::EndChild();ImGui::EndTabItem();
      };
      if(ImGui::BeginTabBar("pdz_settings_tabs",ImGuiTabBarFlags_FittingPolicyScroll)) {
      if(begin_tab("Presets")) {
        bool skip_intro=rex::cvar::Query<bool>("pdz_skip_intro");
        if(ImGui::Checkbox("Skip startup intro (next launch)",&skip_intro))set_bool("pdz_skip_intro",skip_intro);
        const auto apply_preset=[&](pdz::settings::Preset preset,const char* id) {
          toml::table values;
          for(const auto& value:pdz::settings::kPresetValues) {
            values.insert_or_assign(value.name,pdz::settings::Value(value,preset));
          }
          PDZ_DefaultPresetBindings(values);
          std::string error;
          if(!ApplySettings(values,error)){settings_save_failed_=true;settings_save_message_=error;return;}
          mark_unsaved();
          rex::cvar::SetFlagByName("pdz_settings_preset",id);
        };
        ImGui::TextWrapped("Built-in presets restore modern keyboard/mouse buttons and default controller buttons, including menu navigation. Sensitivity, deadzones, frame cap and VSync are preserved.");
        if(ImGui::Button("Classic Rare Style Perfect Dark")) apply_preset(pdz::settings::Preset::Classic,"classic");
        ImGui::TextWrapped("Your approved PD-style gameplay, HUD and colour settings. GameCube look is available separately under Experimental.");
        if(ImGui::Button("Modernized Perfect Dark Zero")) apply_preset(pdz::settings::Preset::Modernized,"modernized");
        ImGui::TextWrapped("Same as Classic Rare Style, without weapon sway. Uses the game's own aim assist when enabled in-game; overlay aim assist is off.");
        if(ImGui::Button("Vanilla Perfect Dark Zero")) apply_preset(pdz::settings::Preset::Vanilla,"vanilla");
        ImGui::TextWrapped("Original PDZ movement, materials, HUD, weapon timing and Warehouse door timers. Native camera FOV restored. Scoped auto-aim remains permanently disabled.");
        ImGui::SeparatorText("Your presets");
        ImGui::TextWrapped("Save stores your current enhancements, sensitivity and custom bindings in an independent slot. Load restores that slot. Saving over a used slot requires confirmation; its previous contents are backed up.");
        for(int slot=1;slot<=3;++slot) {
          ImGui::PushID(slot);
          const auto path=pdz::SettingsDirectory(rex::filesystem::GetExecutableFolder())/
              ("user-preset-"+std::to_string(slot)+".toml");
          const bool exists=std::filesystem::exists(path);
          const auto save_slot=[&]() {
            std::string error;
            const bool ok=pdz::SaveEnhancementFile(path,CaptureSettings(),error);
            settings_save_failed_=!ok;
            settings_save_message_=ok?"User preset "+std::to_string(slot)+" saved.":"Preset save failed: "+error;
          };
          ImGui::Text("User preset %d%s",slot,exists?"":" (empty)");ImGui::SameLine();
          if(ImGui::Button("Save")){if(exists)ImGui::OpenPopup("Overwrite preset?");else save_slot();}
          ImGui::SameLine();ImGui::BeginDisabled(!exists);
          if(ImGui::Button("Load")) {
            try {
              std::string error;
              if(ApplySettings(toml::parse_file(path.string()),error)) {
                mark_unsaved();rex::cvar::SetFlagByName("pdz_settings_preset","user"+std::to_string(slot));
              } else {settings_save_failed_=true;settings_save_message_=error;}
            } catch(const std::exception& e){settings_save_failed_=true;settings_save_message_="Preset load failed: "+std::string(e.what());}
          }
          ImGui::EndDisabled();
          if(ImGui::BeginPopupModal("Overwrite preset?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::Text("Replace user preset %d with your current settings?",slot);
            if(ImGui::Button("Replace")){save_slot();ImGui::CloseCurrentPopup();}
            ImGui::SameLine();if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();
            ImGui::EndPopup();
          }
          ImGui::PopID();
        }
        const auto selected=rex::cvar::Query<std::string>("pdz_settings_preset");
        ImGui::SeparatorText("Current setup");
        ImGui::TextUnformatted(selected=="classic"?"Classic Rare Style Perfect Dark":selected=="gamecube"?"Classic Rare Style — GameCube look (Experimental / Work in progress)":selected=="modernized"?"Modernized Perfect Dark Zero":selected=="vanilla"?"Vanilla Perfect Dark Zero":selected=="user1"?"User preset 1":selected=="user2"?"User preset 2":selected=="user3"?"User preset 3":"Custom");
        ImGui::TextDisabled("Individual changes turn the preset into Custom; changes are automatically saved.");
        ImGui::Separator();
        if(ImGui::Button("Exit Game##preset_exit")) {
          restart_requested_=false;lifecycle_error_.clear();leave_requested=true;
        }
        end_tab();
      }
      if(begin_tab("Movement")) {
      ImGui::SeparatorText("Movement style");
      bool dodge_roll = rex::cvar::Query<bool>("pdz_dodge_roll");
      if (ImGui::Checkbox("Enhanced dodge roll", &dodge_roll)) set_bool("pdz_dodge_roll", dodge_roll);
      ImGui::TextWrapped("Roll boost: 1.6, then 1.3, then original strength when repeated within 2 seconds. Fully recovers after 3.5 seconds without another roll. Off restores original rolls.");
      bool modern_movement = rex::cvar::Query<bool>("pdz_modern_movement");
      const bool classic_movement = rex::cvar::Query<bool>("pdz_classic_movement");
      int movement_preset = classic_movement ? 1 : modern_movement ? 0 : 2;
      const char* movement_presets[] = {"Modern", "Classic (Perfect Dark / GoldenEye)", "Original PDZ"};
      if (ImGui::Combo("Movement preset", &movement_preset, movement_presets, 3)) {
        set_bool("pdz_classic_movement", movement_preset == 1);
        set_bool("pdz_modern_movement", movement_preset == 0);
      }
      ImGui::BeginDisabled(movement_preset != 0);
      float response = float(rex::cvar::Query<double>("pdz_movement_response"));
      if (pdz::ui::SliderFloat("Movement response", &response, 1.0f, 8.0f, "%.1fx"))
        set_double("pdz_movement_response", response);
      ImGui::EndDisabled();
      ImGui::TextDisabled("Classic: momentum on start; quick stops and reversals.");
      ImGui::TextDisabled("80%% base speed; forward run boosts to 100%% after 3s.");
      ImGui::TextDisabled("Classic: consistent equipment speed; no hit slowdown.");
      bool unarmed_boost=rex::cvar::Query<bool>("pdz_unarmed_run_boost");
      ImGui::BeginDisabled(movement_preset!=1);
      if(ImGui::Checkbox("Classic: unarmed top speed 125%",&unarmed_boost))
        set_bool("pdz_unarmed_run_boost",unarmed_boost);
      ImGui::EndDisabled();
      ImGui::TextDisabled("Cover and other movement limits stay original.");
      bool doors=rex::cvar::Query<bool>("pdz_disable_warehouse_door_autoclose");
      if(ImGui::Checkbox("Keep Warehouse doors open",&doors))
        set_bool("pdz_disable_warehouse_door_autoclose",doors);
      ImGui::TextDisabled("Only the seven identified auto-close timers; scripted door actions stay native.");
      ImGui::SeparatorText("Combat balance");
        bool classic_npc = rex::cvar::Query<bool>("pdz_classic_npc_durability");
        if (ImGui::Checkbox("Reduced NPC health (Perfect Dark / GoldenEye style)", &classic_npc))
          set_bool("pdz_classic_npc_durability", classic_npc);
        float npc_health = float(rex::cvar::Query<double>("pdz_npc_health_scale") * 100.);
        ImGui::BeginDisabled(!classic_npc);
        if (pdz::ui::SliderFloat("NPC effective health", &npc_health, 25.f, 100.f, "%.0f%%"))
          set_double("pdz_npc_health_scale", npc_health / 100.);
        ImGui::EndDisabled();
        ImGui::TextDisabled("Standard NPCs, including allies; player and armor unchanged.");
        ImGui::TextDisabled("Preserves lethal headshots; not an exact Perfect Dark / GoldenEye balance port.");
        end_tab();
      }
      if(begin_tab("Weapons")) {
      ImGui::SeparatorText("Unarmed and equipment");
      bool hold_holster=rex::cvar::Query<bool>("pdz_hold_y_holster");
      if(ImGui::Checkbox("Hold Y to holster (0.5 seconds)",&hold_holster))
        set_bool("pdz_hold_y_holster",hold_holster);
      ImGui::TextDisabled("Tap Y on release retains native switch/pickup behavior.");
      float bob=float(rex::cvar::Query<double>("pdz_weapon_bob_amount"));
      if(pdz::ui::SliderFloat("Native weapon bob",&bob,0.f,1.f,"%.2fx"))
        set_double("pdz_weapon_bob_amount",bob);
      ImGui::TextDisabled("Procedural bob only; aim sway and action animations unchanged.");
      ImGui::SeparatorText("Animation speed");
      float weapon_speed = float(rex::cvar::Query<double>("pdz_weapon_action_speed"));
      if (pdz::ui::SliderFloat("Equip / holster / reload", &weapon_speed, 1.0f, 3.0f, "%.1fx"))
        set_double("pdz_weapon_action_speed", weapon_speed);
      ImGui::TextDisabled("1x = vanilla. Applied when the next action starts.");
        end_tab();
      }
      if(begin_tab("Aiming")) {
      ImGui::SeparatorText("Perfect Dark / GoldenEye aiming");
      bool classic_sway=rex::cvar::Query<bool>("pdz_classic_weapon_sway");
      if(ImGui::Checkbox("Perfect Dark / GoldenEye weapon sway",&classic_sway))
        set_bool("pdz_classic_weapon_sway",classic_sway);
      float sway_amount=float(rex::cvar::Query<double>("pdz_classic_weapon_sway_amount"));
      ImGui::BeginDisabled(!classic_sway);
      if(pdz::ui::SliderFloat("Weapon sway strength",&sway_amount,0.f,8.f,"%.2fx"))
        set_double("pdz_classic_weapon_sway_amount",sway_amount);
      ImGui::EndDisabled();
      bool sway_aim=rex::cvar::Query<bool>("pdz_classic_sway_aim");
      if(ImGui::Checkbox("Sway crosshair and shot direction",&sway_aim))
        set_bool("pdz_classic_sway_aim",sway_aim);
      float center_deadzone=float(rex::cvar::Query<double>("pdz_cursor_sway_deadzone")*180./3.141592653589793);
      ImGui::BeginDisabled(!classic_sway);
      if(pdz::ui::SliderFloat("Cursor sway center deadzone",&center_deadzone,0.f,3.f,"%.2f deg"))
        set_double("pdz_cursor_sway_deadzone",center_deadzone*3.141592653589793/180.);
      ImGui::EndDisabled();
      ImGui::TextDisabled("Quiet area for turn sway; deliberate free aim is unchanged.");
      bool ads_sway=rex::cvar::Query<bool>("pdz_sway_during_ads");
      if(ImGui::Checkbox("Camera / cursor sway during ADS",&ads_sway))
        set_bool("pdz_sway_during_ads",ads_sway);
      ImGui::TextDisabled("Off: centered mouse aim in ADS. Scoped target attraction is always disabled.");
      float cursor_ms=float(rex::cvar::Query<double>("pdz_cursor_visual_smoothing")*1000.);
      if(pdz::ui::SliderFloat("Cursor visual easing",&cursor_ms,0.f,150.f,"%.0f ms"))
        set_double("pdz_cursor_visual_smoothing",cursor_ms/1000.);
      ImGui::TextDisabled("Visual only: inputs and firing stay raw; 0 disables easing.");
      float zoom_ms=float(rex::cvar::Query<double>("pdz_zoom_transition")*1000.);
      if(pdz::ui::SliderFloat("Zoom easing",&zoom_ms,0.f,250.f,"%.0f ms"))
        set_double("pdz_zoom_transition",zoom_ms/1000.);
      ImGui::TextDisabled("D-pad zoom: 1/3, 2/3, full; one stage per press.");
      bool auto_aim=rex::cvar::Query<bool>("pdz_auto_aim");
      if(ImGui::Checkbox("Overlay auto-aim",&auto_aim)) {
        set_bool("pdz_auto_aim",auto_aim);
        if(auto_aim)set_bool("pdz_native_aim_assist",false);
      }
      bool native_assist=rex::cvar::Query<bool>("pdz_native_aim_assist");
      if(ImGui::Checkbox("Use in-game aim-assist setting",&native_assist)) {
        set_bool("pdz_native_aim_assist",native_assist);
        if(native_assist)set_bool("pdz_auto_aim",false);
      }
      ImGui::TextDisabled("Hip-fire only. Scoped aim is always player-controlled; manual sway remains available.");
      bool free_aim=rex::cvar::Query<bool>("pdz_classic_free_aim");
      if(ImGui::Checkbox("Perfect Dark / GoldenEye free aim",&free_aim))
        set_bool("pdz_classic_free_aim",free_aim);
      float edge=float(rex::cvar::Query<double>("pdz_crosshair_edge_deadzone"));
      ImGui::BeginDisabled(!free_aim);
      bool ads_only=rex::cvar::Query<bool>("pdz_free_aim_ads_only");
      if(ImGui::Checkbox("ADS / zoom only",&ads_only))
        set_bool("pdz_free_aim_ads_only",ads_only);
      if(pdz::ui::SliderFloat("Crosshair edge deadzone",&edge,0.f,1.f,"%.2f"))
        set_double("pdz_crosshair_edge_deadzone",edge);
      ImGui::EndDisabled();
      ImGui::TextDisabled("Mouse only; on foot. 0=turn from center, 1=no edge turn.");
      bool dpad_zoom = rex::cvar::Query<bool>("pdz_dpad_zoom");
      if (ImGui::Checkbox("D-pad scope zoom", &dpad_zoom)) set_bool("pdz_dpad_zoom", dpad_zoom);
      ImGui::TextDisabled("While aiming: left decreases, right increases zoom.");
        end_tab();
      }
      if(begin_tab("HUD")) {
      ImGui::SeparatorText("Perfect Dark HUD");
      bool half_native_hud=rex::cvar::Query<bool>("pdz_half_native_hud");
      if(ImGui::Checkbox("Compact default HUD (75%; Classic unchanged)",&half_native_hud))
        set_bool("pdz_half_native_hud",half_native_hud);
      bool classic_hud = rex::cvar::Query<bool>("pdz_classic_hud");
      if (ImGui::Checkbox("Perfect Dark-style HUD", &classic_hud))
        set_bool("pdz_classic_hud", classic_hud);
      bool ammo_hide=rex::cvar::Query<bool>("pdz_ammo_auto_hide");
      if(ImGui::Checkbox("Auto-hide Classic ammo HUD", &ammo_hide))
        set_bool("pdz_ammo_auto_hide",ammo_hide);
      float ammo_delay=float(rex::cvar::Query<double>("pdz_ammo_hide_delay"));
      if(pdz::ui::SliderFloat("Ammo hide delay",&ammo_delay,1.f,30.f,"%.1f seconds"))
        set_double("pdz_ammo_hide_delay",ammo_delay);
      float ammo_size=float(rex::cvar::Query<double>("pdz_ammo_hud_scale"));
      if(pdz::ui::SliderFloat("Ammo HUD size",&ammo_size,.25f,2.f,"%.2fx"))
        set_double("pdz_ammo_hud_scale",ammo_size);
      bool hide_objective = rex::cvar::Query<bool>("pdz_classic_hud_hide_objective");
      if (ImGui::Checkbox("Hide objective HUD / update icon", &hide_objective))
        set_bool("pdz_classic_hud_hide_objective", hide_objective);
      bool hide_interactions = rex::cvar::Query<bool>("pdz_hide_interaction_prompts");
      if (ImGui::Checkbox("Hide interaction prompts", &hide_interactions))
        set_bool("pdz_hide_interaction_prompts", hide_interactions);
      ImGui::TextDisabled("Hides action text; interactions still work. Pickup panels stay visible.");
      bool classic_crosshair = rex::cvar::Query<bool>("pdz_classic_hud_crosshair");
      if (ImGui::Checkbox("Perfect Dark-style reticle", &classic_crosshair))
        set_bool("pdz_classic_hud_crosshair", classic_crosshair);
      ImGui::TextDisabled("Health follows the original visibility rules; reticle size 0 gives a square dot.");
      bool classic_scope=rex::cvar::Query<bool>("pdz_classic_scope");
      if(ImGui::Checkbox("Perfect Dark scope", &classic_scope))
        set_bool("pdz_classic_scope",classic_scope);
      ImGui::TextDisabled("Weapon scope artwork includes RCP90; thermal/X-ray effects stay unchanged.");
      ImGui::SeparatorText("Reticle appearance");
      ImGui::TextDisabled("Color and opacity are shared by the regular reticle and Perfect Dark scope.");
      bool hide_original = rex::cvar::Query<bool>("pdz_hide_original_crosshair");
      if (ImGui::Checkbox("Hide original crosshair", &hide_original)) {
        set_bool("pdz_hide_original_crosshair", hide_original);
      }
      float size = float(rex::cvar::Query<double>("pdz_crosshair_size"));
      float gap = float(rex::cvar::Query<double>("pdz_crosshair_gap"));
      float thickness =
          float(rex::cvar::Query<double>("pdz_crosshair_thickness"));
      float outline =
          float(rex::cvar::Query<double>("pdz_crosshair_outline"));
      if (pdz::ui::SliderFloat("Size", &size, 0.0f, 24.0f, "%.1f"))
        set_double("pdz_crosshair_size", size);
      if (pdz::ui::SliderFloat("Gap", &gap, 0.0f, 20.0f, "%.1f"))
        set_double("pdz_crosshair_gap", gap);
      if (pdz::ui::SliderFloat("Thickness", &thickness, 1.0f, 6.0f, "%.1f"))
        set_double("pdz_crosshair_thickness", thickness);
      if (pdz::ui::SliderFloat("Outline", &outline, 0.0f, 4.0f, "%.1f"))
        set_double("pdz_crosshair_outline", outline);

      float color[3] = {
          std::clamp(rex::cvar::Query<int32_t>("pdz_crosshair_red") / 255.0f,
                     0.0f, 1.0f),
          std::clamp(rex::cvar::Query<int32_t>("pdz_crosshair_green") / 255.0f,
                     0.0f, 1.0f),
          std::clamp(rex::cvar::Query<int32_t>("pdz_crosshair_blue") / 255.0f,
                     0.0f, 1.0f)};
      if (ImGui::ColorEdit3("Color", color,
                            ImGuiColorEditFlags_NoInputs)) {
        set_int("pdz_crosshair_red", int(color[0] * 255.0f + 0.5f));
        set_int("pdz_crosshair_green", int(color[1] * 255.0f + 0.5f));
        set_int("pdz_crosshair_blue", int(color[2] * 255.0f + 0.5f));
      }
      // Numeric channels are navigable without a mouse-driven colour picker.
      for(const auto& channel:std::array<std::pair<const char*,const char*>,3>{{{"Red","pdz_crosshair_red"},{"Green","pdz_crosshair_green"},{"Blue","pdz_crosshair_blue"}}}){
        int value=rex::cvar::Query<int32_t>(channel.second);
        if(pdz::ui::SliderInt(channel.first,&value,0,255))set_int(channel.second,value);
      }
      int opacity = std::clamp(
          int(rex::cvar::Query<int32_t>("pdz_crosshair_opacity")), 0, 255);
      if (pdz::ui::SliderInt("Opacity", &opacity, 0, 255))
        set_int("pdz_crosshair_opacity", opacity);

        end_tab();
      }
      if(begin_tab("Display")) {
      ImGui::SeparatorText("Window mode");
      const int mode=display_mode();
      if(ImGui::BeginCombo("Display mode",pdz::ui::DisplayModeLabel(mode))) {
        for(int option=0;option<3;++option)
          if(ImGui::Selectable(pdz::ui::DisplayModeLabel(option),mode==option))apply_display_mode(option);
        ImGui::EndCombo();
      }
      if(ImGui::Button("Cycle display mode (F11)"))apply_display_mode(pdz::ui::NextDisplayMode(mode));
      ImGui::TextWrapped("F11 cycles Windowed, Borderless and Fullscreen. Changes apply live and save automatically. Fullscreen requests an exclusive display mode; unsupported modes may fall back to Borderless.");
      ImGui::SeparatorText("Graphics quality");
      const auto resolution=rex::cvar::Query<std::string>("resolution");
      if(ImGui::BeginCombo("Display resolution",resolution.empty()?"Default":resolution.c_str())) {
        for(auto v:{"1280x720","1920x1080","2560x1440","3840x2160"})
          if(ImGui::Selectable(v,resolution==v))if(rex::cvar::SetFlagByName("resolution",v))mark_unsaved();
        ImGui::EndCombo();
      }
      ImGui::TextWrapped("Resizes the window and selects the fullscreen display mode. Restart after changing this for the game's video-mode setting. Internal render scale below controls actual scene detail independently of window size.");
      int render_scale=rex::cvar::Query<int32_t>("resolution_scale");
      if(ImGui::BeginCombo("Internal render scale (restart)",(std::to_string(render_scale)+"x").c_str())) {
        for(int option=1;option<=3;++option)if(ImGui::Selectable((std::to_string(option)+"x").c_str(),render_scale==option)) {
          set_int("resolution_scale",option);set_int("draw_resolution_scale_x",option);set_int("draw_resolution_scale_y",option);
        }
        ImGui::EndCombo();
      }
      ImGui::TextWrapped("Higher render scales improve detail but use more GPU memory and performance. Requires a full game restart.");
      bool smaa=rex::cvar::Query<bool>("pdz_smaa");
      const bool dx12=rex::cvar::Query<std::string>("graphics_backend")=="d3d12";
      ImGui::BeginDisabled(!dx12);
      if(ImGui::Checkbox("SMAA 1x High anti-aliasing",&smaa))set_bool("pdz_smaa",smaa);
      ImGui::EndDisabled();
      ImGui::TextWrapped(dx12?"Optional edge smoothing; applies live. Keeps the original game rendering when off. The F8 menu is drawn afterward and stays sharp.":"SMAA currently requires the DX12 renderer. Vulkan SMAA is not implemented.");
      if(rex::cvar::GetFlagInfo("pdz_disable_motion_blur")) {
        bool disabled=rex::cvar::Query<bool>("pdz_disable_motion_blur");
        ImGui::BeginDisabled(!dx12);
        if(ImGui::Checkbox("Disable motion blur",&disabled))
          set_bool("pdz_disable_motion_blur",disabled);
        ImGui::EndDisabled();
        ImGui::TextWrapped("Removes the game's motion-blur filter. Requires DX12; bloom and scope effects are preserved.");
      }
      ImGui::SeparatorText("Menu legibility");
      float ui_scale=float(rex::cvar::Query<double>("pdz_overlay_scale"));
      bool auto_scale=ui_scale==0;
      if(ImGui::Checkbox("Automatic interface scale",&auto_scale))set_double("pdz_overlay_scale",auto_scale?0.f:scale);
      if(!auto_scale){ui_scale=std::max(1.f,ui_scale);if(pdz::ui::SliderFloat("Interface scale",&ui_scale,1.f,2.5f,"%.2fx"))set_double("pdz_overlay_scale",ui_scale);}
      ImGui::TextWrapped("Auto follows screen height. Increase the manual scale for a distant TV. Large scales are capped on small screens to keep controls reachable. Settings scroll automatically with controller focus.");
      ImGui::SeparatorText("Camera");
      float horizontal_fov =
          float(rex::cvar::Query<double>("pdz_horizontal_fov"));
      if (pdz::ui::SliderFloat("Horizontal FOV", &horizontal_fov, 60.0f,
                             140.0f, "%.0f deg")) {
        set_double("pdz_horizontal_fov", horizontal_fov);
      }
      ImGui::SeparatorText("Color grading");
      bool scene_only = rex::cvar::Query<bool>("pdz_scene_only_grading");
      if (ImGui::Checkbox("World only (preserve HUD)", &scene_only))
        set_bool("pdz_scene_only_grading", scene_only);

      float gamma = float(rex::cvar::Query<double>("pdz_display_gamma"));
      if (pdz::ui::SliderFloat("Gamma", &gamma, 0.5f, 2.0f, "%.2f"))
        set_double("pdz_display_gamma", gamma);
      ImGui::SameLine();
      if (ImGui::SmallButton("Reset gamma")) set_double("pdz_display_gamma", 1.0f);
      ImGui::TextDisabled("1.00 = original; higher lifts midtones, not black level");

      float contrast = float(rex::cvar::Query<double>("pdz_display_contrast"));
      float saturation = float(rex::cvar::Query<double>("pdz_display_saturation"));
      float tint = float(rex::cvar::Query<double>("pdz_display_tint"));
      if (pdz::ui::SliderFloat("Contrast", &contrast, 0.0f, 2.0f, "%.2f"))
        set_double("pdz_display_contrast", contrast);
      if (pdz::ui::SliderFloat("Saturation", &saturation, 0.0f, 2.0f, "%.2f"))
        set_double("pdz_display_saturation", saturation);
      if (pdz::ui::SliderFloat("Tint (green / magenta)", &tint, -0.5f, 0.5f, "%.2f"))
        set_double("pdz_display_tint", tint);
      if (ImGui::SmallButton("Reset color grading")) {
        set_double("pdz_display_contrast", 1.0f);
        set_double("pdz_display_saturation", 1.0f);
        set_double("pdz_display_tint", 0.0f);
      }
      ImGui::TextDisabled(scene_only ? "Scene only; HUD and debug menus are unchanged."
                                    : "Includes game HUD; debug menus are unchanged.");
      ImGui::SeparatorText("Frame pacing");
      int frame_limit = rex::cvar::Query<int32_t>("pdz_frame_limit");
      const std::string cap_label=frame_limit?std::to_string(frame_limit)+" FPS":"Uncapped";
      if(ImGui::BeginCombo("Frame rate limit",cap_label.c_str())) {
        for(int fps:{0,30,60,90,120,144,165,240}) {
          const std::string label=fps?std::to_string(fps)+" FPS":"Uncapped";
          if(ImGui::Selectable(label.c_str(),frame_limit==fps))set_int("pdz_frame_limit",fps);
        }
        ImGui::EndCombo();
      }
      bool host_vsync = rex::cvar::Query<bool>("pdz_host_vsync");
      if (ImGui::Checkbox("VSync (DX12 display)", &host_vsync)) {
        set_bool("pdz_host_vsync", host_vsync);
      }
      ImGui::TextWrapped("VSync prevents tearing by syncing presentation to the display. The frame limit caps completed game frames; it does not guarantee the game can reach that rate.");
        end_tab();
      }
      if(begin_tab("Mouse/keys")) {
      ImGui::SeparatorText("Mouse look");
      float mouse_sensitivity =
          float(rex::cvar::Query<double>("pdz_mouse_sensitivity"));
      float zoom_compensation =
          float(rex::cvar::Query<double>("pdz_mouse_zoom_compensation"));
      float zoom_sensitivity = float(
          rex::cvar::Query<double>("pdz_mouse_zoom_sensitivity_scale"));
      if (pdz::ui::SliderFloat("Mouse sensitivity", &mouse_sensitivity,
                             0.01f, 5.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic)) {
        set_double("pdz_mouse_sensitivity", mouse_sensitivity);
      }
      if (pdz::ui::SliderFloat("Zoom precision compensation", &zoom_compensation,
                             0.0f, 1.0f, "%.3f")) {
        set_double("pdz_mouse_zoom_compensation", zoom_compensation);
      }
      if (pdz::ui::SliderFloat("Zoom relative sensitivity", &zoom_sensitivity,
                             0.001f, 2.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic)) {
        set_double("pdz_mouse_zoom_sensitivity_scale", zoom_sensitivity);
      }
      ImGui::TextDisabled("Relative sensitivity applies only while zoomed");

      ImGui::SeparatorText("Gameplay bindings");
        bool input_changed=false;
        PDZ_DrawInputTab(input_changed,PDZ_InputPage::KeyboardMouse);
        if(input_changed) mark_unsaved();
        end_tab();
      }
      if(begin_tab("Controller")) {
        bool input_changed=false;
        PDZ_DrawInputTab(input_changed,PDZ_InputPage::Controller);
        if(input_changed) mark_unsaved();
        end_tab();
      }
      if(begin_tab("Gyro")) {
        bool gyro_changed=false;
        PDZ_DrawInputTab(gyro_changed,PDZ_InputPage::Gyro);
        if(gyro_changed)mark_unsaved();
        end_tab();
      }
      if(begin_tab("Menus")) {
        bool input_changed=false;
        PDZ_DrawInputTab(input_changed,PDZ_InputPage::Menus);
        if(input_changed) mark_unsaved();
        end_tab();
      }
      if(begin_tab("Experimental")) {
      bool gamecube=rex::cvar::Query<bool>("pdz_stylized_lighting");
      if(ImGui::Button(gamecube?"GameCube look: On (Experimental / Work in progress)":"GameCube look: Off (Experimental / Work in progress)"))
        set_bool("pdz_stylized_lighting",!gamecube);
      ImGui::TextWrapped("Toggles cel shading only; other settings are unchanged. Quick Access / semicolon remains available.");
      ImGui::SeparatorText("HDR output (Windows DX12 — experimental)");
      if (rex::cvar::GetFlagInfo("pdz_hdr_output")) {
      bool hdr_output=rex::cvar::Query<bool>("pdz_hdr_output");
      if(ImGui::Checkbox("HDR output (SDR-derived)",&hdr_output))set_bool("pdz_hdr_output",hdr_output);
      ImGui::TextUnformatted(rex::cvar::Query<bool>("pdz_hdr_active") ? "Current output: HDR scRGB" : "Current output: SDR");
      ImGui::TextWrapped("Requires Windows HDR enabled on this window's display. Unsupported displays use SDR automatically. This preserves SDR colours in an HDR signal; original HDR lighting is not recovered yet. Vulkan and Proton HDR are not qualified.");
      if(hdr_output) {
        float white=float(rex::cvar::Query<double>("pdz_hdr_paper_white"));
        if(pdz::ui::SliderFloat("HDR game / native HUD white",&white,80.f,400.f,"%.0f nits"))
          set_double("pdz_hdr_paper_white",white);
        float ui_white=float(rex::cvar::Query<double>("pdz_hdr_ui_white"));
        if(pdz::ui::SliderFloat("HDR overlay white",&ui_white,80.f,400.f,"%.0f nits"))
          set_double("pdz_hdr_ui_white",ui_white);
        ImGui::TextDisabled("Output mode updates within a second. No highlight expansion or peak-brightness control yet.");
      }
      } else ImGui::TextDisabled("HDR transport requires the Windows DX12 build.");
      ImGui::SeparatorText("Upstream third-person camera (test)");
      bool third_person=rex::cvar::Query<bool>("pdz_tp_enable");
      if(ImGui::Checkbox("Third-person camera",&third_person))
        set_bool("pdz_tp_enable",third_person);
      ImGui::TextWrapped("Start in first person; F7 or both stick clicks toggles third person. Stable-cover anchoring is suspended while third person is enabled.");
      ImGui::TextWrapped("Both stick clicks together: toggle perspective (F7 also works). F6 / right-stick click: ease to the opposite shoulder in third person. Lone stick clicks wait 150ms for the combination; first-person actions remain native. A centred offset uses 80 units. Changes save automatically.");
      ImGui::TextWrapped("Includes wall-clearance targeting and a dynamic obstruction indicator. Character aiming animation does not follow the off-centre sway cursor yet.");
      if(third_person) {
        for(const char* axis:{"x","y","z"}) {
          const std::string flag=std::string("pdz_tp_offset_")+axis;
          float offset=float(rex::cvar::Query<double>(flag));
          const std::string label=std::string("Third-person offset ")+axis;
          if(pdz::ui::SliderFloat(label.c_str(),&offset,-1000.f,1000.f,"%.0f"))
            set_double(flag.c_str(),offset);
        }
      }
      ImGui::SeparatorText("Cover camera");
      bool stable_cover=rex::cvar::Query<bool>("pdz_cover_camera_stabilization");
      if(ImGui::Checkbox("Stable cover camera (test)",&stable_cover))
        set_bool("pdz_cover_camera_stabilization",stable_cover);
      ImGui::TextWrapped("Anchors horizontal camera movement during cover peeking. Height and normal cover exits stay native. Test wall clearance and shot alignment; off restores original positioning.");
      bool experimental_input_changed=false;
      PDZ_DrawInputTab(experimental_input_changed,PDZ_InputPage::Experimental);
      if(experimental_input_changed)mark_unsaved();
      ImGui::TextWrapped("Unfinished features and rendering experiments. Presets may enable selected experiments. Enhanced dodge roll is a separate Movement option.");
      ImGui::SeparatorText("Experimental material lighting (DX12)");
      bool stylized=rex::cvar::Query<bool>("pdz_stylized_lighting");
      if(ImGui::Checkbox("Stylized lighting experiment",&stylized))
        set_bool("pdz_stylized_lighting",stylized);
      ImGui::TextDisabled("Flat materials, preserved color, softer shade boundaries; glow/material emission 15%%.");
      ImGui::TextDisabled("Semicolon / Quick Access toggles GameCube look; changes save automatically.");
      ImGui::TextColored(ImVec4(1,.65f,.2f,1),"Baked texture relief/lighting may remain; shared material emission can retain faint reflections.");
      ImGui::TextDisabled("Off restores vanilla materials; first enable may compile new shaders.");
      ImGui::SeparatorText("Unfinished weapon / aim features");
      bool p9p_clean=rex::cvar::Query<bool>("pdz_p9p_hide_attachments");
      if(ImGui::Checkbox("P9P: hide scope, flashlight and bracket (experimental)",&p9p_clean)) {
        set_bool("pdz_p9p_hide_attachments",p9p_clean);
        PDZ_UpdateP9PModel();
      }
      ImGui::TextWrapped("First-person held P9P only. Keeps the silencer and original aiming behaviour. Off restores attachments.");
      bool idle_hands=rex::cvar::Query<bool>("pdz_hide_unarmed_hands");
      if(ImGui::Checkbox("Hide unarmed hands (idle and movement)",&idle_hands))
        set_bool("pdz_hide_unarmed_hands",idle_hands);
      ImGui::TextColored(ImVec4(1.f,.7f,.2f,1.f),"Forward/back movement correction awaiting in-game confirmation.");
      bool accuracy=rex::cvar::Query<bool>("pdz_weapon_accuracy");
      if(ImGui::Checkbox("50% less weapon spread",&accuracy))
        set_bool("pdz_weapon_accuracy",accuracy);
      ImGui::TextDisabled("Verified pistols and RCP90 for now; other weapons pending classification.");
      bool range_boost=rex::cvar::Query<bool>("pdz_auto_aim_range_boost");
      if(ImGui::Checkbox("Close-range auto-aim boost",&range_boost))
        set_bool("pdz_auto_aim_range_boost",range_boost);
      float close_boost=float(rex::cvar::Query<double>("pdz_auto_aim_close_boost"));
      if(pdz::ui::SliderFloat("Close-range attraction area",&close_boost,1.f,2.f,"%.2fx"))
        set_double("pdz_auto_aim_close_boost",close_boost);
      ImGui::TextDisabled("Mouse assist only; widens attraction, keeps native visibility checks.");
        end_tab();
      }
      if(begin_tab("System")) {
      ImGui::SeparatorText("Game session");
      ImGui::TextWrapped("Restart or exit the game. Settings are saved first; unsaved mission progress will be lost.");
      if(ImGui::Button("Restart Game")) {
        restart_requested_=true; lifecycle_error_.clear(); leave_requested=true;
      }
      ImGui::SameLine();
      if(ImGui::Button("Exit Game")) {
        restart_requested_=false; lifecycle_error_.clear(); leave_requested=true;
      }
      ImGui::SeparatorText("Diagnostics");
      if(settings_save_failed_) {
        ImGui::TextWrapped("%s",settings_save_message_.c_str());
        if(ImGui::Button("Retry saving")) SaveSettings();
      }
      if(ImGui::CollapsingHeader("Menu controls and shortcuts")) {
        ImGui::TextWrapped("D-pad or left stick: navigate. Bottom face: select/edit. Right face: cancel editing or close. LB/RB: change tab. Select a slider, adjust left/right, then select again to finish.");
        ImGui::TextWrapped("F8 or View + Menu opens settings. Semicolon or Quick Access toggles GameCube look. F11 cycles display modes. Changes save automatically after adjustment.");
      }
#if defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
      bool timing_log = rex::cvar::Query<bool>("pdz_timing_log");
      if (ImGui::Checkbox("Record frame timing", &timing_log)) {
        set_bool("pdz_timing_log", timing_log);
      }
      ImGui::TextDisabled("Diagnostic logging only; leave off unless investigating frame pacing.");
#endif
      ImGui::SeparatorText("Known limitations");
      ImGui::TextWrapped("Gadget overlays stay original. Replacing weapon scope artwork does not remove thermal tint or X-ray effects.");
        end_tab();
      }
      ImGui::EndTabBar();
      requested_tab_=-1;
      }
      if(leave_requested)ImGui::OpenPopup("Leave game?");
      DrawLeaveConfirmation();
      ImGui::TextWrapped("D-pad: navigate | Select: edit | Back: close | LB / RB: tabs");
      if(settings_save_failed_) {
        ImGui::TextColored(ImVec4(1.f,.4f,.4f,1.f),"Save failed (hover for details)");
        if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",settings_save_message_.c_str());
      } else ImGui::TextDisabled(settings_dirty_?"Saving after adjustment...":"Changes saved automatically");
    }
    ImGui::End();
    ImGui::GetStyle()=saved_style;
  }
  SetControllerBlocked(show_menu);

  // Flush completed controls and hotkeys even when F8 has just been closed.
  // Active slider drags are coalesced; failed writes retry with a visible error.
  if(settings_dirty_&&!ImGui::IsAnyItemActive()&&ImGui::GetTime()>=settings_retry_after_) SaveSettings();

  const bool capture_mouse = pdz::HasInteractiveUi(show_menu);
  const bool changed = pdz::ui_mouse_captured.exchange(
      capture_mouse, std::memory_order_acq_rel) != capture_mouse;
  if (changed) {
    float discarded_x = 0, discarded_y = 0;
    rex::input::mnk::ConsumeRelativeMouseMotion(&discarded_x, &discarded_y);
    REXLOG_INFO("PDZ UI mouse: {}", capture_mouse ? "released to UI" : "returned to game");
  }
  // Release pointer lock through the driver's existing capture mechanism.
  rex::input::mnk::SetMouseLookActive(!capture_mouse);
  io.MouseDrawCursor = capture_mouse;
  ImGui::SetNextFrameWantCaptureMouse(capture_mouse);

  if (rex::cvar::Query<bool>("pdz_classic_hud")) {
    pdz::hud::LiveState live;
    if (PDZ_ReadClassicHud(live)) {
      ImDrawList* draw = ImGui::GetBackgroundDrawList();
      const float scale = std::max(.25f, std::min(io.DisplaySize.x/1280.f,io.DisplaySize.y/720.f));
      const auto mesh = [&](const pdz::hud::Mesh& geometry, ImVec2 origin,float opacity=1.f) {
        const ImDrawListFlags flags = draw->Flags;
        draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        const auto point = [&](pdz::hud::Point p) {return ImVec2(origin.x+p.x*scale,origin.y+p.y*scale);};
        for (const auto& triangle : geometry.triangles) {
          const auto c=triangle.a.color;
          draw->AddTriangleFilled(point(triangle.a.point),point(triangle.b.point),point(triangle.c.point),IM_COL32(c.r,c.g,c.b,int(c.a*opacity)));
        }
        draw->Flags=flags;
      };
      if(live.health_alpha>0.f)
        mesh(pdz::hud::Health(live.health,live.shield,live.recoverable_health),ImVec2(io.DisplaySize.x*.5f-85.f*scale,45.f*scale),live.health_alpha);
      // Reference screenshot proportions, independent of the health HUD scale.
      const float ammo_alpha=ammo_visibility_.Update(live,ImGui::GetTime(),
          rex::cvar::Query<bool>("pdz_ammo_auto_hide"),rex::cvar::Query<double>("pdz_ammo_hide_delay"));
      const float ammo_scale=std::max(.25f,std::min(io.DisplaySize.x/384.f,io.DisplaySize.y/216.f))*
          float(rex::cvar::Query<double>("pdz_ammo_hud_scale"));
      const auto ammo_mesh=[&](const pdz::hud::Mesh& geometry,ImVec2 origin) {
        const auto flags=draw->Flags;draw->Flags&=~ImDrawListFlags_AntiAliasedFill;
        const auto point=[&](pdz::hud::Point p){return ImVec2(origin.x+p.x*ammo_scale,origin.y+p.y*ammo_scale);};
        for(const auto& t:geometry.triangles) {const auto c=t.a.color;
          draw->AddTriangleFilled(point(t.a.point),point(t.b.point),point(t.c.point),IM_COL32(c.r,c.g,c.b,int(c.a*ammo_alpha)));}
        draw->Flags=flags;
      };
      const auto ammo = [&](uint32_t loaded,uint32_t capacity,uint32_t reserve,uint32_t reserve_capacity,float right,pdz::hud::Color tint,bool marker) {
        if (!capacity||ammo_alpha<=0.f) return;
        const float bottom=io.DisplaySize.y-13.f*ammo_scale;
        const float x=io.DisplaySize.x-right*ammo_scale;
        ammo_mesh(pdz::hud::Ammo(int(loaded),int(capacity),9,57,tint),{x,bottom-96.f*ammo_scale});
        ammo_mesh(pdz::hud::Reserve(reserve,reserve_capacity),{x,bottom-36.f*ammo_scale});
        const auto text = [&](uint32_t value,float y,pdz::hud::Color color) {
          const auto label=std::to_string(value);
          const float font_size=5.f*ammo_scale;
          const float width=pdz::hud::numeric_font.Ready()?pdz::hud::numeric_font.Width(value):
              ImGui::GetFont()->CalcTextSizeA(font_size,FLT_MAX,0,label.c_str()).x/ammo_scale;
          const ImVec2 p(x-(width+2)*ammo_scale,y);
          draw->AddRectFilled({p.x-ammo_scale*.5f,p.y-ammo_scale*.5f},
              {p.x+(width+.5f)*ammo_scale,p.y+5.5f*ammo_scale},IM_COL32(0,0,0,int(160*ammo_alpha)));
          const auto ink=IM_COL32(color.r,color.g,color.b,int(color.a*ammo_alpha));
          if(!pdz::hud::numeric_font.Draw(draw,value,p,ammo_scale,ink))
            draw->AddText(ImGui::GetFont(),font_size,p,ink,label.c_str());
        };
        text(loaded,bottom-44.f*ammo_scale,{tint.r,tint.g,tint.b,255});
        text(reserve,bottom-35.f*ammo_scale,{0,208,176,255});
        if(marker&&live.function_known)
          ammo_mesh(pdz::hud::FunctionMarker(live.secondary_function),{x-13.f*ammo_scale,bottom-11.f*ammo_scale});
      };
      ammo(live.loaded,live.capacity,live.reserve,live.reserve_capacity,32.f,{0,192,0,96},true);
      ammo(live.secondary_loaded,live.secondary_capacity,live.secondary_reserve,live.secondary_reserve_capacity,64.f,{255,176,48,96},false);
    } else ammo_visibility_.Reset();
  } else ammo_visibility_.Reset();

  const auto read_reticle_color=[]() {
    const auto channel=[](const char* flag) {
      return uint8_t(std::clamp(rex::cvar::Query<int32_t>(flag),0,255));
    };
    return pdz::hud::Color{channel("pdz_crosshair_red"),channel("pdz_crosshair_green"),
                           channel("pdz_crosshair_blue"),channel("pdz_crosshair_opacity")};
  };
  // Observe native scope visibility too, not only replacement artwork.
  PDZ_ClassicScopeState scope;
  const bool scope_visible=!capture_mouse&&PDZ_ReadClassicScope(scope);
  const bool auxiliary_impact_marker=pdz::hud::ShowAuxiliaryThirdPersonMarker(scope_visible,scope.aiming);
  PDZ_ThirdPersonImpact impact;
  const bool impact_active=!capture_mouse&&PDZ_ShouldDrawCustomCrosshair()&&PDZ_ReadThirdPersonImpact(impact);
  static pdz::aim::CursorVisual impact_visual;
  if(!impact_active)impact_visual.Reset();
  const auto display_impact=[&](double& x,double& y) {
    if(!impact_active)return;
    const pdz::aim::Point intended{x,y};
    const auto visual=impact_visual.Update(impact.blocked?pdz::aim::Point{impact.x,impact.y}:intended,io.DeltaTime,.035);
    if(impact.blocked&&auxiliary_impact_marker) {
      const ImVec2 p(io.DisplaySize.x*float(.5+.5*intended.x),io.DisplaySize.y*float(.5-.5*intended.y));
      ImGui::GetForegroundDrawList()->AddCircle(p,3.f,IM_COL32(210,210,210,100),12,1.f);
    }
    x=visual.x;y=visual.y;
  };
  if(scope_visible&&rex::cvar::Query<bool>("pdz_classic_scope")) {
    const auto scope_color=read_reticle_color();
    const ImU32 ink=IM_COL32(scope_color.r,scope_color.g,scope_color.b,scope_color.a);
    double x=0,y=0; PDZ_ReadVisualCrosshair(x,y);
    if(scope.aiming)display_impact(x,y);
    const pdz::hud::Point cursor{io.DisplaySize.x*float(.5+.5*x),io.DisplaySize.y*float(.5-.5*y)};
    auto* draw=ImGui::GetForegroundDrawList();
    const auto geometry=pdz::hud::ClassicScope(io.DisplaySize.x,io.DisplaySize.y,
        scope.aiming?1.f:1.f/scope.selected_zoom,scope.aiming,cursor,scope_color);
    const auto flags=draw->Flags;draw->Flags&=~ImDrawListFlags_AntiAliasedFill;
    for(const auto& t:geometry.triangles) {
      const auto c=t.a.color;
      draw->AddTriangleFilled({t.a.point.x,t.a.point.y},{t.b.point.x,t.b.point.y},
          {t.c.point.x,t.c.point.y},IM_COL32(c.r,c.g,c.b,c.a));
    }
    draw->Flags=flags;
    char label[48];std::snprintf(label,sizeof(label),"%.2fX / %.2fX",scope.current_zoom,scope.selected_zoom);
    const float scale=io.DisplaySize.y/240.f;
    float tx=io.DisplaySize.x*.5f-float(std::char_traits<char>::length(label)*4-1)*scale*.5f;
    const float ty=io.DisplaySize.y-20.f*scale;
    draw->AddRectFilled({tx-scale,ty-scale},{io.DisplaySize.x-tx+scale,ty+6*scale},IM_COL32(0,0,0,160*scope_color.a/255));
    for(char ch:std::string_view(label)) {
      if(ch>='0'&&ch<='9') {
        if(!pdz::hud::numeric_font.Draw(draw,uint32_t(ch-'0'),{tx,ty},scale,ink)) {
          char digit[]{ch,0};draw->AddText(ImGui::GetFont(),5*scale,{tx,ty},ink,digit);
        }
      } else if(ch=='.') draw->AddRectFilled({tx,ty+4*scale},{tx+scale,ty+5*scale},ink);
      else if(ch=='X') {
        draw->AddLine({tx,ty},{tx+2*scale,ty+4*scale},ink,scale);
        draw->AddLine({tx+2*scale,ty},{tx,ty+4*scale},ink,scale);
      } else if(ch=='/') draw->AddLine({tx,ty+4*scale},{tx+2*scale,ty},ink,scale);
      tx+=4*scale;
    }
    if(scope.aiming) {draw->AddRectFilled({cursor.x-scale*.5f,cursor.y-scale*.5f},
        {cursor.x+scale*.5f,cursor.y+scale*.5f},ink);return;}
  }

  const bool classic_reticle=rex::cvar::Query<bool>("pdz_classic_hud")&&
      rex::cvar::Query<bool>("pdz_classic_hud_crosshair");
  // Native HUD remains authoritative in Vanilla; add a distinct impact ring.
  if(!classic_reticle&&impact_active&&impact.blocked&&auxiliary_impact_marker) {
    const auto visual=impact_visual.Update({impact.x,impact.y},io.DeltaTime,.035);
    const ImVec2 p(io.DisplaySize.x*float(.5+.5*visual.x),io.DisplaySize.y*float(.5-.5*visual.y));
    auto* ink=ImGui::GetForegroundDrawList();
    ink->AddCircle(p,6.f,IM_COL32(0,0,0,220),16,3.f);
    ink->AddCircle(p,6.f,IM_COL32(255,185,65,240),16,1.5f);
  }
  if (!classic_reticle) return;
  // F8 is an explicit preview: mouse capture pauses aim updates and the native
  // visibility heartbeat may expire, but neither should hide its live reticle.
  const bool preview_classic_reticle=show_menu&&classic_reticle;
  if (!preview_classic_reticle&&(capture_mouse || !PDZ_ShouldDrawCustomCrosshair())) return;

  const float size = float(rex::cvar::Query<double>("pdz_crosshair_size"));
  const float gap = float(rex::cvar::Query<double>("pdz_crosshair_gap"));
  const float thickness =
      float(rex::cvar::Query<double>("pdz_crosshair_thickness"));
  const float outline =
      float(rex::cvar::Query<double>("pdz_crosshair_outline"));
  const auto reticle_color=read_reticle_color();
  const int alpha=reticle_color.a,red=reticle_color.r,green=reticle_color.g,blue=reticle_color.b;
  double aim_x=0,aim_y=0;
  if(!preview_classic_reticle) PDZ_ReadVisualCrosshair(aim_x,aim_y);
  if(!preview_classic_reticle)display_impact(aim_x,aim_y);
  const ImVec2 c(io.DisplaySize.x * float(.5+.5*aim_x), io.DisplaySize.y * float(.5-.5*aim_y));
  ImDrawList* draw = ImGui::GetForegroundDrawList();
  if(classic_reticle) {
    const auto geometry=pdz::hud::Crosshair(size,gap,thickness,
        {uint8_t(red),uint8_t(green),uint8_t(blue),uint8_t(alpha)},outline);
    const auto flags=draw->Flags;
    draw->Flags &= ~ImDrawListFlags_AntiAliasedFill;
    const auto point=[&](pdz::hud::Point p) {return ImVec2(c.x+p.x,c.y+p.y);};
    for(const auto& triangle:geometry.triangles) {
      const auto tint=triangle.a.color;
      draw->AddTriangleFilled(point(triangle.a.point),point(triangle.b.point),point(triangle.c.point),IM_COL32(tint.r,tint.g,tint.b,tint.a));
    }
    draw->Flags=flags;
    return;
  }

}

#include "../src/pdz_settings_file.h"
#include <cassert>
#include <iostream>

static std::string Read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}
int main(int argc, char** argv) {
  assert(argc == 2);
  const auto root = std::filesystem::absolute(argv[1]);
  assert(!std::filesystem::exists(root));
  std::filesystem::create_directories(root / "assets");
  const auto folder = pdz::SettingsDirectory(root);
  assert(folder == root / "settings");
  std::filesystem::create_directories(folder);
  const auto config = folder / "enhancements.toml";
  toml::table values;
  values.insert("pdz_horizontal_fov", 113.75);
  values.insert("pdz_display_gamma", 1.375);
  values.insert("pdz_display_contrast", 1.125);
  values.insert("pdz_display_saturation", .875);
  values.insert("pdz_display_tint", -.125);
  values.insert("pdz_scene_only_grading", true);
  values.insert("pdz_classic_hud", true);
  values.insert("pdz_half_native_hud", true);
  values.insert("pdz_classic_hud_hide_objective", true);
  values.insert("pdz_classic_hud_crosshair", true);
  values.insert("pdz_crosshair_size", 0.0);
  values.insert("pdz_modern_movement", false);
  values.insert("pdz_classic_movement", true);
  values.insert("pdz_unarmed_run_boost",true);
  values.insert("pdz_hide_unarmed_hands",true);
  values.insert("pdz_hold_y_holster",true);
  values.insert("pdz_weapon_bob_amount",.25);
  values.insert("pdz_controller_menu_remap",true);
  values.insert("pdz_menu_pad_a",12);
  values.insert("pdz_pad_a",11);
  values.insert("pdz_gyro_space","player");
  values.insert("pdz_gyro_acceleration","relaxed");
  values.insert("fullscreen",true);
  values.insert("fullscreen_exclusive",false);
  values.insert("pdz_gyro_enable_input",24);
  values.insert("pdz_gyro_clutch",26);
  values.insert("pdz_extra_buttons",true);
  values.insert("pdz_extra_r4",0);
  values.insert("pdz_dodge_roll", true);
  values.insert("pdz_dpad_zoom", true);
  values.insert("pdz_classic_weapon_sway", true);
  values.insert("pdz_classic_sway_aim", true);
  values.insert("pdz_classic_free_aim", true);
  values.insert("pdz_free_aim_ads_only", false);
  values.insert("pdz_sway_during_ads", false);
  values.insert("pdz_cursor_visual_smoothing", .04);
  values.insert("pdz_zoom_transition", .08);
  values.insert("pdz_cursor_sway_deadzone", .01);
  values.insert("pdz_auto_aim", false);
  values.insert("pdz_crosshair_edge_deadzone", .6);
  values.insert("pdz_classic_weapon_sway_amount", 1.25);
  values.insert("pdz_dodge_roll_strength", 1.75);
  values.insert("pdz_classic_npc_durability", true);
  values.insert("pdz_npc_health_scale", .5);
  values.insert("pdz_movement_response", 4.5);
  values.insert("pdz_weapon_action_speed", 2.0);
  values.insert("pdz_mouse_sensitivity", .1375);
  values.insert("pdz_custom_crosshair", false); // Persist default-like values too.
  values.insert("pdz_frame_limit", 60);
  values.insert("pdz_crosshair_style", "cross_dot");
  std::string error;
  assert(pdz::SaveEnhancementFile(config, values, error));
  auto reloaded = toml::parse_file(config.string());
  assert(reloaded["pdz_horizontal_fov"].value<double>() == 113.75);
  assert(reloaded["pdz_display_gamma"].value<double>() == 1.375);
  assert(reloaded["pdz_display_contrast"].value<double>() == 1.125);
  assert(reloaded["pdz_display_saturation"].value<double>() == .875);
  assert(reloaded["pdz_display_tint"].value<double>() == -.125);
  assert(reloaded["pdz_scene_only_grading"].value<bool>() == true);
  assert(reloaded["pdz_classic_hud"].value<bool>() == true);
  assert(reloaded["pdz_half_native_hud"].value<bool>() == true);
  assert(reloaded["pdz_classic_hud_hide_objective"].value<bool>() == true);
  assert(reloaded["pdz_classic_hud_crosshair"].value<bool>() == true);
  assert(reloaded["pdz_crosshair_size"].value<double>() == 0.0);
  assert(reloaded["pdz_modern_movement"].value<bool>() == false);
  assert(reloaded["pdz_classic_movement"].value<bool>() == true);
  assert(reloaded["pdz_unarmed_run_boost"].value<bool>()==true);
  assert(reloaded["pdz_hide_unarmed_hands"].value<bool>()==true);
  assert(reloaded["pdz_hold_y_holster"].value<bool>()==true);
  assert(reloaded["pdz_weapon_bob_amount"].value<double>()==.25);
  assert(reloaded["pdz_controller_menu_remap"].value<bool>()==true);
  assert(reloaded["pdz_menu_pad_a"].value<int>()==12);
  assert(reloaded["pdz_pad_a"].value<int>()==11);
  assert(reloaded["pdz_gyro_space"].value<std::string>()=="player");
  assert(reloaded["pdz_gyro_acceleration"].value<std::string>()=="relaxed");
  assert(reloaded["fullscreen"].value<bool>()==true);
  assert(reloaded["fullscreen_exclusive"].value<bool>()==false);
  assert(reloaded["pdz_gyro_enable_input"].value<int>()==24);
  assert(reloaded["pdz_gyro_clutch"].value<int>()==26);
  assert(reloaded["pdz_extra_buttons"].value<bool>()==true);
  assert(reloaded["pdz_extra_r4"].value<int>()==0);
  assert(reloaded["pdz_dodge_roll"].value<bool>() == true);
  assert(reloaded["pdz_dpad_zoom"].value<bool>() == true);
  assert(reloaded["pdz_classic_weapon_sway"].value<bool>() == true);
  assert(reloaded["pdz_classic_sway_aim"].value<bool>() == true);
  assert(reloaded["pdz_classic_free_aim"].value<bool>() == true);
  assert(reloaded["pdz_free_aim_ads_only"].value<bool>() == false);
  assert(reloaded["pdz_sway_during_ads"].value<bool>() == false);
  assert(reloaded["pdz_cursor_visual_smoothing"].value<double>() == .04);
  assert(reloaded["pdz_zoom_transition"].value<double>() == .08);
  assert(reloaded["pdz_cursor_sway_deadzone"].value<double>() == .01);
  assert(reloaded["pdz_auto_aim"].value<bool>() == false);
  assert(reloaded["pdz_crosshair_edge_deadzone"].value<double>() == .6);
  assert(reloaded["pdz_classic_weapon_sway_amount"].value<double>() == 1.25);
  assert(!reloaded.contains("pdz_dodge_roll_strength"));
  assert(reloaded["pdz_classic_npc_durability"].value<bool>() == true);
  assert(reloaded["pdz_npc_health_scale"].value<double>() == .5);
  assert(reloaded["pdz_movement_response"].value<double>() == 4.5);
  assert(reloaded["pdz_weapon_action_speed"].value<double>() == 2.0);
  assert(reloaded["pdz_custom_crosshair"].value<bool>() == false);
  assert(reloaded["pdz_crosshair_style"].value<std::string>() == "cross_dot");
  assert(!reloaded.contains("pdz_rcp_normal_colors"));
  // Independent user slots must never overwrite each other or active settings.
  const auto active = Read(config);
  for(int slot=1;slot<=3;++slot) {
    toml::table preset;
    preset.insert("pdz_crosshair_size",double(slot));
    preset.insert("keybind_x",slot==1?"R":"F");
    assert(pdz::SaveEnhancementFile(folder/("user-preset-"+std::to_string(slot)+".toml"),preset,error));
  }
  toml::table replacement;replacement.insert("pdz_crosshair_size",9.0);
  assert(pdz::SaveEnhancementFile(folder/"user-preset-2.toml",replacement,error));
  assert(toml::parse_file((folder/"user-preset-1.toml").string())["pdz_crosshair_size"].value<double>()==1.0);
  assert(toml::parse_file((folder/"user-preset-3.toml").string())["pdz_crosshair_size"].value<double>()==3.0);
  assert(toml::parse_file((folder/"user-preset-2.toml.bak").string())["pdz_crosshair_size"].value<double>()==2.0);
  assert(Read(config)==active);
  const auto original = Read(config);
  values.insert_or_assign("pdz_horizontal_fov", 120.0);
  assert(pdz::SaveEnhancementFile(config, values, error));
  assert(Read(config.string()+".bak") == original);
  assert(toml::parse_file(config.string())["pdz_horizontal_fov"].value<double>() == 120.0);
  const auto previous = Read(config);
  { std::ofstream file(config); file << "invalid = ["; }
  const auto malformed = Read(config);
  assert(!pdz::SaveEnhancementFile(config, values, error));
  assert(!error.empty() && Read(config) == malformed);
  { std::ofstream file(config); file << previous << "\nextra_setting = 42\npdz_auto_aim_scoped = true\n"; }
  assert(pdz::SaveEnhancementFile(config, values, error));
  assert(toml::parse_file(config.string())["extra_setting"].value<int>() == 42);
  assert(!toml::parse_file(config.string()).contains("pdz_auto_aim_scoped"));
#ifdef _WIN32
  const auto old = Read(config);
  assert(SetFileAttributesW(config.c_str(), FILE_ATTRIBUTE_READONLY));
  assert(!pdz::SaveEnhancementFile(config, values, error));
  assert(Read(config) == old);
  assert(SetFileAttributesW(config.c_str(), FILE_ATTRIBUTE_NORMAL));
#endif
  for (const auto& file : std::filesystem::directory_iterator(folder))
    assert(file.path().filename().string().find(".tmp-") == std::string::npos);
  assert(!std::filesystem::exists(folder / "hardware.toml"));
  assert(!std::filesystem::exists(folder / "mapping.toml"));
  std::cout << "PASS: round-trip, default values, backup, malformed/read-only refusal, "
               "extra-key preservation, temporary cleanup, shared path resolution\n";
}

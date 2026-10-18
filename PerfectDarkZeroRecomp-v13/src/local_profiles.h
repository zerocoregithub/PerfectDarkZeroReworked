// Local player profiles: separate saves per profile, managed in game with
// bind_local_profiles (F9). A profile switch applies on the next launch.
#pragma once

#include <filesystem>

namespace rex::ui
{
  class ImGuiDrawer;
} // namespace rex::ui

namespace local_profiles
{
  // Reads the profile list kept in user_data_root and returns the folder that
  // holds the running profile's saves. Runs from OnConfigurePaths, before
  // logging is up; Init logs what it found.
  std::filesystem::path Load(const std::filesystem::path &user_data_root);

  // Registers the dialog hotkey (bind_local_profiles, default F9).
  void Init(rex::ui::ImGuiDrawer *drawer);

  // Closes the dialog. Must run while the ImGui drawer is still alive.
  void Shutdown();
} // namespace local_profiles

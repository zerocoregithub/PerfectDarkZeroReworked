#pragma once

#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <toml++/toml.hpp>
#ifdef _WIN32
#include <windows.h>
#endif

namespace pdz {
inline std::filesystem::path SettingsDirectory(std::filesystem::path dir) {
  for (int i = 0; i < 6 && !dir.empty() && dir.has_parent_path(); ++i) {
    if (std::filesystem::exists(dir / "perfectdarkzerorecomp_manifest.toml") ||
        std::filesystem::exists(dir / "assets")) break;
    dir = dir.parent_path();
  }
  return dir / "settings";
}

// Write a separate overlay configuration, never hardware.toml or mapping.toml.
// Parse before writing, keep a recovery copy, then atomically replace the file.
inline bool SaveEnhancementFile(const std::filesystem::path& path,
                                const toml::table& values, std::string& error) {
  std::filesystem::path temporary;
  try {
    auto merged = std::filesystem::exists(path)
                      ? toml::parse_file(path.string()) : toml::table{};
    for (const auto& [key, value] : values) merged.insert_or_assign(key, value);
    // Retired setting must never resurrect scoped target attraction.
    merged.erase("pdz_auto_aim_scoped");
    // Target retention is now part of regular auto-aim, not an independent option.
    merged.erase("pdz_auto_aim_retention");
    merged.erase("pdz_dodge_roll_strength"); // Fixed fatigue values, no tuning cvar.
    std::ostringstream serialized;
    serialized << "# Saved from the PDZ F8 enhancements menu.\n" << merged << '\n';
    const auto content = serialized.str();
    (void)toml::parse(content);
    std::filesystem::create_directories(path.parent_path());
    temporary = path;
    temporary += ".tmp-" + std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    {
      std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
      stream.exceptions(std::ios::badbit | std::ios::failbit);
      stream.write(content.data(), static_cast<std::streamsize>(content.size()));
      stream.flush();
      stream.close();
    }
    if (std::filesystem::exists(path)) {
      auto backup = path;
      backup += ".bak";
      std::filesystem::copy_file(path, backup,
          std::filesystem::copy_options::overwrite_existing);
    }
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
      throw std::system_error(GetLastError(), std::system_category(),
                              "Cannot replace enhancements settings");
    }
#else
    std::filesystem::rename(temporary, path);
#endif
    error.clear();
    return true;
  } catch (const std::exception& e) {
    error = e.what();
    if (!temporary.empty()) {
      std::error_code ignored;
      std::filesystem::remove(temporary, ignored);
    }
    return false;
  }
}
}

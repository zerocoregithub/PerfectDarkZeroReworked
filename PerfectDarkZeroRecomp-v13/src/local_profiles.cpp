// Local player profiles. See "Local player profiles" in the README.
//
// The list lives in <user data>/local_profiles.toml. Profile 0 is the original
// one and keeps saving straight into the user data folder; profile N saves into
// <user data>/profiles/N/, so names never reach the filesystem and a rename
// moves nothing. The save folder is fixed before the runtime starts, so a
// switch applies on the next launch. The shader cache stays shared.
//
// The game reads the player's name with XamUserGetName, which the SDK always
// answers with "User"; its thunk is hooked to answer the running profile's name.
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

#include <fmt/format.h>
#include <imgui.h>
#include <toml++/toml.hpp>

#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/ui/imgui_dialog.h>
#include <rex/ui/keybinds.h>

#include "local_profiles.h"

REX_EXTERN(__imp__sub_82ACA290);

namespace
{
  constexpr const char *kFileName = "local_profiles.toml";

  // XamUserGetName hands out at most 15 characters plus the terminator.
  constexpr size_t kMaxNameLength = 15;

  struct Profile
  {
    uint32_t id;
    std::string name;
  };

  // Set by Load before the runtime starts and read-only afterwards: guest
  // threads read g_running_name through the XamUserGetName hook.
  std::filesystem::path g_file;
  uint32_t g_running_id = 0;
  std::string g_running_name = "User";
  // Why local_profiles.toml couldn't be read. While set, the file is left
  // alone, the original profile runs and the dialog is read-only.
  std::string g_load_error;

  // Owned by the UI thread (hotkey and dialog).
  std::vector<Profile> g_profiles;
  uint32_t g_selected = 0; // Profile for the next launch.
  std::unique_ptr<rex::ui::ImGuiDialog> g_dialog;

  const Profile *FindProfile(const std::vector<Profile> &profiles, uint32_t id)
  {
    const auto it = std::find_if(profiles.begin(), profiles.end(),
                                 [id](const Profile &p) { return p.id == id; });
    return it != profiles.end() ? &*it : nullptr;
  }

  bool EqualsIgnoreCase(const std::string &a, const std::string &b)
  {
    return std::equal(a.begin(), a.end(), b.begin(), b.end(), [](char x, char y)
                      { return std::tolower(static_cast<unsigned char>(x)) ==
                               std::tolower(static_cast<unsigned char>(y)); });
  }

  // Names are drawn with the game's font, so they stay plain ASCII.
  bool IsNameChar(char c)
  {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
           c == ' ' || c == '-' || c == '_';
  }

  // Returns why profile `id` can't be called `name`, or nullptr if it can.
  const char *CheckName(const std::vector<Profile> &profiles, const std::string &name, uint32_t id)
  {
    if (name.empty() || name.size() > kMaxNameLength)
    {
      return "Names are 1-15 characters long.";
    }
    if (name.front() == ' ' || name.back() == ' ')
    {
      return "Names can't start or end with a space.";
    }
    if (!std::all_of(name.begin(), name.end(), IsNameChar))
    {
      return "Names use letters, digits, spaces, - and _.";
    }
    for (const Profile &p : profiles)
    {
      if (p.id != id && EqualsIgnoreCase(p.name, name))
      {
        return "Another profile already has that name.";
      }
    }
    return nullptr;
  }

  // Parses the profile list. Returns an error message, or "" on success.
  std::string ReadProfiles(std::vector<Profile> &profiles, uint32_t &selected)
  {
    toml::table table;
    try
    {
      std::ifstream in(g_file, std::ios::binary);
      if (!in)
      {
        return "can't open the file";
      }
      table = toml::parse(in);
    }
    catch (const toml::parse_error &err)
    {
      return std::string(err.description());
    }

    const toml::array *list = table["profile"].as_array();
    const auto selected_id = table["selected"].value<int64_t>();
    if (!list || !selected_id)
    {
      return "expected 'selected' and [[profile]] entries";
    }
    for (const toml::node &node : *list)
    {
      const toml::table *entry = node.as_table();
      const auto id = entry ? (*entry)["id"].value<int64_t>() : std::nullopt;
      const auto name = entry ? (*entry)["name"].value<std::string>() : std::nullopt;
      if (!id || !name || *id < 0 || *id > UINT32_MAX)
      {
        return "every [[profile]] needs an id and a name";
      }
      if (FindProfile(profiles, static_cast<uint32_t>(*id)))
      {
        return fmt::format("profile id {} is used twice", *id);
      }
      if (const char *problem = CheckName(profiles, *name, static_cast<uint32_t>(*id)))
      {
        return fmt::format("profile {} (\"{}\"): {}", *id, *name, problem);
      }
      profiles.push_back({static_cast<uint32_t>(*id), *name});
    }
    if (!FindProfile(profiles, 0))
    {
      return "profile 0 (the original saves) is missing";
    }
    if (*selected_id < 0 || *selected_id > UINT32_MAX ||
        !FindProfile(profiles, static_cast<uint32_t>(*selected_id)))
    {
      return "'selected' is not one of the profiles";
    }
    selected = static_cast<uint32_t>(*selected_id);
    return {};
  }

  // Saves the list and makes it current. On failure nothing changes. The file
  // is written next to the real one and swapped in, so a failed write can't
  // leave a half-written list behind.
  bool Commit(std::vector<Profile> profiles, uint32_t selected)
  {
    toml::array list;
    for (const Profile &p : profiles)
    {
      list.push_back(toml::table{{"id", static_cast<int64_t>(p.id)}, {"name", p.name}});
    }
    const toml::table table{{"selected", static_cast<int64_t>(selected)},
                            {"profile", std::move(list)}};

    std::error_code ec;
    std::filesystem::create_directories(g_file.parent_path(), ec);
    std::filesystem::path temporary = g_file;
    temporary += ".tmp";
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out << "# Local player profiles, edited in game with bind_local_profiles (F9).\n"
        << table << '\n';
    out.close();
    if (!out)
    {
      REXLOG_ERROR("Local profiles: failed to write {}", temporary.string());
      return false;
    }
    std::filesystem::rename(temporary, g_file, ec);
    if (ec)
    {
      REXLOG_ERROR("Local profiles: failed to replace {}: {}", g_file.string(), ec.message());
      return false;
    }

    g_profiles = std::move(profiles);
    g_selected = selected;
    return true;
  }

  class ProfilesDialog final : public rex::ui::ImGuiDialog
  {
  public:
    explicit ProfilesDialog(rex::ui::ImGuiDrawer *drawer) : ImGuiDialog(drawer) {}

  protected:
    void OnDraw(ImGuiIO & /*io*/) override
    {
      ImGui::SetNextWindowSize(ImVec2(480, 360), ImGuiCond_FirstUseEver);
      ImGui::SetNextWindowBgAlpha(0.92f);
      if (!ImGui::Begin("Local profiles##pdz", nullptr, ImGuiWindowFlags_NoCollapse))
      {
        ImGui::End();
        return;
      }

      ImGui::Text("Playing as: %s", g_running_name.c_str());
      ImGui::Separator();
      if (!g_load_error.empty())
      {
        ImGui::TextWrapped("%s can't be read (%s), so the original profile is running. "
                           "Fix or delete the file and restart the game.",
                           g_file.string().c_str(), g_load_error.c_str());
        ImGui::End();
        return;
      }

      ImGui::TextUnformatted("Profile for the next launch:");
      // Committing replaces g_profiles, so apply a pick after the loop.
      std::optional<uint32_t> picked;
      for (const Profile &p : g_profiles)
      {
        ImGui::PushID(static_cast<int>(p.id));
        if (ImGui::Selectable(p.name.c_str(), p.id == g_selected))
        {
          picked = p.id;
        }
        ImGui::PopID();
      }
      if (picked && *picked != g_selected && !Commit(g_profiles, *picked))
      {
        status_ = "Could not save the profile list, see the log.";
      }

      ImGui::Separator();
      ImGui::InputText("Name", name_, sizeof(name_));
      if (ImGui::Button("Create"))
      {
        Create();
      }
      ImGui::SameLine();
      if (ImGui::Button("Rename selected"))
      {
        Rename();
      }
      if (!status_.empty())
      {
        ImGui::TextWrapped("%s", status_.c_str());
      }

      // The game reads the name once, so both changes wait for a restart.
      if (g_selected != g_running_id)
      {
        ImGui::TextWrapped("Restart the game to play as %s.",
                           FindProfile(g_profiles, g_selected)->name.c_str());
      }
      else if (FindProfile(g_profiles, g_running_id)->name != g_running_name)
      {
        ImGui::TextWrapped("Restart the game to show the new name in game.");
      }
      ImGui::End();
    }

  private:
    // New profiles become the profile for the next launch.
    void Create()
    {
      uint32_t id = 1;
      while (FindProfile(g_profiles, id))
      {
        ++id;
      }
      std::vector<Profile> profiles = g_profiles;
      profiles.push_back({id, name_});
      Apply(std::move(profiles), id, id);
    }

    void Rename()
    {
      std::vector<Profile> profiles = g_profiles;
      for (Profile &p : profiles)
      {
        if (p.id == g_selected)
        {
          p.name = name_;
        }
      }
      Apply(std::move(profiles), g_selected, g_selected);
    }

    // Commits the list if profile `id` may be called name_.
    void Apply(std::vector<Profile> profiles, uint32_t selected, uint32_t id)
    {
      if (const char *problem = CheckName(profiles, name_, id))
      {
        status_ = problem;
      }
      else if (!Commit(std::move(profiles), selected))
      {
        status_ = "Could not save the profile list, see the log.";
      }
      else
      {
        status_.clear();
        name_[0] = '\0';
      }
    }

    char name_[kMaxNameLength + 1] = {};
    std::string status_;
  };
} // namespace

// sub_82ACA290 is the thunk for XamUserGetName(user_index, buffer, length).
// After the SDK fills in "User", put the running profile's name there instead,
// truncated the way the SDK truncates (at most 16 bytes with the terminator).
REX_HOOK_RAW(sub_82ACA290)
{
  const uint32_t user_index = ctx.r3.u32;
  const uint32_t buffer = ctx.r4.u32;
  const uint32_t length = ctx.r5.u32;
  __imp__sub_82ACA290(ctx, base);
  if (ctx.r3.u32 != 0 || user_index != 0 || !buffer || !length)
  {
    return;
  }

  const size_t count = std::min<size_t>(g_running_name.size(), std::min(length, 16u) - 1);
  std::memcpy(base + buffer, g_running_name.data(), count);
  base[buffer + count] = 0;
}

namespace local_profiles
{
  std::filesystem::path Load(const std::filesystem::path &user_data_root)
  {
    g_file = user_data_root / kFileName;
    g_profiles = {{0, "User"}};
    g_selected = 0;

    std::error_code ec;
    if (std::filesystem::exists(g_file, ec))
    {
      std::vector<Profile> profiles;
      uint32_t selected = 0;
      g_load_error = ReadProfiles(profiles, selected);
      if (g_load_error.empty())
      {
        g_profiles = std::move(profiles);
        g_selected = selected;
      }
    }

    g_running_id = g_selected;
    g_running_name = FindProfile(g_profiles, g_running_id)->name;
    if (g_running_id == 0)
    {
      return user_data_root;
    }
    return user_data_root / "profiles" / std::to_string(g_running_id);
  }

  void Init(rex::ui::ImGuiDrawer *drawer)
  {
    if (!g_load_error.empty())
    {
      REXLOG_ERROR("Local profiles: can't read {}: {}. Running the original profile.",
                   g_file.string(), g_load_error);
    }
    else
    {
      REXLOG_INFO("Local profile: {} (id {})", g_running_name, g_running_id);
    }

    rex::ui::RegisterBind("bind_local_profiles", "F9", "Toggle local profiles", [drawer]
                          {
      if (g_dialog)
      {
        g_dialog.reset();
      }
      else
      {
        g_dialog = std::make_unique<ProfilesDialog>(drawer);
      } });
  }

  void Shutdown()
  {
    rex::ui::UnregisterBind("bind_local_profiles");
    g_dialog.reset();
  }
} // namespace local_profiles

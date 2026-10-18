#pragma once
#include <imgui.h>
#include <algorithm>

namespace pdz::ui {
inline void BeginSettingsPage(const char* label) {
  // Unique page IDs preserve independent scroll positions. Explicit height
  // keeps long pages inside the window and reserves a reachable status footer.
  const float footer=std::max(75.f,ImGui::GetFontSize()*5.f);
  const float height=std::max(ImGui::GetFontSize()*4.f,ImGui::GetContentRegionAvail().y-footer);
  ImGui::BeginChild(label,ImVec2(0,height),ImGuiChildFlags_NavFlattened,
                    ImGuiWindowFlags_AlwaysVerticalScrollbar);
}
}

#pragma once
#include <imgui.h>
#include <imgui_internal.h>

namespace pdz::ui {
// Focus a control for navigation without entering its numeric editor.
inline void FocusNavigationHere() {
  ImGui::SetKeyboardFocusHere();
  ImGui::GetCurrentContext()->NavMoveFlags &= ~ImGuiNavMoveFlags_Activate;
  ImGui::SetNavCursorVisible(true);
}
// Request ImGui's existing inline numeric editor before submitting the widget.
// The hit area is the slider frame, not the adjacent label.
inline void DoubleClickInput(const char* label) {
  auto& g = *ImGui::GetCurrentContext();
  auto* window = g.CurrentWindow;
  if (window->SkipItems || !ImGui::IsWindowHovered() ||
      !ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) return;
  const ImVec2 p = ImGui::GetCursorScreenPos();
  const ImRect frame(p, ImVec2(p.x + ImGui::CalcItemWidth(),
                             p.y + ImGui::GetFrameHeight()));
  if (!ImGui::IsMouseHoveringRect(frame.Min, frame.Max)) return;
  g.NavActivateId = window->GetID(label);
  g.NavActivateFlags = ImGuiActivateFlags_PreferInput;
}
inline bool SliderFloat(const char* label, float* value, float low, float high,
                        const char* format = "%.3f", ImGuiSliderFlags flags = 0) {
  DoubleClickInput(label);
  return ImGui::SliderFloat(label, value, low, high, format,
                           flags | ImGuiSliderFlags_AlwaysClamp);
}
inline bool SliderInt(const char* label, int* value, int low, int high,
                      const char* format = "%d", ImGuiSliderFlags flags = 0) {
  DoubleClickInput(label);
  return ImGui::SliderInt(label, value, low, high, format,
                         flags | ImGuiSliderFlags_AlwaysClamp);
}
}

#pragma once

#include <atomic>
#include <string_view>
#include <imgui.h>
#include <imgui_internal.h>

namespace pdz {
inline std::atomic<bool> ui_mouse_captured{false};
inline bool UiMouseCaptured() {
  return ui_mouse_captured.load(std::memory_order_acquire);
}

// Visibility, not hover, owns the mouse. WasActive covers dialogs drawn after
// the permanent PDZ overlay; closing another dialog releases on the next frame.
// The fallback window and noninteractive toasts/HUD must never capture input.
inline bool HasInteractiveUi(bool enhancements_open) {
  if (enhancements_open) return true;
  const auto* context = ImGui::GetCurrentContext();
  if (!context) return false;
  for (const auto* window : context->Windows) {
    const std::string_view root_name=(window->RootWindow?window->RootWindow:window)->Name;
    if (window->IsFallbackWindow || (window->Flags & ImGuiWindowFlags_NoInputs) ||
        root_name == "Perfect Dark Zero enhancements##pdz" ||
        root_name == "Perfect Dark Zero settings##pdz") continue;
    if (window->Active || window->WasActive) return true;
  }
  return false;
}
}

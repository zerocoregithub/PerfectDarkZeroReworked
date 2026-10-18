#include "../src/pdz_ui_mouse.h"
#include <cassert>
#include <cstdio>

static bool Frame(bool settings, bool log, bool enhancements, bool toast=false) {
  ImGui::NewFrame();
  if(settings) { ImGui::Begin("Settings##rex"); ImGui::TextUnformatted("settings"); ImGui::End(); }
  if(log) { ImGui::Begin("Console##rex"); ImGui::TextUnformatted("log"); ImGui::End(); }
  if(enhancements) {
    ImGui::Begin("Perfect Dark Zero settings##pdz");
    ImGui::BeginChild("settings_page");ImGui::TextUnformatted("tab contents");ImGui::EndChild();
    ImGui::End();
  }
  if(toast) { ImGui::Begin("##ach_toast", nullptr, ImGuiWindowFlags_NoInputs); ImGui::End(); }
  const bool captured=pdz::HasInteractiveUi(enhancements);
  ImGui::Render();
  return captured;
}
int main() {
  assert(!pdz::HasInteractiveUi(false));
  ImGui::CreateContext();
  auto& io=ImGui::GetIO();
  io.DisplaySize=ImVec2(1920,1080);
  io.DeltaTime=1.f/60;
  io.IniFilename=nullptr;
  unsigned char* pixels; int width,height;
  io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
  // Cursor deliberately away from every window: capture must not depend on hover.
  io.MousePos=ImVec2(1800,1000);
  assert(!Frame(false,false,false));
  assert(!Frame(false,false,false,true));
  assert(Frame(false,false,true));
  assert(!Frame(false,false,false)); // F8 close releases immediately.
  assert(Frame(true,true,false));
  assert(Frame(false,true,false)); // Closing one window retains capture.
  assert(Frame(false,false,false)); // Previous frame's SDK dialog, one-frame grace.
  assert(!Frame(false,false,false));
  pdz::ui_mouse_captured.store(true);
  assert(pdz::UiMouseCaptured());
  pdz::ui_mouse_captured.store(false);
  assert(!pdz::UiMouseCaptured());
  ImGui::DestroyContext();
  std::puts("PASS: off-window capture, F8 close, overlapping dialogs, last close, toast exclusion, ownership gate");
}

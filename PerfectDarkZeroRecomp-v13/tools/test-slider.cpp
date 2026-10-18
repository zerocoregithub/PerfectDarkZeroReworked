#include "../src/pdz_slider.h"
#include <cassert>
#include <iostream>
int main() {
  ImGui::CreateContext();
  auto& io=ImGui::GetIO();io.IniFilename=nullptr;io.DisplaySize={800,600};io.DeltaTime=1.f/60.f;
  unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
  float value=.5f;int integer=5;ImVec2 hit;
  auto frame=[&](bool down) {
    io.AddMousePosEvent(hit.x,hit.y);io.AddMouseButtonEvent(0,down);
    ImGui::NewFrame();ImGui::SetNextWindowPos({0,0});ImGui::SetNextWindowSize({600,400});
    ImGui::Begin("Test",nullptr,ImGuiWindowFlags_NoTitleBar);
    ImGui::SetNextItemWidth(300);
    auto p=ImGui::GetCursorScreenPos();hit={p.x+150,p.y+10};
    pdz::ui::SliderFloat("Float",&value,0,1);
    const bool editing=ImGui::TempInputIsActive(ImGui::GetID("Float"));
    pdz::ui::SliderInt("Integer",&integer,0,10);
    ImGui::End();ImGui::Render();return editing;
  };
  frame(false);frame(false);assert(!frame(true));frame(false);
  assert(frame(true)); // Double-click enters the existing inline text editor.
  frame(false);
  io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_A,true);frame(false);
  io.AddKeyEvent(ImGuiKey_A,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame(false);
  io.AddInputCharactersUTF8("999");frame(false);
  io.AddKeyEvent(ImGuiKey_Enter,true);frame(false);
  assert(value==1.f); // Typed input respects the slider bounds.
  ImGui::DestroyContext();std::cout<<"slider double-click and numeric clamp passed\n";
}

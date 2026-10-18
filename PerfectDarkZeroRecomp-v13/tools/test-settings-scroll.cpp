#include "../src/pdz_settings_layout.h"
#include <imgui_internal.h>
#include <cassert>
#include <iostream>
int main(){
 ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={1280,720};io.DeltaTime=1.f/60;io.IniFilename=nullptr;
 unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
 ImGuiID ids[2]{};bool reached_bottom=false;
 for(int frame=0;frame<9;++frame){
   if(frame==6)io.AddMousePosEvent(60,100);
   if(frame==7)io.AddMouseWheelEvent(0,-5);
   ImGui::NewFrame();ImGui::SetNextWindowSize({600,450});ImGui::SetNextWindowPos({20,20});
   ImGui::Begin("Settings",nullptr,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
   const int page=frame<4?0:1;pdz::ui::BeginSettingsPage(page?"Other":"Experimental");
   ids[page]=ImGui::GetCurrentWindow()->ID;
   for(int row=0;row<100;++row)ImGui::Text("Setting %d",row);
   if(frame==2){assert(ImGui::GetScrollMaxY()>1000);ImGui::SetScrollY(ImGui::GetScrollMaxY());}
   if(frame==3){assert(ImGui::GetScrollY()>1000);reached_bottom=true;}
   if(frame==5)assert(ImGui::GetScrollY()==0);
   if(frame==8)assert(ImGui::GetScrollY()>0);
   assert(ImGui::GetCurrentWindow()->Flags&ImGuiWindowFlags_AlwaysVerticalScrollbar);
   ImGui::EndChild();ImGui::TextUnformatted("Status footer");assert(ImGui::GetScrollY()==0);
   ImGui::End();ImGui::Render();
 }
 assert(ids[0]!=ids[1]&&reached_bottom);ImGui::DestroyContext();
 std::cout<<"PASS: long page reaches bottom, mouse wheel works, visible scrollbar, independent page scroll, fixed footer\n";
}

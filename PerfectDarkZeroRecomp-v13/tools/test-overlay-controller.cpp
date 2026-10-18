#include "../src/pdz_overlay_controller.h"
#include "../src/pdz_settings_layout.h"
#include "../src/pdz_slider.h"
#include <imgui_internal.h>
#include <cassert>
#include <iostream>

int main(){
 using namespace pdz::ui;
 OverlayShortcuts shortcut;
 auto event=shortcut.Update(kMenu,false,0,false);assert(event.buttons==0&&!event.overlay);
 event=shortcut.Update(kMenuChord,false,.08,false);assert(event.buttons==0&&event.overlay);
 event=shortcut.Update(kMenuChord,false,.5,true);assert(!event.overlay&&event.buttons==0);
 event=shortcut.Update(kView,false,.6,true);assert(!event.overlay&&event.buttons==0);
 shortcut.Update(0,false,.7,true);
 event=shortcut.Update(kMenuChord,false,.8,true);assert(event.overlay&&event.buttons==0);
 shortcut.Update(0,false,.9,false);
 event=shortcut.Update(kView,false,1,false);assert(event.buttons==0);
 event=shortcut.Update(0,false,1.1,false);assert(event.buttons==0&&!event.overlay);
 assert(shortcut.Update(0,false,1.26,false).buttons==kView);
 assert(shortcut.Update(0,false,1.30,false).buttons==kView);
 assert(shortcut.Update(0,false,1.40,false).buttons==0);
 shortcut.Update(kMenu,false,2,false);
 assert(shortcut.Update(kMenu,false,2.3,false).buttons==kMenu);
 shortcut.Update(0,false,2.4,false);
 assert(shortcut.Update(0,true,3,false).render);
 assert(!shortcut.Update(0,true,3.1,false).render);
 shortcut.Update(0,false,3.2,false);
 assert(shortcut.Update(0,true,3.3,false).render);
 // Start released after a quick tap still reaches the guest after the delay.
 OverlayShortcuts tap;
 assert(tap.Update(kMenu,false,0,false).buttons==0);
 assert(tap.Update(0,false,.02,false).buttons==0);
 for(int poll=0;poll<20;++poll)assert(tap.Update(0,false,.03+poll*.01,false).buttons==0);
 assert(tap.Update(0,false,.26,false).buttons==kMenu);
 assert(tap.Update(0,false,.30,false).buttons==kMenu);
 assert(tap.Update(0,false,.40,false).buttons==0);
 assert(tap.Update(0,false,2,false).buttons==0);
 assert(MenuAxis(.2f)==0&&MenuAxis(1)==1);
 assert(OverlayScale(0,720)==1&&OverlayScale(0,2160)==2.25f&&OverlayScale(2,1200)==2&&OverlayScale(2.5f,600)==1);

 ImGui::CreateContext();auto& io=ImGui::GetIO();io.DisplaySize={1280,720};io.DeltaTime=1.f/60;io.IniFilename=nullptr;
 io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad|ImGuiConfigFlags_NavEnableKeyboard;
 io.ConfigInputTrickleEventQueue=false;
 unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
 bool checked=false;float slider=.5f;int selection=0;ImGuiID check_id=0,slider_id=0,combo_id=0;
 bool first=true;float scroll=0;
 const auto frame=[&](uint16_t buttons=0,float lx=0,float ly=0){
   FeedOverlayPad(io,{buttons,lx,ly,0,0,true,false},true);ImGui::NewFrame();
   ImGui::SetNextWindowSize({600,450});ImGui::SetNextWindowPos({20,20});
   if(first)ImGui::SetNextWindowFocus();
   ImGui::Begin("Settings",nullptr,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);
   BeginSettingsPage("Controls");if(first){ImGui::SetKeyboardFocusHere();ImGui::SetNavCursorVisible(true);first=false;}
   ImGui::Checkbox("Toggle",&checked);check_id=ImGui::GetItemID();
   SliderFloat("Slider",&slider,0,1);slider_id=ImGui::GetItemID();
   const char* options[]={"Local","Player","World"};ImGui::Combo("Space",&selection,options,3);combo_id=ImGui::GetItemID();
   for(int row=0;row<80;++row){ImGui::PushID(row);ImGui::Button("Setting");ImGui::PopID();}
   scroll=ImGui::GetScrollY();ImGui::EndChild();ImGui::TextUnformatted("Controller legend");ImGui::End();ImGui::Render();
 };
 for(int i=0;i<5;++i)frame();
 assert(ImGui::GetCurrentContext()->NavId==check_id);
 frame(0x1000);frame();frame();assert(checked);
 frame(2);frame();frame();assert(ImGui::GetCurrentContext()->NavId==slider_id);
 frame(0x1000);frame();const float original=slider;
 for(int i=0;i<24;++i)frame(8);frame();assert(slider>original);
 frame(0x1000);frame();
 frame(2);frame();frame();assert(ImGui::GetCurrentContext()->NavId==combo_id);
 frame(0x1000);frame();frame();
 assert(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel));
 frame(2);frame();frame();frame(0x1000);frame();frame();assert(selection==1);
 assert(!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel));
 frame(0x1000);frame();frame();frame(0x2000);frame();frame();
 assert(!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel));
 for(int i=0;i<360;++i)frame(0,0,-1);frame();frame();assert(scroll>300);
 // Capture disabled clears held navigation on the next frame.
 FeedOverlayPad(io,{0x1000,1,1,0,0,true,false},false);ImGui::NewFrame();
 assert(!ImGui::IsKeyDown(ImGuiKey_GamepadFaceDown)&&!ImGui::IsKeyDown(ImGuiKey_GamepadLStickUp));ImGui::EndFrame();
 // Oversized manual scale must still leave a usable page and visible footer.
 for(const auto display:{ImVec2(800,600),ImVec2(1280,720),ImVec2(1920,1080),ImVec2(3840,2160)}){
   io.DisplaySize=display;const float scale=OverlayScale(2.5f,display.y);
   for(int pass=0;pass<3;++pass){
     ImGui::NewFrame();const auto saved=ImGui::GetStyle();auto& style=ImGui::GetStyle();style.ScaleAllSizes(scale);style.FramePadding.y=8*scale;style.ItemSpacing.y=8*scale;
     ImGui::SetNextWindowPos({16,16});ImGui::SetNextWindowSize({std::min(740*scale,display.x-32),std::min(640*scale,display.y-32)});
     ImGui::Begin("Scaled settings",nullptr,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse);ImGui::SetWindowFontScale(scale);
     ImGui::TextWrapped("F8 / View + Menu: open or close | Quick Access (...): GameCube look on/off");
     if(ImGui::BeginTabBar("Tabs")){if(ImGui::BeginTabItem("Controller")){
       BeginSettingsPage("Long page");for(int row=0;row<80;++row)ImGui::Button("Setting");
       assert(ImGui::GetWindowHeight()>=ImGui::GetFontSize()*4);ImGui::EndChild();ImGui::EndTabItem();}ImGui::EndTabBar();}
     ImGui::TextWrapped("D-pad: navigate | Select: edit | Back: close | LB / RB: tabs");
     ImGui::TextDisabled("Changes saved automatically");
     if(pass>0)assert(ImGui::GetItemRectMax().y<display.y-16);
     ImGui::End();ImGui::GetStyle()=saved;ImGui::Render();
   }
 }
 // Tab arrival on a slider focuses it without entering numeric edit mode.
 ImGuiID landing=0;
 for(int pass=0;pass<6;++pass){
   FeedOverlayPad(io,{0,0,0,0,0,true,false},true);ImGui::NewFrame();
   ImGui::SetNextWindowSize({600,450});if(pass==0)ImGui::SetNextWindowFocus();
   ImGui::Begin("Mouse settings regression");
   if(pass==0)FocusNavigationHere();
   SliderFloat("Mouse sensitivity",&slider,0,1);landing=ImGui::GetItemID();
   assert(!ImGui::IsAnyItemActive());
   ImGui::End();ImGui::Render();
 }
 assert(ImGui::GetCurrentContext()->NavId==landing);
 // Session controls live inside the page, not in an unreachable parent footer.
 io.DisplaySize={1280,720};bool arrived=true,opened=false,confirmed=false;
 ImGuiID restart_id=0,exit_id=0,cancel_id=0;
 const auto system_frame=[&](uint16_t buttons=0){
   FeedOverlayPad(io,{buttons,0,0,0,0,true,false},true);ImGui::NewFrame();
   ImGui::SetNextWindowSize({600,450});if(arrived)ImGui::SetNextWindowFocus();
   ImGui::Begin("System regression");BeginSettingsPage("System");
   if(arrived){FocusNavigationHere();arrived=false;}
   ImGui::Button("Restart Game");restart_id=ImGui::GetItemID();ImGui::SameLine();
   if(ImGui::Button("Exit Game")){opened=true;ImGui::OpenPopup("Leave game?");}
   exit_id=ImGui::GetItemID();
   if(ImGui::BeginPopupModal("Leave game?",nullptr,ImGuiWindowFlags_AlwaysAutoResize)){
     if(ImGui::Button("Cancel"))ImGui::CloseCurrentPopup();cancel_id=ImGui::GetItemID();ImGui::SetItemDefaultFocus();
     ImGui::SameLine();if(ImGui::Button("Confirm Exit")){confirmed=true;ImGui::CloseCurrentPopup();}
     ImGui::EndPopup();
   }
   ImGui::EndChild();ImGui::TextUnformatted("Saved");ImGui::End();ImGui::Render();
 };
 for(int pass=0;pass<6;++pass)system_frame();
 assert(ImGui::GetCurrentContext()->NavId==restart_id);
 system_frame(8);system_frame();system_frame();assert(ImGui::GetCurrentContext()->NavId==exit_id);
 system_frame(0x1000);system_frame();system_frame();assert(opened&&!confirmed);
 assert(ImGui::GetCurrentContext()->NavId==cancel_id);
 system_frame(0x1000);system_frame();system_frame();assert(!confirmed);
 assert(!ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel));
 system_frame(0x1000);system_frame();system_frame();
 system_frame(8);system_frame();system_frame();
 system_frame(0x1000);system_frame();system_frame();assert(confirmed);
 // Every page shares navigation with the persistent header Exit control.
 bool header_first=true;ImGuiID header_exit=0;bool header_opened=false;
 const auto header_frame=[&](uint16_t buttons=0){
   FeedOverlayPad(io,{buttons,0,0,0,0,true,false},true);ImGui::NewFrame();
   ImGui::SetNextWindowSize({600,450});if(header_first)ImGui::SetNextWindowFocus();
   ImGui::Begin("Persistent exit regression");
   ImGui::TextUnformatted("Settings");ImGui::SameLine();
   if(ImGui::SmallButton("Exit Game##quick_exit"))header_opened=true;
   header_exit=ImGui::GetItemID();
   BeginSettingsPage("Any tab");
   if(header_first){FocusNavigationHere();header_first=false;}
   ImGui::Button("First setting");ImGui::Button("Second setting");
   ImGui::EndChild();ImGui::TextUnformatted("Saved");
   ImGui::End();ImGui::Render();
 };
 for(int i=0;i<6;++i)header_frame();
 for(int i=0;i<6&&ImGui::GetCurrentContext()->NavId!=header_exit;++i){
   header_frame(1);header_frame();header_frame();
 }
 assert(ImGui::GetCurrentContext()->NavId==header_exit);
 header_frame(0x1000);header_frame();header_frame();assert(header_opened);
 ImGui::DestroyContext();
 std::cout<<"PASS: chord/tap replay, edge-only QAM, scaling, controller focus, checkbox, slider, combo, B cancel, stick scrolling and release\n";
}

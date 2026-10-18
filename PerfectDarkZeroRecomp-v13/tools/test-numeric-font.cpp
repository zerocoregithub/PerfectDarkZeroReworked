#include "../src/pdz_numeric_font.h"
#include <cassert>
#include <iostream>
int main(int argc,char** argv) {
  assert(argc==2);
  ImGui::CreateContext();auto& io=ImGui::GetIO();io.IniFilename=nullptr;
  io.DisplaySize={800,600};io.DeltaTime=1.f/60.f;
  io.Fonts->AddFontDefault();
  pdz::hud::NumericFont font;
  assert(!font.Load(io.Fonts,"does-not-exist.pdnf"));
  assert(font.Load(io.Fonts,argv[1]));assert(font.Ready());
  assert(font.Width(8)==3&&font.Width(106)==11);
  unsigned char* pixels;int width,height;io.Fonts->GetTexDataAsRGBA32(&pixels,&width,&height);
  // The font survives the backend's second atlas query/build, then draws quads.
  io.Fonts->SetTexID(1);
  ImGui::NewFrame();auto* draw=ImGui::GetBackgroundDrawList();
  assert(font.Draw(draw,8,{20,20},5,IM_COL32_WHITE));
  assert(font.Draw(draw,106,{20,60},5,IM_COL32_WHITE));
  assert(draw->VtxBuffer.Size==16&&draw->IdxBuffer.Size==24);
  // Check the actual source glyph at the UV location, not merely draw success.
  const auto uv=draw->VtxBuffer[0].uv;
  const int x=int(uv.x*width+.01f),y=int(uv.y*height+.01f);
  assert(pixels[4*(y*width+x)+3]==255); // top left ink of '8'
  assert(pixels[4*((y+8)*width+x+8)+3]==0); // hole in '8'
  ImGui::Render();ImGui::DestroyContext();
  std::cout<<"PASS: actual numeric font loading, width, atlas pixels and glyph draw\n";
}

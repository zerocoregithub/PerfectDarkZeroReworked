#pragma once
#include <imgui.h>
#include <array>
#include <filesystem>
#include <fstream>
#include <string>
#include <cstring>

namespace pdz::hud {
// Real ROM bitmap font, supplied locally by the user; never compiled into the app.
class NumericFont {
 public:
  bool Load(ImFontAtlas* atlas,const std::filesystem::path& path) {
    atlas_=nullptr;rect_=ImFontAtlasRectId_Invalid;
    std::ifstream file(path,std::ios::binary);
    std::array<unsigned char,198> bytes{};
    if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||file.peek()!=EOF||
       std::memcmp(bytes.data(),"PDNF0001",8))return false;
    for(int i=0;i<10;++i) {
      const int start=8+i*19;
      if(bytes[start]!=3||bytes[start+1]!=5||bytes[start+2]!=4||bytes[start+3]!=0)return false;
      for(int j=0;j<15;++j)if(bytes[start+4+j]!=0&&bytes[start+4+j]!=255)return false;
    }
    // Bake nearest-neighbour enlarged texels to avoid smoothing away 3x5 detail.
    // Use one custom atlas rectangle, querying its current UVs at draw time.
    ImFontAtlasRect area;
    rect_=atlas->AddCustomRect(10*cell_width*upscale,7*upscale,&area);
    if(rect_==ImFontAtlasRectId_Invalid)return false;
    unsigned char* pixels=nullptr;int width=0,height=0;
    atlas->GetTexDataAsRGBA32(&pixels,&width,&height);
    if(!pixels||!atlas->GetCustomRect(rect_,&area))return false;
    for(int y=0;y<7*upscale;++y)for(int x=0;x<10*cell_width*upscale;++x) {
      const int digit=x/(cell_width*upscale),gx=(x/upscale)%cell_width-1,gy=y/upscale-1;
      const unsigned char alpha=gx>=0&&gx<3&&gy>=0&&gy<5?bytes[8+digit*19+4+gy*3+gx]:0;
      auto* p=pixels+4*((int(area.y)+y)*width+int(area.x)+x);
      p[0]=p[1]=p[2]=255;p[3]=alpha;
    }
    atlas_=atlas;return true;
  }
  bool Ready() const {return atlas_!=nullptr;}
  float Width(uint32_t value) const {return float(std::to_string(value).size()*4-1);}
  bool Draw(ImDrawList* draw,uint32_t value,ImVec2 origin,float scale,ImU32 color) const {
    if(!atlas_)return false;
    ImFontAtlasRect area;if(!atlas_->GetCustomRect(rect_,&area))return false;
    const auto text=std::to_string(value);
    const float du=(area.uv1.x-area.uv0.x)/float(10*cell_width*upscale);
    const float dv=(area.uv1.y-area.uv0.y)/float(7*upscale);
    float x=origin.x;
    for(char ch:text) {
      const float u=area.uv0.x+float(((ch-'0')*cell_width+1)*upscale)*du;
      const float v=area.uv0.y+float(upscale)*dv;
      draw->AddImage(atlas_->TexRef,{x,origin.y},{x+3*scale,origin.y+5*scale},
                     {u,v},{u+3*upscale*du,v+5*upscale*dv},color);
      x+=4*scale;
    }
    return true;
  }
 private:
  static constexpr int upscale=8,cell_width=5;
  ImFontAtlas* atlas_=nullptr;
  ImFontAtlasRectId rect_=ImFontAtlasRectId_Invalid;
};
inline NumericFont numeric_font;
}

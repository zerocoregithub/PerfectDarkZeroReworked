#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace pdz::hud {
// Independent geometric reconstruction, informed by Dab's Mod healthbar.c and
// bondgun.c. Coordinates are logical units, not bitmap or game-memory offsets.
struct Point { float x, y; };
struct Color { uint8_t r, g, b, a; };
struct Vertex { Point point; Color color; };
struct Triangle { Vertex a, b, c; };
struct Mesh { std::vector<Triangle> triangles; };
inline float Fraction(float value) {
  return std::isfinite(value) ? std::clamp(value, 0.f, 1.f) : 0.f;
}
inline void Quad(Mesh& mesh, Point a, Point b, Point c, Point d, Color color) {
  mesh.triangles.push_back({{a,color},{b,color},{c,color}});
  mesh.triangles.push_back({{a,color},{c,color},{d,color}});
}
struct Marker { Point outer, inner; float fraction; };
inline Point Mix(Point a, Point b, float t) {
  return {a.x+(b.x-a.x)*t, a.y+(b.y-a.y)*t};
}
// Split the strip at the exact fill boundary; no per-pixel texture scaling.
inline void Ribbon(Mesh& mesh, const std::vector<Marker>& markers,
                   float value, Color fill, Color empty,float recovery=-1.f) {
  value=Fraction(value);
  recovery=recovery<0.f?value:std::max(value,Fraction(recovery));
  for (size_t i=1; i<markers.size(); ++i) {
    const auto& a=markers[i-1]; const auto& b=markers[i];
    std::vector<float> cuts{a.fraction,b.fraction};
    for(float boundary:{value,recovery})
      if(boundary>a.fraction&&boundary<b.fraction) cuts.push_back(boundary);
    std::sort(cuts.begin(),cuts.end());
    cuts.erase(std::unique(cuts.begin(),cuts.end()),cuts.end());
    for(size_t j=1;j<cuts.size();++j) {
      const float t0=(cuts[j-1]-a.fraction)/(b.fraction-a.fraction);
      const float t1=(cuts[j]-a.fraction)/(b.fraction-a.fraction);
      const float midpoint=(cuts[j-1]+cuts[j])*.5f;
      Color color=midpoint<=value?fill:empty;
      if(midpoint>value&&midpoint<=recovery) color={fill.r,fill.g,fill.b,uint8_t(fill.a/3)};
      Quad(mesh,Mix(a.outer,b.outer,t0),Mix(a.inner,b.inner,t0),
           Mix(a.inner,b.inner,t1),Mix(a.outer,b.outer,t1),color);
    }
  }
}
inline Mesh Health(float health, float shield,float recovery=-1.f) {
  Mesh mesh;
  const Color empty{0,0,0,128}, green{0,192,0,96}, red{255,0,0,96}, shield_color{16,80,0,144};
  const std::vector<Marker> main{
    {{47,12},{47,-12},0},{{170,12},{170,-12},.9f},
    {{174.1f,11.28f},{174.1f,-11.28f},.94f},
    {{177.72f,9.19f},{177.72f,-9.19f},.97f},
    {{180.39f,6},{180.39f,-6},.99f},{{181.82f,2.09f},{181.82f,-2.09f},1}};
  const std::vector<Marker> trauma{
    {{40,12},{40,-12},0},{{0,12},{0,-12},.8f},
    {{-4.6f,11.09f},{-4.6f,-11.09f},.85f},
    {{-8.48f,8.48f},{-8.48f,-8.48f},.9f},
    {{-11.09f,4.6f},{-11.09f,-4.6f},.95f},{{-12,0},{-12,0},1}};
  // Approximate the original split: lower quarter red, remaining health green.
  recovery=recovery<0.f?Fraction(health):std::max(Fraction(health),Fraction(recovery));
  Ribbon(mesh,main,(Fraction(health)-.25f)/.75f,green,empty,(recovery-.25f)/.75f);
  Ribbon(mesh,trauma,Fraction(health)/.25f,red,empty,recovery/.25f);
  std::vector<Marker> shields;
  constexpr float pi=3.14159265358979323846f;
  for(int i=0;i<=4;++i) {
    const float angle=float(i)*pi/8;
    shields.push_back({{170+32.4f*std::cos(angle),30*std::sin(angle)},
                       {170+18*std::cos(angle),18*std::sin(angle)},float(i)*.05f});
  }
  shields.push_back({{0,30},{0,18},.8f});
  for(int i=1;i<=4;++i) {
    const float angle=pi/2+float(i)*pi/8;
    shields.push_back({{32.4f*std::cos(angle),30*std::sin(angle)},
                       {18*std::cos(angle),18*std::sin(angle)},.8f+float(i)*.05f});
  }
  Ribbon(mesh,shields,shield,shield_color,empty);
  return mesh;
}
inline Mesh Ammo(int remaining, int capacity, float width=9, float height=57,
                 Color fill={0,192,0,160}) {
  Mesh mesh;
  if(capacity<=0||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0) return mesh;
  remaining=std::clamp(remaining,0,capacity);
  const Color empty{0,0,0,128};
  if(capacity<=20) {
    const float unit=height/float(capacity), gap=std::min(1.f,unit*.25f);
    for(int i=0;i<capacity;++i) {
      const float y=height-float(i+1)*unit;
      Quad(mesh,{0,y},{width,y},{width,y+unit-gap},{0,y+unit-gap},i<remaining?fill:empty);
    }
  } else {
    const float split=height*(1-float(remaining)/float(capacity));
    if(split>0) Quad(mesh,{0,0},{width,0},{width,split},{0,split},empty);
    if(split<height) Quad(mesh,{0,split},{width,split},{width,height},{0,height},fill);
  }
  return mesh;
}
// Reserve is always continuous, even for ammunition with a low carry limit.
// Unlike the magazine, the reserve fills from its top (as in PD's flipped gauge).
inline Mesh Reserve(uint32_t remaining,uint32_t capacity,float width=9,float height=36) {
  Mesh mesh;
  if(!capacity||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0) return mesh;
  const float split=height*Fraction(float(remaining)/float(capacity));
  const Color empty{0,48,40,128},fill{0,192,160,96};
  if(split>0) Quad(mesh,{0,0},{width,0},{width,split},{0,split},fill);
  if(split<height) Quad(mesh,{0,split},{width,split},{width,height},{0,height},empty);
  return mesh;
}
inline Mesh FunctionMarker(bool secondary) {
  Mesh mesh;const Color color=secondary?Color{224,184,0,128}:Color{224,0,0,128};
  Quad(mesh,{0,0},{11,0},{11,11},{0,11},color);return mesh;
}
inline Mesh Crosshair(float size,float gap,float thickness,Color color,float outline=0) {
  Mesh mesh;
  if(!std::isfinite(size)||!std::isfinite(gap)||!std::isfinite(thickness)||!std::isfinite(outline)) return mesh;
  size=std::clamp(size,0.f,32.f); gap=std::clamp(gap,0.f,32.f);
  thickness=std::clamp(thickness,1.f,8.f); outline=std::clamp(outline,0.f,4.f);
  const auto rect=[&](float x0,float y0,float x1,float y1,Color c,float border) {
    Quad(mesh,{x0-border,y0-border},{x1+border,y0-border},
              {x1+border,y1+border},{x0-border,y1+border},c);
  };
  const auto shape=[&](Color c,float border) {
    const float half=thickness*.5f;
    if(size==0.f) {rect(-half,-half,half,half,c,border);return;}
    rect(-gap-size,-half,-gap,half,c,border);
    rect(gap,-half,gap+size,half,c,border);
    rect(-half,-gap-size,half,-gap,c,border);
    rect(-half,gap,half,gap+size,c,border);
  };
  if(outline>0) shape({0,0,0,color.a},outline);
  shape(color,0);
  return mesh;
}
}

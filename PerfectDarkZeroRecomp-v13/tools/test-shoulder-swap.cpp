#include "../src/pdz_shoulder_swap.h"
#include <cassert>
#include <limits>
#include <cstdio>
#include <initializer_list>
int main(){
 using namespace pdz::input;
 {
  StickCameraShortcuts input;
  auto a=input.Update(128,1.,true,true);assert(!a.shoulder&&!a.perspective&&!(a.buttons&128));
  a=input.Update(192,1.05,true,true);assert(a.perspective&&!a.shoulder&&!(a.buttons&192));
  for(int i=1;i<40;++i){a=input.Update(192,1.05+i*.01,true,false);assert(!a.perspective&&!a.shoulder&&!(a.buttons&192));}
  input.Update(0,1.6,true,false);
  a=input.Update(192,1.7,true,false);assert(a.perspective&&!a.shoulder);
 }
 for(bool third:{false,true}) {
  StickCameraShortcuts input;
  auto a=input.Update(128,1.,true,third);assert(!a.shoulder&&!a.perspective);
  a=input.Update(0,1.02,true,third);assert(a.shoulder==third&&!a.perspective);
  if(!third){assert(a.buttons&128);a=input.Update(0,1.08,true,false);assert(a.buttons&128);}
  else assert(!(a.buttons&128));
 }
 {
  StickCameraShortcuts input;
  input.Update(64,1.,true,false);
  auto a=input.Update(0,1.02,true,false);assert((a.buttons&64)&&!a.perspective);
  a=input.Update(0,1.2,true,false);assert(!(a.buttons&192));
  input.Update(192,2.,false,false);
  a=input.Update(192,2.1,true,false);assert(!a.perspective);
 }
 for(int hz:{30,60,120,240}) {
  ShoulderTransition transition;assert(transition.Update(-80,0)==-80);
  double previous=-80;
  for(int i=0;i<hz/5;++i){const auto next=transition.Update(80,1./hz);assert(next>=previous&&next<=80);previous=next;}
  assert(std::abs(previous-80)<5);
  transition.Reset();assert(transition.Update(45,0)==45);
 }
 ShoulderSwapPress press;
 for(int hz:{30,60,120,240}) {
  ShoulderTransition zoom;assert(zoom.Update(1,0)==1);
  double previous=1;
  for(int i=0;i<hz/2;++i){double next=zoom.Update(0,1./hz);assert(next>=0&&next<=previous);previous=next;}
  assert(previous==0); // Native eye reached exactly, not an asymptotic offset.
  for(int i=0;i<hz/2;++i){double next=zoom.Update(1,1./hz);assert(next<=1&&next>=previous);previous=next;}
  assert(previous==1);
  zoom.Reset();assert(zoom.Update(0,0)==0); // Entering a new camera in ADS.
 }
 auto r=press.Update(true,false);assert(!r.toggle&&!r.consume);
 r=press.Update(true,true);assert(!r.toggle&&!r.consume); // No held entry.
 press.Update(false,true);
 r=press.Update(true,true);assert(r.toggle&&r.consume);
 for(int i=0;i<100;++i){r=press.Update(true,true);assert(!r.toggle&&r.consume);}
 r=press.Update(true,false);assert(!r.toggle&&r.consume); // Drain owned hold.
 press.Update(false,false);
 r=press.Update(true,false);assert(!r.toggle&&!r.consume); // First-person native.
 assert(OppositeShoulder(-80)==80&&OppositeShoulder(80)==-80);
 assert(OppositeShoulder(-45.5)==45.5&&OppositeShoulder(0)==-80);
 assert(std::isnan(OppositeShoulder(std::numeric_limits<double>::quiet_NaN())));
 puts("PASS: rising edge, held/mode-entry guards, native first-person passthrough, mirrored offsets");
}

#include "../src/pdz_input_math.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace pdz::input;
 assert(StickExponent("linear")==1&&StickExponent("relaxed")==2&&StickExponent("wide")==3&&StickExponent("extra_wide")==4);
 for(auto mode:{"linear","relaxed","wide","extra_wide"}){
   const double exponent=StickExponent(mode);
   assert(Curve({.08,0},.08,.98,exponent).x==0);
   assert(std::abs(Curve({.98,0},.08,.98,exponent).x-1)<1e-12);
   double last=0;for(int i=0;i<=100;++i){const auto value=Curve({i*.01,0},.08,.98,exponent);assert(value.x>=last);last=value.x;}
   const auto diagonal=Curve({.5,.5},.08,.98,exponent);assert(diagonal.x==diagonal.y);
   const auto negative=Curve({-.5,0},.08,.98,exponent);assert(negative.x<0);
 }
 const auto half=[&](auto mode){return Curve({.5,0},0,1,StickExponent(mode)).x;};
 assert(half("linear")==.5&&half("relaxed")==.25&&half("wide")==.125&&half("extra_wide")==.0625);
 assert(Curve({.5,0},.08,.98,true).x==Curve({.5,0},.08,.98,StickExponent("relaxed")).x);
 std::cout<<"PASS: stick curve progression, monotonicity, peak speed, deadzone, directions and Relaxed compatibility\n";
}

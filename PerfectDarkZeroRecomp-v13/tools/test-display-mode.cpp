#include "../src/pdz_display_mode.h"
#include <cassert>
#include <iostream>
int main(){
 using namespace pdz::ui;
 static_assert(DisplayMode(false,false)==0&&DisplayMode(false,true)==0);
 static_assert(DisplayMode(true,false)==1&&DisplayMode(true,true)==2);
 static_assert(NextDisplayMode(0)==1&&NextDisplayMode(1)==2&&NextDisplayMode(2)==0);
 for(int mode=0;mode<3;++mode)assert(DisplayMode(mode!=0,mode==2)==mode);
 std::cout<<"PASS: windowed/borderless/exclusive mapping and shortcut cycle\n";
}

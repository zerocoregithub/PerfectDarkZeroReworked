#include "../src/pdz_ab_timing.h"
#include <cstdlib>
int main(int argc,char** argv) {
  if(argc!=2)return 2;
  pdz::diagnostic::HudTiming timing;
  timing.Record(1000,5,false,false);
  timing.Record(2000,6,true,false);
  // Match the corrected sequencing: explicitly flush before non-unwinding exit.
  if(!timing.Export(std::filesystem::path(argv[1])))return 3;
  std::_Exit(0);
}

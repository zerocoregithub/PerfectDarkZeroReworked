#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "../src/pdz_first_run_setup.h"
#include <cassert>
#include <cstdio>
int main(int argc,char** argv) {
  assert(argc==2);
  pdz::setup::ValidateExtractedExecutable(std::filesystem::path(argv[1]));
  bool rejected=false;
  try{pdz::setup::ValidateExtractedExecutable(std::filesystem::path(argv[1]).parent_path()/"nonexistent-setup-test.xex");}catch(...){rejected=true;}
  assert(rejected);
  const auto wrong=std::filesystem::current_path()/("synthetic-wrong-executable-"+std::to_string(GetCurrentProcessId())+".bin");
  assert(!std::filesystem::exists(wrong));
  {std::ofstream output(wrong,std::ios::binary);output<<"synthetic non-game test input";}
  rejected=false;try{pdz::setup::ValidateExtractedExecutable(wrong);}catch(...){rejected=true;}assert(rejected);
  puts("PASS: Windows SHA-256 validates matched input, rejects missing/wrong executable; no GUI/game launch");
}

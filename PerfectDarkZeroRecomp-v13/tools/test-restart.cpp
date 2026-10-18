#include "../src/pdz_restart.h"
#include <cassert>
#include <fstream>
#include <cstdlib>
int main(int argc,char** argv) {
  pdz::HandleRestartHelper();
  assert(argc==2);
  if(std::getenv("PDZ_RESTART_TEST_CHILD")) {
    // Child gets the exact original argument; parent must have exited first.
    const DWORD parent=DWORD(std::strtoul(std::getenv("PDZ_RESTART_TEST_PARENT"),nullptr,10));
    HANDLE process=OpenProcess(SYNCHRONIZE,FALSE,parent);
    if(process){assert(WaitForSingleObject(process,0)==WAIT_OBJECT_0);CloseHandle(process);}
    std::ofstream marker(argv[1]); marker << "PASS: relaunch after parent exit, original arguments preserved";
    return marker?0:1;
  }
  SetEnvironmentVariableA("PDZ_RESTART_TEST_CHILD","1");
  SetEnvironmentVariableA("PDZ_RESTART_TEST_PARENT",std::to_string(GetCurrentProcessId()).c_str());
  std::string error;
  assert(pdz::StartRestartHelper(error));
  return 0;
}

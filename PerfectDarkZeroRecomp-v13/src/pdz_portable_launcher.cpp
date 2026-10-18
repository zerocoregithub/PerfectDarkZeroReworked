#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include "pdz_portable_launch.h"
#include "pdz_first_run_setup.h"

int WINAPI wWinMain(HINSTANCE,HINSTANCE,PWSTR,int) {
  wchar_t module[32768]{};
  const auto size=GetModuleFileNameW(nullptr,module,DWORD(std::size(module)));
  if(!size||size>=std::size(module))return 2;
  try {
    const auto runtime=pdz::portable::Runtime(std::filesystem::path(module));
    const auto data=runtime.parent_path();
    if(!std::filesystem::is_regular_file(runtime)) {
      MessageBoxW(nullptr,L"The game runtime is missing from the Data folder. Please restore the complete portable package.",
                  L"Perfect Dark Zero Reworked",MB_OK|MB_ICONERROR);
      return 3;
    }
    // One setup/game session per portable directory, across all launcher names.
    const auto lock_path=data/L"portable-session.lock";
    HANDLE session=CreateFileW(lock_path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(session==INVALID_HANDLE_VALUE)throw std::runtime_error("This portable folder is already in use, or is not writable.");
    struct Session {HANDLE h;~Session(){CloseHandle(h);}} session_guard{session};
    if(!pdz::setup::EnsureGameData(data))return 0;
    auto command=pdz::portable::Command(runtime,GetCommandLineW());
    STARTUPINFOW startup{};startup.cb=sizeof(startup);
    PROCESS_INFORMATION child{};
    if(!CreateProcessW(runtime.c_str(),command.data(),nullptr,nullptr,FALSE,0,
                       nullptr,data.c_str(),&startup,&child)) {
      const auto message=L"Could not start the game (Windows error "+std::to_wstring(GetLastError())+L").";
      MessageBoxW(nullptr,message.c_str(),L"Perfect Dark Zero Reworked",MB_OK|MB_ICONERROR);
      return 4;
    }
    CloseHandle(child.hThread);
    // Keep launcher-based sessions alive while the actual game is running.
    const auto wait=WaitForSingleObject(child.hProcess,INFINITE);
    DWORD result=5;
    if(wait==WAIT_OBJECT_0)GetExitCodeProcess(child.hProcess,&result);
    CloseHandle(child.hProcess);
    return int(result);
  } catch(const std::exception& e) {
    const std::string error=e.what();
    const std::wstring message(error.begin(),error.end());
    MessageBoxW(nullptr,message.c_str(),L"Perfect Dark Zero Reworked — setup",MB_OK|MB_ICONERROR);
    return 6;
  } catch(...) {
    MessageBoxW(nullptr,L"The portable game folder could not be opened.",L"Perfect Dark Zero Reworked",MB_OK|MB_ICONERROR);
    return 6;
  }
}

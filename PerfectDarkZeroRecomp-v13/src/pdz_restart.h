#pragma once
#include <string>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace pdz {
inline bool StartRestartHelper(std::string& error) {
  wchar_t path[32768]{};
  const DWORD size = GetModuleFileNameW(nullptr, path, DWORD(std::size(path)));
  if (!size || size >= std::size(path)) { error = "Cannot locate game executable."; return false; }
  std::wstring command = L"\"" + std::wstring(path) + L"\" --pdz-restart-wait=" +
      std::to_wstring(GetCurrentProcessId()) + L" " + GetCommandLineW();
  STARTUPINFOW startup{}; startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (!CreateProcessW(path, command.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
    error = "Cannot prepare restart (Windows error " + std::to_string(GetLastError()) + ").";
    return false;
  }
  CloseHandle(process.hThread); CloseHandle(process.hProcess);
  return true;
}

// Called before creating the runtime. The helper never loads or runs guest code.
inline void HandleRestartHelper() {
  const std::wstring command = GetCommandLineW();
  // Skip the executable token; do not treat text inside its path as an option.
  const size_t start = command.front() == L'"' ? command.find(L'"', 1) + 1 : command.find(L' ');
  const std::wstring marker = L" --pdz-restart-wait=";
  if (start == std::wstring::npos || command.compare(start, marker.size(), marker) != 0) return;
  const wchar_t* value = command.c_str() + start + marker.size();
  wchar_t* end = nullptr;
  const unsigned long parent = wcstoul(value, &end, 10);
  if (!parent || end == value || *end != L' ' || !end[1]) ExitProcess(2);
  HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(parent));
  if (process) {
    const DWORD result = WaitForSingleObject(process, 60000);
    CloseHandle(process);
    if (result != WAIT_OBJECT_0) ExitProcess(3); // Never start a second game over a hung one.
  } else if (GetLastError() != ERROR_INVALID_PARAMETER) ExitProcess(4);
  wchar_t path[32768]{};
  if (!GetModuleFileNameW(nullptr, path, DWORD(std::size(path)))) ExitProcess(5);
  std::wstring original = end + 1;
  STARTUPINFOW startup{}; startup.cb = sizeof(startup);
  PROCESS_INFORMATION child{};
  const BOOL launched = CreateProcessW(path, original.data(), nullptr, nullptr, FALSE,
                                      0, nullptr, nullptr, &startup, &child);
  if (launched) { CloseHandle(child.hThread); CloseHandle(child.hProcess); }
  ExitProcess(launched ? 0 : 6);
}
}
#else
namespace pdz {
inline bool StartRestartHelper(std::string& error) { error="Restart is currently supported on Windows only."; return false; }
inline void HandleRestartHelper() {}
}
#endif

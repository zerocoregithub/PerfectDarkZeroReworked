#pragma once
#include <filesystem>
#include <string>
#include <string_view>

namespace pdz::portable {
inline std::filesystem::path Runtime(const std::filesystem::path& launcher) {
  return launcher.parent_path() / L"Data" / L"perfectdarkzerorecomp.exe";
}
// Skip only argv[0], preserving the caller's argument quoting and Unicode.
inline std::wstring_view Arguments(std::wstring_view command) {
  auto pos=command.find_first_not_of(L" \t");
  if(pos==command.npos)return {};
  if(command[pos]==L'"') {
    pos=command.find(L'"',pos+1);
    if(pos==command.npos)return {};
    ++pos;
  } else {
    pos=command.find_first_of(L" \t",pos);
    if(pos==command.npos)return {};
  }
  pos=command.find_first_not_of(L" \t",pos);
  return pos==command.npos?std::wstring_view{}:command.substr(pos);
}
inline std::wstring Command(const std::filesystem::path& runtime,std::wstring_view original) {
  std::wstring result=L"\""+runtime.wstring()+L"\"";
  const auto args=Arguments(original);
  if(!args.empty()){result+=L' ';result+=args;}
  return result;
}
}

#include "../src/pdz_portable_launch.h"
#include <cassert>
#include <cstdio>

int main() {
  using namespace pdz::portable;
  const std::filesystem::path root=LR"(C:\Games\Moved Folder\PerfectDarkZeroReworked-20261014.exe)";
  const auto runtime=Runtime(root);
  assert(runtime==root.parent_path()/L"Data"/L"perfectdarkzerorecomp.exe");
  assert(Arguments(LR"("C:\Games\Some Folder\launch.exe" --flag "two words" "møtion")")==LR"(--flag "two words" "møtion")");
  assert(Arguments(L"launch.exe\t--flag value")==L"--flag value");
  assert(Arguments(L"launch.exe").empty());
  assert(Arguments(L"  \"launch.exe\"  ").empty());
  assert(Command(runtime,L"launch.exe --test")==L"\""+runtime.wstring()+L"\" --test");
  puts("PASS: executable-relative Data path, spaces/Unicode and original argument quoting preserved.");
}

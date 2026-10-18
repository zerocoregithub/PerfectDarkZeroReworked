// Exercises the actual imported implementation, no UI/game/devices opened.
#include "../src/local_profiles.cpp"
#include <cassert>
#include <iostream>

extern "C" void __imp__sub_82ACA290(PPCContext& ctx, uint8_t*) {
  ctx.r3.u64 = 0; // successful XamUserGetName for the isolated hook check
}
static std::string Read(const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file), {}};
}
int main(int argc, char** argv) {
  assert(argc == 2);
  const auto root=std::filesystem::absolute(argv[1]);
  assert(!std::filesystem::exists(root));
  std::filesystem::create_directories(root);
  { std::ofstream save(root/"existing-save"); save << "keep"; }
  assert(local_profiles::Load(root)==root);
  assert(!std::filesystem::exists(root/"local_profiles.toml"));
  assert(g_running_name=="User");
  assert(Commit({{0,"User"},{1,"Tester"}},1));
  assert(local_profiles::Load(root)==root/"profiles"/"1");
  assert(g_running_name=="Tester");
  assert(Read(root/"existing-save")=="keep");
  assert(CheckName(g_profiles,"../bad",2));
  assert(CheckName(g_profiles,"tester",2));
  assert(CheckName(g_profiles," bad",2));
  assert(CheckName(g_profiles,"1234567890123456",2));
  assert(!CheckName(g_profiles,"Valid-2",2));
  assert(Commit({{0,"User"},{1,"Renamed"}},1));
  assert(local_profiles::Load(root)==root/"profiles"/"1");
  assert(g_running_name=="Renamed");
  uint8_t memory[128]{};
  PPCContext ctx{};
  ctx.r3.u64=0; ctx.r4.u64=64; ctx.r5.u64=4;
  sub_82ACA290(ctx,memory);
  assert(std::string(reinterpret_cast<char*>(memory+64))=="Ren");
  assert(memory[68]==0);
  assert(Commit(g_profiles,0));
  assert(local_profiles::Load(root)==root);
  {std::ofstream malformed(root/"local_profiles.toml"); malformed<<"invalid = [";}
  const auto invalid=Read(root/"local_profiles.toml");
  assert(local_profiles::Load(root)==root);
  assert(!g_load_error.empty());
  assert(Read(root/"local_profiles.toml")==invalid);
  assert(Read(root/"existing-save")=="keep");
  std::cout << "PASS: default root, original save preservation, create/select/rename/reload, validation, name truncation and malformed-file fallback.\n";
}

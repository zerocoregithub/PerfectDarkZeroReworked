#include <filesystem>
#include <fstream>
#include <cstdio>
#include <rex/runtime.h>
#include <rex/kernel/init.h>
#include <rex/system/user_module.h>
#include <rex/system/xex_module.h>
int main(int argc,char** argv) {
  if(argc!=3) return 2;
  const auto root=std::filesystem::absolute(argv[1]);
  const auto output=std::filesystem::absolute(argv[2]);
  std::filesystem::create_directories(output);
  rex::Runtime runtime(root,output/"user",{},output/"cache",output/"metadata");
  if(runtime.Setup(rex::RuntimeConfig{.kernel_init=rex::kernel::InitializeKernel,.tool_mode=true})!=0) return 3;
  if(runtime.LoadXexImage("game:\\default.xex")!=0) return 4;
  // No Launch/PrepareLaunch/guest entrypoint invocation.
  auto module=runtime.kernel_state()->GetExecutableModule();
  auto* xex=module->xex_module();
  const auto base=xex->base_address(),size=xex->image_size();
  std::ofstream image(output/"default-image.bin",std::ios::binary);
  image.write(reinterpret_cast<const char*>(xex->memory()->TranslateVirtual(base)),size);
  if(!image) return 5;
  std::printf("Loaded offline: base=%08X bytes=%u; no guest execution\n",base,size);
  return 0;
}

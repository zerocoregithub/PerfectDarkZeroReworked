#include "../src/pdz_iso_setup.h"
#include <cassert>
#include <cstdio>
#include <chrono>
using namespace pdz::setup;
int main(int argc,char** argv) {
  if(argc==2) {
    std::ifstream f(fs::path(argv[1]),std::ios::binary);assert(f);
    uint32_t crc=~0u;char c;
    while(f.get(c)){crc^=uint8_t(c);for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xEDB88320u&uint32_t(-int(crc&1)));}
    std::printf("Local XEX CRC32: %08X\n",~crc);return 0;
  }
  for(auto bad:{"..",".","a/b","a\\b","C:evil","NUL.txt","COM1","end.","end ","a*"})assert(!SafeName(bad));
  for(auto good:{"default.xex","ASSETS","data.pkg","$SystemUpdate"})assert(SafeName(good));
  const auto root=fs::current_path()/("synthetic-iso-tests-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  if(fs::exists(root))throw std::runtime_error("Test fixture destination must be new.");
  fs::create_directory(root);
  std::vector<uint8_t> bytes(0x20000);
  auto put=[&](size_t at,uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)bytes.at(at+i)=uint8_t(v>>(i*8));};
  const std::string magic="MICROSOFT*XBOX*MEDIA";
  std::copy(magic.begin(),magic.end(),bytes.begin()+0x10000);
  std::copy(magic.begin(),magic.end(),bytes.begin()+0x10800-20);
  put(0x10000+20,40,4);put(0x10000+24,256,4);
  auto entry=[&](unsigned at,std::string name,unsigned sector=50,unsigned size=9,unsigned flags=0) {
    put(at,0,2);put(at+2,0,2);put(at+4,sector,4);put(at+8,size,4);put(at+12,flags,1);put(at+13,unsigned(name.size()),1);
    std::copy(name.begin(),name.end(),bytes.begin()+at+14);
  };
  entry(40*2048,"sample.bin");
  std::copy_n("123456789",9,bytes.begin()+50*2048);
  unsigned sequence=0;
  auto save=[&]{const auto path=root/(std::to_string(sequence++)+".iso");std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(bytes.data()),bytes.size());out.close();return path;};
  const auto good=save();Image image(good);auto plan=image.Inspect();
  assert(plan.files.size()==1&&plan.bytes==9&&image.Crc(plan.files[0])==0xcbf43926);
  image.Extract(plan,root/"extracted",{});
  assert(fs::file_size(root/"extracted/sample.bin")==9);
  bool failed=false;try{Validate(plan,image);}catch(...){failed=true;}assert(failed);
  auto rejects=[&]{bool rejected=false;try{Image bad(save());bad.Inspect();}catch(...){rejected=true;}assert(rejected);};
  entry(40*2048,"../escape");rejects();
  entry(40*2048,"NUL.txt");rejects();
  entry(40*2048,"too-far",0xffffffff,9);rejects();
  entry(40*2048,"cycle",40,256,0x10);rejects();
  entry(40*2048,"sample.bin");put(40*2048+2,8,2);entry(40*2048+32,"SAMPLE.BIN");rejects();
  entry(40*2048,"sample.bin");put(40*2048+2,0xffff,2);rejects();
  failed=false;try{Image cancel(good);cancel.Inspect([]{return true;});}catch(...){failed=true;}assert(failed);
  failed=false;try{image.Extract(plan,root/"extracted",{});}catch(...){failed=true;}assert(failed);
  failed=false;try{image.Extract(plan,root/"cancelled",[](uint64_t,uint64_t){return true;});}catch(...){failed=true;}assert(failed);
  bytes.resize(64);rejects();
  puts("PASS: synthetic ISO parsing/CRC/extraction; wrong game, traversal, reserved names, extents, cycles, case collisions, invalid pointers, truncation, cancellation and overwrite rejection");
}

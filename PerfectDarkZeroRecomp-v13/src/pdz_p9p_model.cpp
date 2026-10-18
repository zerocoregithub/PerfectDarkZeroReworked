#include "pdz_p9p_model.h"
#include <rex/cvar.h>
#include <rex/runtime.h>
#include <rex/memory.h>
#include <rex/memory/utils.h>
#include <array>
#include <atomic>
#include <cstring>
namespace {
std::array<std::atomic<uint64_t>,2> targets{};
thread_local bool hide_groups=false;
bool Readable(uint8_t* base,uint32_t p,uint32_t n) {
 if(!base||p<0x10000||p>UINT32_MAX-n)return false;
 size_t size=n;auto access=rex::memory::PageAccess::kNoAccess;
 return rex::memory::QueryProtect(base+p,size,access)&&size>=n&&
  (access==rex::memory::PageAccess::kReadOnly||access==rex::memory::PageAccess::kReadWrite||
   access==rex::memory::PageAccess::kExecuteReadOnly||access==rex::memory::PageAccess::kExecuteReadWrite);
}
uint32_t Word(uint8_t* base,uint32_t p){return rex::memory::load_and_swap<uint32_t>(base+p);}
uint64_t Key(uint32_t descriptor,uint32_t instance){return (uint64_t(descriptor)<<32)|instance;}
std::array<uint64_t,2> ReadTargets(uint8_t* base) {
 std::array<uint64_t,2> found{};
 if(!Readable(base,0x82d68320,4)||!Readable(base,0x82e35468,4))return found;
 auto manager=Word(base,0x82d68320),root=Word(base,0x82e35468);
 if(!Readable(base,manager,0x540)||!Readable(base,root,0x3bc))return found;
 auto camera=Word(base,manager+0x53c),player=Word(base,root+0x3b8);
 if(!Readable(base,camera,12)||base[camera+11]||!Readable(base,player,0x2000)||
    Word(base,player)!=0x820c93a0||base[player+0x16a3])return found;
 for(unsigned hand=0;hand<2;++hand) {
  auto item=Word(base,player+4076+hand*4);
  if(!Readable(base,item,1100)||Word(base,item)!=0x820c36d0||Word(base,item+92)!=player)continue;
  auto instance=Word(base,item+72);
  if(!Readable(base,instance,8))continue;
  auto resource=Word(base,instance);
  if(!Readable(base,resource,8))continue;
  constexpr char name[]="aid_scenegraph_guns_waltherp99_fp_waltherp99";
  auto address=Word(base,resource),descriptor=Word(base,resource+4);
  if(!descriptor||!Readable(base,address,sizeof(name))||std::memcmp(base+address,name,sizeof(name)))continue;
  found[hand]=Key(descriptor,instance);
 }
 return found;
}
void UpdateTargets(uint8_t* base,bool enabled) {
 auto found=enabled?ReadTargets(base):std::array<uint64_t,2>{};
 for(unsigned i=0;i<2;++i)targets[i].store(found[i],std::memory_order_release);
}
bool Eligible(uint8_t* base,uint32_t descriptor,uint32_t instance,uint32_t caller) {
 // Only this caller is verified for the local first-person instance.
 // Unrelated submissions return before any protection query or guest read.
 if(caller!=0x82407058||!descriptor||!instance)return false;
 auto key=Key(descriptor,instance);
 if(key!=targets[0].load(std::memory_order_acquire)&&key!=targets[1].load(std::memory_order_acquire))return false;
 // Revalidate against live equipment/camera state, never trust a cached pointer.
 auto live=ReadTargets(base);
 return key==live[0]||key==live[1];
}
}
void PDZ_UpdateP9PModel() {
 bool enabled=rex::cvar::Query<bool>("pdz_p9p_hide_attachments")&&!rex::cvar::Query<bool>("pdz_tp_enable");
 auto runtime=rex::Runtime::instance();
 UpdateTargets(runtime?runtime->virtual_membase():nullptr,enabled);
}
PDZ_P9PModelScope::PDZ_P9PModelScope(uint8_t* base,uint32_t descriptor,uint32_t instance,uint32_t caller):previous_(hide_groups) {
 hide_groups=Eligible(base,descriptor,instance,caller);
}
PDZ_P9PModelScope::~PDZ_P9PModelScope(){hide_groups=previous_;}
bool PDZ_P9PGroupHidden(uint32_t group){return hide_groups&&group<3;}

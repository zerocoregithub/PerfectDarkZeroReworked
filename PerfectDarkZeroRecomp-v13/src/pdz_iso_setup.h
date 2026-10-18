#pragma once
// XDVDFS layout references: XboxDev/extract-xiso (volume header, partition
// offsets and directory entry format). Independent bounded read-only parser.
#include <algorithm>
#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>
namespace pdz::setup {
namespace fs=std::filesystem;
struct Entry {fs::path path;uint64_t offset,size;};
struct Plan {std::vector<Entry> files;uint64_t bytes=0;};
inline uint32_t LE(const uint8_t* p,unsigned n) {
  uint32_t v=0;for(unsigned i=0;i<n;++i)v|=uint32_t(p[i])<<(8*i);return v;
}
inline std::string Lower(std::string s) {
  for(auto& c:s)if(c>='A'&&c<='Z')c+=32;return s;
}
inline bool SafeName(const std::string& s) {
  if(s.empty()||s=="."||s==".."||s.back()=='.'||s.back()==' ')return false;
  for(unsigned char c:s)if(c<32||c>126||std::string("<>:\"/\\|?*").find(c)!=std::string::npos)return false;
  const auto stem=Lower(s.substr(0,s.find('.')));
  if(stem=="con"||stem=="prn"||stem=="aux"||stem=="nul")return false;
  if(stem.size()==4&&(stem.substr(0,3)=="com"||stem.substr(0,3)=="lpt")&&stem[3]>='0'&&stem[3]<='9')return false;
  return true;
}
class Image {
  std::ifstream file_;uint64_t size_=0,base_=0;
  void Read(uint64_t at,void* out,size_t size) {
    if(at>size_||size>size_-at)throw std::runtime_error("ISO has an out-of-range extent.");
    file_.clear();file_.seekg(std::streamoff(at));
    if(!file_.read(static_cast<char*>(out),std::streamsize(size)))throw std::runtime_error("ISO read failed or image is truncated.");
  }
 public:
  explicit Image(const fs::path& path):file_(path,std::ios::binary) {
    if(!file_)throw std::runtime_error("Cannot open the selected ISO.");
    file_.seekg(0,std::ios::end);auto n=file_.tellg();
    if(n<0)throw std::runtime_error("Cannot determine ISO size.");size_=uint64_t(n);
  }
  Plan Inspect(const std::function<bool()>& cancelled={}) {
    std::array<uint8_t,2048> header{};bool found=false;
    for(uint64_t base:{0ull,0x0FD90000ull,0x02080000ull,0x18300000ull}) {
      if(base+0x10800>size_)continue;
      Read(base+0x10000,header.data(),header.size());
      constexpr char magic[]="MICROSOFT*XBOX*MEDIA";
      if(std::equal(header.begin(),header.begin()+20,magic)&&std::equal(header.end()-20,header.end(),magic)) {base_=base;found=true;break;}
    }
    if(!found)throw std::runtime_error("No supported Xbox disc filesystem found. Select a decrypted XDVDFS ISO from your own disc.");
    struct Dir {uint32_t sector,size;fs::path path;unsigned depth;};
    std::vector<Dir> dirs={{LE(header.data()+20,4),LE(header.data()+24,4),{},0}};
    std::set<uint64_t> directory_extents;std::set<std::string> paths;Plan plan;
    size_t entry_count=0;
    while(!dirs.empty()) {
      if(cancelled&&cancelled())throw std::runtime_error("Setup cancelled. Existing files are unchanged.");
      auto dir=dirs.back();dirs.pop_back();
      if(dir.depth>32||dir.size>16*1024*1024)throw std::runtime_error("ISO directory limits exceeded.");
      if(!dir.size)continue;
      const uint64_t at=base_+uint64_t(dir.sector)*2048;
      if(!directory_extents.insert(at).second)throw std::runtime_error("ISO directory cycle or alias rejected.");
      std::vector<uint8_t> bytes(dir.size);Read(at,bytes.data(),bytes.size());
      std::vector<uint32_t> nodes={0};std::set<uint32_t> visited;
      while(!nodes.empty()) {
        auto node=nodes.back();nodes.pop_back();
        if(!visited.insert(node).second||node>bytes.size()||bytes.size()-node<14)throw std::runtime_error("Invalid ISO directory tree.");
        const auto* p=bytes.data()+node;
        if(LE(p,2)==0xffff)continue;
        if(++entry_count>100000)throw std::runtime_error("ISO has too many entries.");
        for(unsigned side:{0u,2u}){auto next=LE(p+side,2);if(next)nodes.push_back(next*4);}
        const unsigned length=p[13];
        if(length>bytes.size()-node-14)throw std::runtime_error("Truncated ISO filename.");
        const std::string name(reinterpret_cast<const char*>(p+14),length);
        if(!SafeName(name))throw std::runtime_error("Unsafe or unsupported ISO filename rejected.");
        auto path=dir.path/name;
        if(path.generic_string().size()>220||!paths.insert(Lower(path.generic_string())).second)throw std::runtime_error("Duplicate or excessively long ISO path.");
        const auto sector=LE(p+4,4),size=LE(p+8,4);
        const uint64_t offset=base_+uint64_t(sector)*2048;
        if(offset>size_||size>size_-offset)throw std::runtime_error("Invalid ISO file extent.");
        if(p[12]&0x10)dirs.push_back({sector,size,path,dir.depth+1});
        else {plan.files.push_back({path,offset,size});plan.bytes+=size;}
        if(plan.bytes>16ull*1024*1024*1024)throw std::runtime_error("ISO extraction size limit exceeded.");
      }
    }
    return plan;
  }
  uint32_t Crc(const Entry& entry,const std::function<bool()>& cancelled={}) {
    std::array<uint8_t,65536> bytes{};uint32_t crc=~0u;
    for(uint64_t done=0;done<entry.size;) {
      if(cancelled&&cancelled())throw std::runtime_error("Setup cancelled.");
      auto n=size_t(std::min<uint64_t>(bytes.size(),entry.size-done));Read(entry.offset+done,bytes.data(),n);
      for(size_t i=0;i<n;++i){crc^=bytes[i];for(int bit=0;bit<8;++bit)crc=(crc>>1)^(0xEDB88320u&uint32_t(-int(crc&1)));}
      done+=n;
    }return ~crc;
  }
  void Extract(const Plan& plan,const fs::path& fresh,const std::function<bool(uint64_t,uint64_t)>& progress) {
    if(!fs::create_directory(fresh))throw std::runtime_error("Setup staging folder must be new.");
    std::vector<char> buffer(1024*1024);uint64_t copied=0;
    for(const auto& entry:plan.files) {
      if(progress&&progress(copied,plan.bytes))throw std::runtime_error("Setup cancelled; partial staging retained for inspection.");
      const auto target=fresh/entry.path;
      fs::create_directories(target.parent_path());
      if(fs::exists(target))throw std::runtime_error("Setup refuses to overwrite a file.");
      std::ofstream output(target,std::ios::binary);
      if(!output)throw std::runtime_error("Cannot write extracted game data.");
      for(uint64_t done=0;done<entry.size;) {
        if(progress&&progress(copied,plan.bytes))throw std::runtime_error("Setup cancelled; partial staging retained for inspection.");
        auto n=size_t(std::min<uint64_t>(buffer.size(),entry.size-done));Read(entry.offset+done,buffer.data(),n);
        output.write(buffer.data(),std::streamsize(n));if(!output)throw std::runtime_error("Writing game data failed (check free space).");
        done+=n;copied+=n;
      }
      output.close();if(!output)throw std::runtime_error("Closing extracted file failed.");
    }
  }
};
inline void Validate(const Plan& plan,Image& image,const std::function<bool()>& cancelled={}) {
  const Entry* xex=nullptr;bool packages=false;
  for(const auto& file:plan.files) {
    auto path=Lower(file.path.generic_string());
    if(path=="default.xex")xex=&file;
    if(path.starts_with("pdz/assets/packagedata/")&&path.ends_with(".pkg"))packages=true;
  }
  if(!xex||!packages)throw std::runtime_error("This ISO does not contain the required Perfect Dark Zero layout.");
  // Full-file CRC of the input used by the accepted build. The upstream README's
  // 375EC9BB is not this file's CRC; do not silently accept an unverified variant.
  if(image.Crc(*xex,cancelled)!=0x66DC74B6u)throw std::runtime_error("Unsupported or modified Perfect Dark Zero executable. The selected ISO does not match this test build's verified disc executable.");
}
}

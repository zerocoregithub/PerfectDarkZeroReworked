// Read-only cached guest shaders; no game launch, hook, or cache modification.
#include "test-vulkan-scene-readback.h"
#include <rex/graphics/pipeline/shader/spirv.h>
#include <rex/graphics/pdz_scene_grade.h>
#include <rex/string/buffer.h>
#include <xxhash.h>
#include <spirv-tools/libspirv.hpp>
#include <filesystem>
#include <fstream>
#include <cstdio>
#include <stdexcept>
#include <rex/graphics/pdz_swap_grade_glsl.h>
#include <glslang/Public/ShaderLang.h>
#include <rex/graphics/pipeline/shader/dxbc.h>
#include <rex/graphics/pdz_stylized.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <map>
#include <regex>
#include <SPIRV/GlslangToSpv.h>

class InspectDxbc final : public rex::graphics::DxbcShaderTranslator {
 public:
  InspectDxbc(bool rov):DxbcShaderTranslator(rex::ui::GraphicsProvider::GpuVendorID::kAMD,false,rov) {}
  unsigned normals=0,diffuse=0,specular=0,lighting=0,ambient=0,glow=0;
 protected:
  void ProcessAluInstruction(const rex::graphics::ParsedAluInstruction& i,uint8_t exports) override {
    using namespace rex::graphics;
    switch(PdzStylizedVectorSite(current_shader().ucode_data_hash(),i)) {
      case PdzStylizedVector::NormalXY:++normals;break;
      case PdzStylizedVector::Diffuse:++diffuse;break;
      case PdzStylizedVector::AmbientBlend:
      case PdzStylizedVector::AmbientColor:++ambient;break;
      default:break;
    }
    if(PdzStylizedSpecularSite(current_shader().ucode_data_hash(),i)) ++specular;
    if(PdzStylizedLightingOperand(current_shader().ucode_data_hash(),i)>=0) ++lighting;
    if(PdzStylizedGlowOperand(current_shader().ucode_data_hash(),i)>=0) ++glow;
    DxbcShaderTranslator::ProcessAluInstruction(i,exports);
  }
};
static void ValidatePresentationShaders() {
  glslang::InitializeProcess();
  TBuiltInResource resources{};
  resources.maxDrawBuffers = 8;
  resources.maxComputeWorkGroupSizeX = resources.maxComputeWorkGroupSizeY = 1024;
  resources.maxComputeWorkGroupSizeZ = 64;
  resources.maxComputeImageUniforms = resources.maxFragmentImageUniforms = 8;
  resources.maxImageUnits = resources.maxCombinedImageUniforms = 8;
  resources.maxTextureImageUnits = resources.maxComputeTextureImageUnits = 32;
  resources.maxCombinedTextureImageUnits = 80;
  resources.limits.generalUniformIndexing = true;
  resources.limits.generalSamplerIndexing = true;
  unsigned count = 0;
  for (bool compute : {false, true}) for (bool pwl : {false, true})
    for (bool luma : {false, true}) for (bool rb : {false, true}) {
      auto source = rex::gpu::PdzSwapGradeGlsl(compute, pwl, luma, rb);
      auto stage = compute ? EShLangCompute : EShLangFragment;
      glslang::TShader shader(stage);
      const char* text = source.c_str();
      shader.setStrings(&text, 1);
      shader.setEnvInput(glslang::EShSourceGlsl, stage, glslang::EShClientVulkan, 450);
      shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
      shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
      auto messages = EShMessages(EShMsgSpvRules | EShMsgVulkanRules);
      if (!shader.parse(&resources, 450, false, messages)) throw std::runtime_error(shader.getInfoLog());
      glslang::TProgram program;
      program.addShader(&shader);
      if (!program.link(messages)) throw std::runtime_error(program.getInfoLog());
      std::vector<uint32_t> words;
      glslang::GlslangToSpv(*program.getIntermediate(stage), words);
      spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_0);
      if (!validator.Validate(words)) throw std::runtime_error("Invalid presentation SPIR-V");
      ++count;
    }
  std::printf("PASS: %u presentation shaders (table/PWL, graphics/compute, FXAA, RB swap)\n", count);
}

int main(int argc, char** argv) {
  try {
  using namespace rex::graphics;
  if(argc==4&&std::string_view(argv[1])=="--inspect-package-directory") {
    std::filesystem::create_directories(argv[3]);
    std::ofstream cache(std::filesystem::path(argv[3])/"offline-package-programs.xsh",std::ios::binary);
    uint32_t cacheHeader[2]={0x48534558,0};cache.write(reinterpret_cast<char*>(cacheHeader),8);
    std::map<uint64_t,std::vector<uint32_t>> unique;
    unsigned blobs=0,vertices=0,pixels=0;
    for(const auto& entry:std::filesystem::directory_iterator(argv[2])) {
      if(entry.path().extension()!=".native")continue;
      auto nativePath=std::filesystem::absolute(entry.path());
#ifdef _WIN32
      nativePath=std::filesystem::path(L"\\\\?\\"+nativePath.wstring());
#endif
      std::ifstream input(nativePath,std::ios::binary);
      if(!input)throw std::runtime_error("Native blob open failed: "+entry.path().string());
      std::vector<unsigned char> image((std::istreambuf_iterator<char>(input)),{});
      auto be=[&](size_t p)->uint32_t {return (uint32_t(image.at(p))<<24)|(uint32_t(image.at(p+1))<<16)|(uint32_t(image.at(p+2))<<8)|image.at(p+3);};
      if(image.size()<24||(be(0)&0xFFFFFF00)!=0x102A0E00)throw std::runtime_error("Invalid native blob");
      auto offset=be(4),bytes=be(8);bool pixel=(be(0)&127)==0;
      if((be(0)&127)>1||bytes%12||uint64_t(offset)+bytes!=image.size())throw std::runtime_error("Invalid native bounds");
      std::vector<uint32_t> words(bytes/4);std::memcpy(words.data(),image.data()+offset,bytes);
      auto hash=XXH3_64bits(words.data(),bytes);++blobs;
      auto existing=unique.find(hash);if(existing!=unique.end()){if(existing->second!=words)throw std::runtime_error("Hash collision");continue;}unique.emplace(hash,words);
      SpirvShader shader(pixel?xenos::ShaderType::kPixel:xenos::ShaderType::kVertex,hash,words.data(),words.size());
      rex::string::StringBuffer disassembly;shader.AnalyzeUcode(disassembly);
      char name[64];std::snprintf(name,sizeof(name),"%016llX-%s.txt",(unsigned long long)hash,pixel?"ps":"vs");
      std::ofstream output(std::filesystem::path(argv[3])/name,std::ios::binary);output<<shader.ucode_disassembly();
      if(!output)throw std::runtime_error("Disassembly write failed");
      uint32_t encoded=uint32_t(words.size())|(pixel?0x80000000u:0);cache.write(reinterpret_cast<char*>(&hash),8);cache.write(reinterpret_cast<char*>(&encoded),4);cache.write(reinterpret_cast<char*>(words.data()),bytes);
      pixel?++pixels:++vertices;
    }
    if(!cache)throw std::runtime_error("Offline inventory write failed");
    std::printf("PASS: %u native blobs, %zu unique GPU programs (%u vertex, %u pixel), offline disassembly complete\n",blobs,unique.size(),vertices,pixels);
    return 0;
  }
  if(argc==4&&std::string_view(argv[1])=="--inspect-native-image") {
    std::ifstream input(argv[2],std::ios::binary);
    std::vector<unsigned char> image((std::istreambuf_iterator<char>(input)),{});
    auto be=[&](size_t p)->uint32_t {return (uint32_t(image.at(p))<<24)|(uint32_t(image.at(p+1))<<16)|(uint32_t(image.at(p+2))<<8)|image.at(p+3);};
    std::filesystem::create_directories(argv[3]);
    unsigned total=0;
    for(size_t p=0;p+24<image.size();p+=4) {
      if((be(p)&0xFFFFFF00)!=0x102A0E00) continue;
      const auto code=be(p+4),bytes=be(p+8),meta=be(p+16);
      if(code<24||!bytes||bytes%12||code>0x100000||bytes>0x100000||p+code+bytes>image.size()||meta<24||meta+16>code) continue;
      const auto stage=be(p+meta+12);
      if(stage!=0xFFFE0300&&stage!=0xFFFF0300) continue;
      // The game's sub_827D1790 returns header[1]+header[2] as total size.
      // Copy original guest-endian bytes; Shader performs its own byte swap.
      std::vector<uint32_t> words(bytes/4);
      std::memcpy(words.data(),image.data()+p+code,bytes);
      const auto hash=XXH3_64bits(words.data(),bytes);
      SpirvShader shader(stage==0xFFFF0300?xenos::ShaderType::kPixel:xenos::ShaderType::kVertex,hash,words.data(),words.size());
      rex::string::StringBuffer disassembly;shader.AnalyzeUcode(disassembly);
      char name[80];std::snprintf(name,sizeof(name),"%08llX-%016llX-%s.txt",(unsigned long long)p,(unsigned long long)hash,stage==0xFFFF0300?"ps":"vs");
      std::ofstream output(std::filesystem::path(argv[3])/name,std::ios::binary);output<<shader.ucode_disassembly();
      if(!output) throw std::runtime_error("Native disassembly write failed");
      std::printf("offset=%08llX stage=%s bytes=%u hash=%016llX\n",(unsigned long long)p,stage==0xFFFF0300?"pixel":"vertex",bytes,(unsigned long long)hash);
      ++total;
    }
    std::printf("Native metadata-checked programs analyzed offline: %u\n",total);
    return total?0:1;
  }
  if(argc==4&&std::string_view(argv[1])=="--validate-stylized") {
    std::ifstream cache(argv[2],std::ios::binary);uint32_t header[2]{};
    if(!cache.read(reinterpret_cast<char*>(header),8)||header[0]!=0x48534558)
      throw std::runtime_error("Invalid shader cache");
    std::filesystem::create_directories(argv[3]);
    unsigned shaders=0,variants=0,normals=0,diffuse=0,specular=0;
    while(cache) {
      uint64_t hash;uint32_t encoded;
      if(!cache.read(reinterpret_cast<char*>(&hash),8)) break;
      if(!cache.read(reinterpret_cast<char*>(&encoded),4)) throw std::runtime_error("Truncated shader");
      uint32_t count=encoded&0x7FFFFFFF;
      if(!count||count>0x10000) throw std::runtime_error("Invalid shader size");
      std::vector<uint32_t> words(count);
      if(!cache.read(reinterpret_cast<char*>(words.data()),count*4)||XXH3_64bits(words.data(),count*4)!=hash)
        throw std::runtime_error("Cache hash/read mismatch");
      if(!IsPdzStylizedShader(hash)) continue;
      if(!(encoded>>31)) throw std::runtime_error("Material whitelist includes vertex shader");
      ++shaders;
      for(bool rov:{false,true}) {
        DxbcShader shader(xenos::ShaderType::kPixel,hash,words.data(),count);
        rex::string::StringBuffer disasm;shader.AnalyzeUcode(disasm);
        std::vector<uint8_t> vanilla;
        for(bool enabled:{false,true}) {
          InspectDxbc translator(rov);
          DxbcShaderTranslator::Modification mod(translator.GetDefaultPixelShaderModification(0));
          mod.pixel.interpolator_mask=0xFFFF;
          if(enabled) mod.value|=kPdzStylizedModification;
          auto* result=shader.GetOrCreateTranslation(mod.value);
          if(!translator.TranslateAnalyzedShader(*result)||!result->is_valid())
            throw std::runtime_error("Material DXBC translation failed");
            if(PdzMaterial(hash)&&translator.normals!=1)
            throw std::runtime_error("Normal decode site not unique");
          const unsigned expectedForwardSpecular=(hash==0x3F1FF5706118A3E4||hash==0x8E6834C840E648B1)?1u:2u;
          if(PdzHashIn(hash,kPdzForwardShaders)&&(translator.diffuse!=2||translator.specular!=expectedForwardSpecular)) {
            std::printf("Forward mismatch %016llX diffuse%u specular%u\n",(unsigned long long)hash,translator.diffuse,translator.specular);
            throw std::runtime_error("Forward lighting sites mismatch");
          }
          const auto& bytes=result->translated_binary();
          if(translator.glow!=(PdzHashIn(hash,kPdzCompositeShaders)?2u:0u))
            throw std::runtime_error("Composite glow sites not unique");
          if(translator.lighting!=((PdzMaterial(hash)||PdzHashIn(hash,kPdzForwardShaders))?1u:0u))
            throw std::runtime_error("Material lighting combine not unique");
          if(translator.ambient!=(PdzHashIn(hash,kPdzForwardShaders)?2u:0u)) {
            std::printf("Ambient mismatch %016llX sites%u\n",(unsigned long long)hash,translator.ambient);
            throw std::runtime_error("Ambient sites not unique");
          }
          if(PdzHashIn(hash,kPdzLocalLightShaders)&&translator.diffuse!=1)
            throw std::runtime_error("Local N dot L site not unique");
          Microsoft::WRL::ComPtr<ID3DBlob> assembly;
          if(FAILED(D3DDisassemble(bytes.data(),bytes.size(),0,nullptr,&assembly)))
            throw std::runtime_error("Invalid DXBC disassembly");
          if(enabled) {
            if(bytes==vanilla) throw std::runtime_error("Stylized shader identical to vanilla");
            const std::string emittedLighting(static_cast<const char*>(assembly->GetBufferPointer()),assembly->GetBufferSize());
            if(emittedLighting.find("2.999000")!=std::string::npos)
              throw std::runtime_error("Accumulated brightness quantization still present");
            if(PdzHashIn(hash,kPdzLocalLightShaders)) {
              const std::regex localFlow(R"(mov_sat (r[0-9]+)\.x, \1\.x\s+add \1\.y, \1\.x, l\(-0\.720000\)\s+mul_sat \1\.y, \1\.y, l\(16\.666668\)\s+add \1\.x, \1\.x, l\(-0\.220000\)\s+mul_sat \1\.x, \1\.x, l\(16\.666668\)\s+add \1\.x, \1\.x, \1\.y\s+mul \1\.x, \1\.x, l\(0\.500000\)\s+mov_sat r0\.w, \1\.x\s+mul r[0-9]+\.w, r0\.w, r2\.w)");
              if(!std::regex_search(emittedLighting,localFlow))
                throw std::runtime_error("Local toon N dot L does not feed original attenuation");
            }
            if(PdzHashIn(hash,kPdzForwardShaders)) {
              const std::string emitted(static_cast<const char*>(assembly->GetBufferPointer()),assembly->GetBufferSize());
              // Follow the SAME temporary X through quantization into the
              // guest diffuse Z. This rejects the original valid-but-unused Z patch.
              const std::regex flow(R"(mov_sat (r[0-9]+)\.x, \1\.x\s+add \1\.y, \1\.x, l\(-0\.720000\)\s+mul_sat \1\.y, \1\.y, l\(16\.666668\)\s+add \1\.x, \1\.x, l\(-0\.220000\)\s+mul_sat \1\.x, \1\.x, l\(16\.666668\)\s+add \1\.x, \1\.x, \1\.y\s+mul \1\.x, \1\.x, l\(0\.500000\)\s+mov_sat r12\.z, \1\.x)");
              if(!std::regex_search(emitted,flow))
                throw std::runtime_error("Cel ramp does not reach diffuse result");
              const std::regex directionalFlow(R"(mov_sat (r[0-9]+)\.x, \1\.x\s+add \1\.y, \1\.x, l\(-0\.720000\)\s+mul_sat \1\.y, \1\.y, l\(16\.666668\)\s+add \1\.x, \1\.x, l\(-0\.220000\)\s+mul_sat \1\.x, \1\.x, l\(16\.666668\)\s+add \1\.x, \1\.x, \1\.y\s+mul \1\.x, \1\.x, l\(0\.500000\)\s+mov_sat r6\.zw, \1\.xxxx)");
              if(!std::regex_search(emitted,directionalFlow))
                throw std::runtime_error("Directional diffuse band does not reach pre-shadow ZW");
            }
            if(!rov){normals+=translator.normals;diffuse+=translator.diffuse;specular+=translator.specular;}
          } else vanilla=bytes;
          char name[90];std::snprintf(name,sizeof(name),"%016llX-%s-%s.txt",
              (unsigned long long)hash,rov?"rov":"rtv",enabled?"stylized":"vanilla");
          std::ofstream output(std::filesystem::path(argv[3])/name,std::ios::binary);
          output.write(static_cast<const char*>(assembly->GetBufferPointer()),assembly->GetBufferSize());
          if(!output) throw std::runtime_error("Output write failed");
          ++variants;
        }
      }
    }
      if(shaders<146||normals<122) {
      std::printf("Coverage shaders%u normals%u diffuse%u specular%u\n",shaders,normals,diffuse,specular);
      throw std::runtime_error("Incomplete material cache coverage");
    }
    std::printf("PASS: %u material shaders, %u valid DXBC variants; lighting data flow verified; normal sites%u diffuse%u specular%u\n",
        shaders,variants,normals,diffuse,specular);
    return 0;
  }
  // Offline research only: decode every cached guest shader, without creating
  // a graphics device or touching the original cache/game process.
  if(argc==4&&std::string_view(argv[1])=="--disassemble-cache") {
    std::ifstream cache(argv[2],std::ios::binary);
    uint32_t header[2]{};
    if(!cache.read(reinterpret_cast<char*>(header),sizeof(header))||
       header[0]!=0x48534558) throw std::runtime_error("Invalid shader cache");
    std::filesystem::create_directories(argv[3]);
    unsigned total=0;
    while(cache) {
      uint64_t hash=0;uint32_t encoded=0;
      if(!cache.read(reinterpret_cast<char*>(&hash),8)) break;
      if(!cache.read(reinterpret_cast<char*>(&encoded),4)) throw std::runtime_error("Truncated record");
      const uint32_t count=encoded&0x7FFFFFFF;
      if(!count||count>0x10000) throw std::runtime_error("Invalid shader size");
      std::vector<uint32_t> words(count);
      if(!cache.read(reinterpret_cast<char*>(words.data()),count*4)||
         XXH3_64bits(words.data(),count*4)!=hash) throw std::runtime_error("Shader hash/read mismatch");
      SpirvShader shader((encoded>>31)?xenos::ShaderType::kPixel:xenos::ShaderType::kVertex,
                        hash,words.data(),count);
      rex::string::StringBuffer disassembly;
      shader.AnalyzeUcode(disassembly);
      char name[64];std::snprintf(name,sizeof(name),"%016llX-%s.txt",
          (unsigned long long)hash,(encoded>>31)?"ps":"vs");
      std::ofstream output(std::filesystem::path(argv[3])/name,std::ios::binary);
      output<<shader.ucode_disassembly();
      if(!output) throw std::runtime_error("Disassembly write failed");
      ++total;
    }
    std::printf("PASS: %u hash-verified cached shaders disassembled offline\n",total);
    return 0;
  }
  if (argc != 3) return 2;
  ValidatePresentationShaders();
  RunVulkanGradeReadback();
  using namespace rex::graphics;
  std::ifstream input(argv[1], std::ios::binary);
  if (!input.is_open()) throw std::runtime_error("Shader cache unavailable or locked; close the game");
  uint32_t header[2];
  if (!input.read(reinterpret_cast<char*>(header), sizeof(header)) || header[0] != 0x48534558)
    throw std::runtime_error("Not an Xenos shader cache");
  std::filesystem::create_directories(argv[2]);
  unsigned matches = 0, translations = 0;
  while (input) {
    uint64_t hash;
    uint32_t count_type;
    if (!input.read(reinterpret_cast<char*>(&hash), sizeof(hash))) break;
    if (!input.read(reinterpret_cast<char*>(&count_type), sizeof(count_type))) break;
    const uint32_t count = count_type & 0x7FFFFFFF;
    if (!count || count > 0x10000) throw std::runtime_error("Invalid cache record size");
    std::vector<uint32_t> words(count);
    if (!input.read(reinterpret_cast<char*>(words.data()), count * sizeof(uint32_t))) break;
    if (!rex::gpu::IsPdzSceneComposite(hash)) continue;
    if (!(count_type >> 31) || XXH3_64bits(words.data(), count * sizeof(uint32_t)) != hash)
      throw std::runtime_error("Composite shader cache hash/type mismatch");
    ++matches;
    for (bool interlock : {false, true}) {
      SpirvShaderTranslator::Features features(true);
      SpirvShaderTranslator translator(features, false, false, interlock);
      SpirvShader shader(rex::graphics::xenos::ShaderType::kPixel, hash, words.data(), count);
      rex::string::StringBuffer disassembly;
      shader.AnalyzeUcode(disassembly);
      for (bool grading : {false, true}) {
        SpirvShaderTranslator::Modification modification(translator.GetDefaultPixelShaderModification(0));
        modification.pixel.interpolator_mask = 0xFFFF;
        if (grading) modification.value |= rex::gpu::kPdzSceneGradeModification;
        auto* translation = shader.GetOrCreateTranslation(modification.value);
        if (!translator.TranslateAnalyzedShader(*translation) || !translation->is_valid())
          throw std::runtime_error("Composite SPIR-V translation failed");
        char name[90];
        std::snprintf(name, sizeof(name), "%016llX-%s-%s.spv", (unsigned long long)hash,
                      interlock ? "interlock" : "host", grading ? "grade" : "original");
        std::ofstream output(std::filesystem::path(argv[2]) / name, std::ios::binary);
        const auto& bytes = translation->translated_binary();
        spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_2);
        validator.SetMessageConsumer([](spv_message_level_t, const char*, const spv_position_t& pos,
                                       const char* message) {
          std::fprintf(stderr, "SPIR-V validation at %zu: %s\n", pos.index, message);
        });
        // Match the Vulkan 1.2 features enabled by VulkanDevice for GPU emulation.
        spvtools::ValidatorOptions options;
        options.SetUniformBufferStandardLayout(true);
        options.SetScalarBlockLayout(true);
        if (!validator.Validate(reinterpret_cast<const uint32_t*>(bytes.data()), bytes.size()/4, options))
          throw std::runtime_error("Invalid emitted SPIR-V");
        if (!output.write(reinterpret_cast<const char*>(bytes.data()), bytes.size()))
          throw std::runtime_error("Unable to save validation fixture");
        ++translations;
      }
    }
  }
  if (matches != 2 || translations != 8) throw std::runtime_error("Both proven composites required");
  std::printf("PASS: two real guest composites translated with/without grading, host/interlock (8 modules)\n");
  } catch (const std::exception& error) {
    std::fprintf(stderr, "FAIL: %s\n", error.what());
    return 1;
  }
}

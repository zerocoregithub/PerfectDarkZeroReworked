#pragma once
#include <rex/ui/vulkan/api.h>
#include <rex/graphics/pdz_scene_grade_spirv.h>
#include <rex/graphics/display_grade.h>
#include <spirv-tools/libspirv.hpp>
#include <array>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <cstdio>

inline void RunVulkanGradeReadback() {
  using rex::graphics::SpirvBuilder;
  SpirvBuilder b(spv::Spv_1_0, 0, nullptr);
  b.addCapability(spv::CapabilityShader);
  b.setMemoryModel(spv::AddressingModelLogical, spv::MemoryModelGLSL450);
  const auto ext = b.import("GLSL.std.450");
  const auto f4 = b.makeVectorType(b.makeFloatType(32), 4);
  const auto block = b.makeStructType({f4, f4, f4}, "Fixture");
  b.addDecoration(block, spv::DecorationBufferBlock);
  for (unsigned i=0;i<3;++i) b.addMemberDecoration(block,i,spv::DecorationOffset,16*i);
  const auto data = b.createVariable(spv::NoPrecision,spv::StorageClassUniform,block,"fixture");
  b.addDecoration(data,spv::DecorationDescriptorSet,0);
  b.addDecoration(data,spv::DecorationBinding,0);
  auto* main = b.makeEntryPoint("main");
  b.addEntryPoint(spv::ExecutionModelGLCompute,main,"main");
  b.addExecutionMode(main,spv::ExecutionModeLocalSize,1,1,1);
  const auto field = [&](int i) {
    return b.createAccessChain(spv::StorageClassUniform,data,{b.makeIntConstant(i)});
  };
  const auto color = b.createVariable(spv::NoPrecision,spv::StorageClassFunction,f4,"color");
  b.createStore(b.createLoad(field(0),spv::NoPrecision),color);
  rex::graphics::EmitPdzSceneGradeSpirv(b,color,b.createLoad(field(1),spv::NoPrecision),ext);
  b.createStore(b.createLoad(color,spv::NoPrecision),field(2));
  b.makeReturn(false);
  b.leaveFunction();
  std::vector<uint32_t> code; b.dump(code);
  spvtools::SpirvTools validator(SPV_ENV_VULKAN_1_0);
  validator.SetMessageConsumer([](spv_message_level_t,const char*,const spv_position_t&,const char* message) {
    std::fprintf(stderr,"Readback SPIR-V: %s\n",message);
  });
  if(!validator.Validate(code)) throw std::runtime_error("Readback SPIR-V invalid");

  const auto loader=LoadLibraryW(L"vulkan-1.dll");
  if(!loader) throw std::runtime_error("Vulkan loader unavailable");
  const auto get=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader,"vkGetInstanceProcAddr"));
  if(!get) throw std::runtime_error("Vulkan entry unavailable");
  auto vkCreateInstance=reinterpret_cast<PFN_vkCreateInstance>(get(nullptr,"vkCreateInstance"));
  const auto check=[](VkResult r) {if(r!=VK_SUCCESS) throw std::runtime_error("Vulkan API error: "+std::to_string(r));};
  VkInstance instance{};
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.pApplicationName="PDZ isolated grading check";app.apiVersion=VK_API_VERSION_1_0;
  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ici.pApplicationInfo=&app;
  check(vkCreateInstance(&ici,nullptr,&instance));
#define PDZ_LOAD_I(n) auto n=reinterpret_cast<PFN_##n>(get(instance,#n))
  PDZ_LOAD_I(vkEnumeratePhysicalDevices); PDZ_LOAD_I(vkGetPhysicalDeviceQueueFamilyProperties);
  PDZ_LOAD_I(vkGetPhysicalDeviceMemoryProperties); PDZ_LOAD_I(vkGetPhysicalDeviceProperties);
  PDZ_LOAD_I(vkCreateDevice); PDZ_LOAD_I(vkGetDeviceProcAddr); PDZ_LOAD_I(vkDestroyInstance);
#undef PDZ_LOAD_I
  uint32_t count=0;check(vkEnumeratePhysicalDevices(instance,&count,nullptr));
  if(!count)throw std::runtime_error("No Vulkan GPU");
  std::vector<VkPhysicalDevice> devices(count);check(vkEnumeratePhysicalDevices(instance,&count,devices.data()));
  VkPhysicalDevice physical{};uint32_t family=UINT32_MAX;
  for(auto device:devices) {
    uint32_t n=0;vkGetPhysicalDeviceQueueFamilyProperties(device,&n,nullptr);
    std::vector<VkQueueFamilyProperties> families(n);vkGetPhysicalDeviceQueueFamilyProperties(device,&n,families.data());
    for(uint32_t i=0;i<n;++i)if(families[i].queueFlags&VK_QUEUE_COMPUTE_BIT) {physical=device;family=i;break;}
    if(physical)break;
  }
  if(!physical)throw std::runtime_error("No compute queue");
  VkPhysicalDeviceProperties properties{};vkGetPhysicalDeviceProperties(physical,&properties);
  float priority=1;VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qci.queueFamilyIndex=family;qci.queueCount=1;qci.pQueuePriorities=&priority;
  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dci.queueCreateInfoCount=1;dci.pQueueCreateInfos=&qci;
  VkDevice device{};check(vkCreateDevice(physical,&dci,nullptr,&device));
#define PDZ_LOAD_D(n) auto n=reinterpret_cast<PFN_##n>(vkGetDeviceProcAddr(device,#n))
  PDZ_LOAD_D(vkGetDeviceQueue);PDZ_LOAD_D(vkCreateBuffer);PDZ_LOAD_D(vkGetBufferMemoryRequirements);
  PDZ_LOAD_D(vkAllocateMemory);PDZ_LOAD_D(vkBindBufferMemory);PDZ_LOAD_D(vkMapMemory);
  PDZ_LOAD_D(vkCreateDescriptorSetLayout);PDZ_LOAD_D(vkCreateDescriptorPool);PDZ_LOAD_D(vkAllocateDescriptorSets);PDZ_LOAD_D(vkUpdateDescriptorSets);
  PDZ_LOAD_D(vkCreatePipelineLayout);PDZ_LOAD_D(vkCreateShaderModule);PDZ_LOAD_D(vkCreateComputePipelines);
  PDZ_LOAD_D(vkCreateCommandPool);PDZ_LOAD_D(vkAllocateCommandBuffers);PDZ_LOAD_D(vkBeginCommandBuffer);PDZ_LOAD_D(vkEndCommandBuffer);
  PDZ_LOAD_D(vkCmdBindPipeline);PDZ_LOAD_D(vkCmdBindDescriptorSets);PDZ_LOAD_D(vkCmdDispatch);PDZ_LOAD_D(vkCmdPipelineBarrier);
  PDZ_LOAD_D(vkQueueSubmit);PDZ_LOAD_D(vkQueueWaitIdle);PDZ_LOAD_D(vkDestroyDevice);
  PDZ_LOAD_D(vkDestroyCommandPool);PDZ_LOAD_D(vkDestroyPipeline);PDZ_LOAD_D(vkDestroyShaderModule);
  PDZ_LOAD_D(vkDestroyPipelineLayout);PDZ_LOAD_D(vkDestroyDescriptorPool);PDZ_LOAD_D(vkDestroyDescriptorSetLayout);
  PDZ_LOAD_D(vkUnmapMemory);PDZ_LOAD_D(vkDestroyBuffer);PDZ_LOAD_D(vkFreeMemory);
#undef PDZ_LOAD_D
  VkQueue queue{};vkGetDeviceQueue(device,family,0,&queue);
  VkBuffer buffer{};VkBufferCreateInfo bi{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};bi.size=48;bi.usage=VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
  check(vkCreateBuffer(device,&bi,nullptr,&buffer));VkMemoryRequirements requirements{};vkGetBufferMemoryRequirements(device,buffer,&requirements);
  VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(physical,&memory);
  uint32_t memory_type=UINT32_MAX;
  for(uint32_t i=0;i<memory.memoryTypeCount;++i)if((requirements.memoryTypeBits&(1u<<i))&&
      (memory.memoryTypes[i].propertyFlags&(VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))==
      (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)){memory_type=i;break;}
  if(memory_type==UINT32_MAX)throw std::runtime_error("No coherent memory");
  VkMemoryAllocateInfo mai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};mai.allocationSize=requirements.size;mai.memoryTypeIndex=memory_type;
  VkDeviceMemory allocation{};check(vkAllocateMemory(device,&mai,nullptr,&allocation));check(vkBindBufferMemory(device,buffer,allocation,0));
  float* mapped{};check(vkMapMemory(device,allocation,0,48,0,reinterpret_cast<void**>(&mapped)));
  VkDescriptorSetLayoutBinding binding{0,VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1,VK_SHADER_STAGE_COMPUTE_BIT,nullptr};
  VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=1;li.pBindings=&binding;
  VkDescriptorSetLayout layout{};check(vkCreateDescriptorSetLayout(device,&li,nullptr,&layout));
  VkDescriptorPoolSize size{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1};VkDescriptorPoolCreateInfo pi{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};pi.maxSets=1;pi.poolSizeCount=1;pi.pPoolSizes=&size;
  VkDescriptorPool pool{};check(vkCreateDescriptorPool(device,&pi,nullptr,&pool));
  VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=pool;ai.descriptorSetCount=1;ai.pSetLayouts=&layout;
  VkDescriptorSet set{};check(vkAllocateDescriptorSets(device,&ai,&set));
  VkDescriptorBufferInfo db{buffer,0,48};VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};write.dstSet=set;write.descriptorCount=1;write.descriptorType=VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;write.pBufferInfo=&db;
  vkUpdateDescriptorSets(device,1,&write,0,nullptr);
  VkPipelineLayoutCreateInfo pli{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pli.setLayoutCount=1;pli.pSetLayouts=&layout;
  VkPipelineLayout pipeline_layout{};check(vkCreatePipelineLayout(device,&pli,nullptr,&pipeline_layout));
  VkShaderModuleCreateInfo smi{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};smi.codeSize=code.size()*4;smi.pCode=code.data();
  VkShaderModule shader{};check(vkCreateShaderModule(device,&smi,nullptr,&shader));
  VkComputePipelineCreateInfo cpi{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};cpi.layout=pipeline_layout;cpi.stage={VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO};cpi.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;cpi.stage.module=shader;cpi.stage.pName="main";
  VkPipeline pipeline{};check(vkCreateComputePipelines(device,VK_NULL_HANDLE,1,&cpi,nullptr,&pipeline));
  VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.queueFamilyIndex=family;
  VkCommandPool commands{};check(vkCreateCommandPool(device,&cp,nullptr,&commands));
  VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=commands;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;
  VkCommandBuffer command{};check(vkAllocateCommandBuffers(device,&ca,&command));
  VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};check(vkBeginCommandBuffer(command,&begin));
  vkCmdBindPipeline(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);vkCmdBindDescriptorSets(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline_layout,0,1,&set,0,nullptr);vkCmdDispatch(command,1,1,1);
  VkMemoryBarrier barrier{VK_STRUCTURE_TYPE_MEMORY_BARRIER};barrier.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;barrier.dstAccessMask=VK_ACCESS_HOST_READ_BIT;
  vkCmdPipelineBarrier(command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,1,&barrier,0,nullptr,0,nullptr);check(vkEndCommandBuffer(command));
  VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&command;
  const std::array<std::array<float,4>,7> colors={{{0,0,0,.37f},{1,1,1,.5f},{.1f,.5f,.9f,.77f},{.9f,.2f,.4f,1},{-.3f,1.2f,.2f,.4f},{.001f,.007f,.02f,.2f},{.4f,.7f,.3f,.9f}}};
  const std::array<std::array<float,4>,8> grades={{{1,1,1,0},{.5f,1,1,0},{2,1,1,0},{1,0,1,0},{1,2,0,0},{1,1,2,.5f},{1,1,1,-.5f},{.8f,1.2f,.7f,.2f}}};
  unsigned tests=0;float max_error=0;
  for(const auto& color_value:colors)for(const auto& grade:grades) {
    std::memcpy(mapped,color_value.data(),16);std::memcpy(mapped+4,grade.data(),16);
    check(vkQueueSubmit(queue,1,&submit,VK_NULL_HANDLE));check(vkQueueWaitIdle(queue));
    std::array<float,3> expected{color_value[0],color_value[1],color_value[2]};
    const bool neutral=grade==grades[0];
    if(!neutral) {
      if(grade[0]!=1)for(auto& c:expected)c=std::pow(std::clamp(c,0.f,1.f),grade[0]);
      expected=rex::gpu::ApplyDisplayGrade(expected,{grade[1],grade[2],grade[3]});
      // Gamma-only still performs the epilogue's final clamp.
      for(auto& c:expected)c=std::clamp(c,0.f,1.f);
    }
    if(neutral&&std::memcmp(mapped,mapped+8,16))throw std::runtime_error("Neutral Vulkan output changed");
    if(std::memcmp(mapped+3,mapped+11,4))throw std::runtime_error("Vulkan alpha changed");
    for(unsigned i=0;i<3;++i) {
      const float error=std::abs(mapped[8+i]-expected[i]);max_error=std::max(max_error,error);
      if(!std::isfinite(mapped[8+i])||error>0.00004f)throw std::runtime_error("Vulkan grading RGB mismatch");
    }
    ++tests;
  }
  std::printf("PASS: Vulkan actual epilogue GPU readback (%s), %u cases, neutral bit-exact, alpha unchanged, max error %.8f\n",properties.deviceName,tests,max_error);
  vkDestroyCommandPool(device,commands,nullptr);vkDestroyPipeline(device,pipeline,nullptr);vkDestroyShaderModule(device,shader,nullptr);
  vkDestroyPipelineLayout(device,pipeline_layout,nullptr);vkDestroyDescriptorPool(device,pool,nullptr);vkDestroyDescriptorSetLayout(device,layout,nullptr);
  vkUnmapMemory(device,allocation);vkDestroyBuffer(device,buffer,nullptr);vkFreeMemory(device,allocation,nullptr);
  vkDestroyDevice(device,nullptr);vkDestroyInstance(instance,nullptr);FreeLibrary(loader);
}

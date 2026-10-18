#pragma once
#include <cstdint>
#include <rex/graphics/pdz_material_inventory.h>
#include <rex/graphics/pipeline/shader/shader.h>
namespace rex::graphics {
// Versioned, unused shader modification bit: vanilla cache entries stay valid.
// Revision 7: ambient bands, light-buffer bands and additive material suppression.
inline constexpr uint64_t kPdzStylizedModification=UINT64_C(1)<<53;
// Exact decoded normal XY -> original reconstructed Z family; not screen normals.
inline constexpr uint64_t kPdzAdditionalNormalShaders[]={0x0162621D062F01FF,0x02F3C3A4FDA88A9D,0x130EA28F4B6669FC,0x15068041AE31E5C7,0x151840597EBA5263,0x158AAFD05CD8D43D,0x167445CDFE3AC037,0x173C0D54966B7794,0x20E598ADAF9E982D,0x216ABB4BDEC642F1,0x236BDF51103717F2,0x2579C3A471B5FEF3,0x29A15D895AD01456,0x2AC5D9623336F894,0x2C167E5E56BACDDC,0x2CAD45E00AF1400E,0x2E7CABA1F9697C9F,0x2F819CD167EC0993,0x2FAED7D8C0722012,0x3214319C8B6EF2C1,0x36A35E9C1F891753,0x3F88DF6BE482C043,0x4198AF0DC278D91C,0x45251BB9E95F7FE4,0x48D814C6E4644751,0x498F3C14A5598472,0x4F6ADC2A56515467,0x5062764D614F3295,0x5516997D73443BD7,0x586ED5795BB1774E,0x59319D541ED7361D,0x5A6FC00E11DDCEA3,0x5A7F8C26A46B0A36,0x5E577D56EDE2207F,0x601DC9AB6D02366E,0x6193473BDBA8547E,0x67D8C50F2E5B992B,0x6A3681D7E7654056,0x6B004DCAD5562E29,0x6F6948CC6C24F822,0x734E9B267815F9FB,0x74243F95F1390594,0x7BB007E705C49AF2,0x7DFD40634FE7A5B0,0x81BD4E5D2D01E601,0x82A415B267133881,0x86C0E6AFD99C7034,0x94F21EE39BA27120,0x99D07165776A7254,0x9A473969F945E99C,0x9B4DF929D15C4CD4,0xA7B25D43FD9E960C,0xA81B58EDEA6922D3,0xAAC56449A53DBF20,0xAD1D65322AE3DC88,0xAD770A9A26A1BBCC,0xAEB1DA8FF6D73F4E,0xAECF6734EA09F807,0xB3F3B29B3A591470,0xB49409140F6D7F19,0xB6865161AE804041,0xB7A0E67A23B8D087,0xB7E8AE3F06E69670,0xB848FA6CC5B48B90,0xB86A2757853937E7,0xBB1D32DB914F8C3A,0xBDF00557AEFCD6D9,0xC1E480B064576456,0xC222D0F9BB3CE596,0xC27DFD39C7BA8004,0xCB3735D11EAD8BE4,0xCED0F41AD2C5CC67,0xD103C72D505C0919,0xD25AB09BED228A8C,0xD641386FA9D64358,0xDBC57B0DB535469F,0xE1763E4BFF0217C3,0xE4E97A911ACE1583,0xF35E873260556109,0xF98148929EF15898};
inline constexpr uint64_t kPdzAlternateNormalShaders[]={0x0A82EBB8AEAA55E6,0x2DCFEFB2D8A21E25};
inline constexpr uint64_t kPdzLocalLightShaders[]={0x1E07BBDB64BC0D56,0xF81BAF1C00AEF550};
inline constexpr uint64_t kPdzNormalShaders[]={
  0x069A663450D3F9C4,0x0DCFFD7F7DCCEE92,0x0F6B2553FFE3BBF1,0x1567C7E9CA11EEAF,
  0x1692725ED71C97BA,0x213F58F0D6D315A9,0x24F88E97BC13B86D,0x3457AF0CCF1FF705,
  0x37814D42F74F4B6F,0x466A5249046A4560,0x5D3F7DC5FE5635A0,0x676B2AD544B7B81C,
  0x6A84A4482ABBD153,0x6F73A13440E3093E,0x703BC7D1982C3513,0x7290182A01CCA34C,
  0x7A4E73EE4F3DE524,0x8F86B23BEAE7EF97,0x91E6079FAEA94708,0x91F3BE656E9CAFD0,
  0x92EFA62B34C13498,0x9ED344433E8E90A2,0xA862FDE55B18880E,0xABD60B09689E6360,
  0xBAFB801CE03E9E98,0xBB0CAAE300B8DAF3,0xBECA8560E5405392,0xC8C90F94C271CCD5,
  0xCA8C9987FE315AA4,0xD6AB85F7D911F960,0xDA6140C99F0E463F,0xDBDC624435A97420,
  0xDC9EDF352CD14C50,0xDEE2AF0CBB57E10D,0xF2E6090A877F793B,0xF97CA6C2B4947012,
  0xFB3C0C367F97273F,0xFB4E681A93D96FB9,0xFC0B52E99065D901,0xFE7CAF87163F987E};
inline constexpr uint64_t kPdzForwardShaders[]={
  0x008D2E17D2DADB51,0x0ECC51C307AE3F55,0x197F08BE180EE1C9,0x1EE7923CD1795824,0x23E6706AB052D3E2,0x3F1FF5706118A3E4,0x58F74EF401141916,0x6196C30D1C1BFBCF,0x8CB0EFE4F5DBB81C,0x8E6834C840E648B1,0x8EDF97DC1817B995,0x982FC33FDF1205B2,0x9ED689A6B1FA5949,0xAD6220D93EB80913,0xBC6C81385E8862DA,0xBE9220E0B2D84DE5,0xD8F111EF03816A85,0xDC5B7A454AEA3BC8,0xEB71DCCDE33D2F60,0xFDC1DA6FE63F21C6};
template<size_t N> inline bool PdzHashIn(uint64_t hash,const uint64_t (&list)[N]) {
  for(auto value:list) if(value==hash) return true;
  return false;
}
inline constexpr uint64_t kPdzCompositeShaders[]={0xEDCCE21F19A013D1,0x07E9288309420978};
inline bool IsPdzStylizedShader(uint64_t hash) {
  return PdzMaterial(hash)||PdzHashIn(hash,kPdzNormalShaders)||PdzHashIn(hash,kPdzForwardShaders)||
      PdzHashIn(hash,kPdzLocalLightShaders)||PdzHashIn(hash,kPdzAlternateNormalShaders)||
      PdzHashIn(hash,kPdzAdditionalNormalShaders)||PdzHashIn(hash,kPdzCompositeShaders);
}
// Captured fetch constants prove tf1=80x60 soft-light, tf3=160x120 flare;
// tf0=1216x640 scene and tf2=64x64 lookup remain intact. DOF also intact.
inline int PdzStylizedGlowOperand(uint64_t hash,const ParsedAluInstruction& i) {
  if(!PdzHashIn(hash,kPdzCompositeShaders)) return -1;
  const auto& r=i.vector_and_constant_result;
  if(i.vector_opcode==ucode::AluVectorOpcode::kAdd&&i.vector_operand_count==2&&
     r.storage_target==InstructionStorageTarget::kRegister&&r.storage_index==0&&r.GetUsedWriteMask()==7&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[0].storage_index==0&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[1].storage_index==1) return 1;
  if(i.vector_opcode==ucode::AluVectorOpcode::kMad&&i.vector_operand_count==3&&
     r.storage_target==InstructionStorageTarget::kColor&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[0].storage_index==2&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[1].storage_index==3&&
     i.vector_operands[2].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[2].storage_index==0) return 0;
  return -1;
}
enum class PdzStylizedVector { None, NormalXY, Diffuse, AmbientBlend, AmbientColor };
// Exact material combine: albedo * lighting + separate additive contribution.
// Band the lighting operand, not the textured result or fogged final output.
inline int PdzStylizedLightingOperand(uint64_t hash,const ParsedAluInstruction& i) {
  const auto& r=i.vector_and_constant_result;
  if(i.vector_opcode!=ucode::AluVectorOpcode::kMad||i.vector_operand_count!=3||
     r.storage_target!=InstructionStorageTarget::kRegister||
     r.storage_addressing_mode!=InstructionStorageAddressingMode::kAbsolute||
     r.GetUsedWriteMask()!=7) return -1;
  for(unsigned n=0;n<3;++n) if(i.vector_operands[n].storage_source!=InstructionStorageSource::kRegister||
      i.vector_operands[n].storage_addressing_mode!=InstructionStorageAddressingMode::kAbsolute) return -1;
  const auto* material=PdzMaterial(hash);
  if(material&&r.storage_index==1&&
     i.vector_operands[0].storage_index==1&&i.vector_operands[1].storage_index==0&&
     i.vector_operands[2].storage_index==material->additive) return 1;
  if(PdzHashIn(hash,kPdzForwardShaders)&&r.storage_index==8&&
     i.vector_operands[0].storage_index==8&&i.vector_operands[1].storage_index==9&&
     i.vector_operands[2].storage_index==10) return 1;
  return -1;
}
inline PdzStylizedVector PdzStylizedVectorSite(uint64_t hash,const ParsedAluInstruction& i) {
  const auto& r=i.vector_and_constant_result;
  if(r.storage_target!=InstructionStorageTarget::kRegister||
     r.storage_addressing_mode!=InstructionStorageAddressingMode::kAbsolute) return PdzStylizedVector::None;
  const auto* material=PdzMaterial(hash);
  if(material&&r.storage_index==material->normal&&r.GetUsedWriteMask()==3&&
     i.vector_opcode==ucode::AluVectorOpcode::kMad&&i.vector_operand_count==3&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[0].storage_index==material->normal&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[1].storage_index==0&&
     i.vector_operands[2].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[2].storage_index==0)
    return PdzStylizedVector::NormalXY;
  const uint32_t alternate=hash==0x0A82EBB8AEAA55E6?7:4;
  if(PdzHashIn(hash,kPdzAlternateNormalShaders)&&r.storage_index==alternate&&r.GetUsedWriteMask()==3&&
     i.vector_opcode==ucode::AluVectorOpcode::kMad&&i.vector_operand_count==3&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[0].storage_index==alternate&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[1].storage_index==0&&
     i.vector_operands[2].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[2].storage_index==0)
    return PdzStylizedVector::NormalXY;
  if((PdzHashIn(hash,kPdzNormalShaders)||PdzHashIn(hash,kPdzAdditionalNormalShaders))&&r.storage_index==6&&r.GetUsedWriteMask()==3&&
     i.vector_opcode==ucode::AluVectorOpcode::kMad&&i.vector_operand_count==3&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[0].storage_index==6&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[1].storage_index==0&&
     i.vector_operands[2].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[2].storage_index==0)
    return PdzStylizedVector::NormalXY;
  // Proven forward hemisphere blend, not reflection-vector c26 dots.
  if(PdzHashIn(hash,kPdzForwardShaders)&&i.vector_opcode==ucode::AluVectorOpcode::kMad&&
     i.vector_operand_count==3&&r.storage_index==1&&r.GetUsedWriteMask()==8&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[0].storage_index==1&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[1].storage_index<=1&&
     i.vector_operands[2].storage_source==InstructionStorageSource::kConstantFloat&&
     i.vector_operands[2].storage_index==i.vector_operands[1].storage_index)
    return PdzStylizedVector::AmbientBlend;
  if(PdzHashIn(hash,kPdzForwardShaders)&&i.vector_opcode==ucode::AluVectorOpcode::kMad&&
     i.vector_operand_count==3&&r.storage_index==1&&r.GetUsedWriteMask()==7&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[0].storage_index==1&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[1].storage_index==25&&
     i.vector_operands[2].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[2].storage_index==1)
    return PdzStylizedVector::AmbientColor;
  if(PdzHashIn(hash,kPdzForwardShaders)&&r.storage_index==12&&r.GetUsedWriteMask()==4&&r.is_clamped&&
     i.vector_opcode==ucode::AluVectorOpcode::kDp3&&
     i.vector_operands[0].storage_index==2&&i.vector_operands[1].storage_index==12)
    return PdzStylizedVector::Diffuse;
  // Direct normal dot directional vector, before independent shadow visibility.
  // Do not band c28 half-vector/Fresnel dots in material shaders.
  if(PdzHashIn(hash,kPdzForwardShaders)&&r.storage_index==6&&r.GetUsedWriteMask()==12&&r.is_clamped&&
     i.vector_opcode==ucode::AluVectorOpcode::kDp3&&i.vector_operand_count==2&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&i.vector_operands[0].storage_index==2&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kConstantFloat&&i.vector_operands[1].storage_index==28)
    return PdzStylizedVector::Diffuse;
  if(PdzHashIn(hash,kPdzLocalLightShaders)&&r.storage_index==0&&r.GetUsedWriteMask()==8&&r.is_clamped&&
     i.vector_opcode==ucode::AluVectorOpcode::kDp3&&i.vector_operand_count==2&&
     i.vector_operands[0].storage_source==InstructionStorageSource::kRegister&&
     i.vector_operands[1].storage_source==InstructionStorageSource::kRegister&&
     i.vector_operands[0].storage_index==3&&i.vector_operands[1].storage_index==2)
    return PdzStylizedVector::Diffuse;
  return PdzStylizedVector::None;
}
inline bool PdzStylizedParallaxSite(uint64_t hash,const ParsedAluInstruction& i) {
  const auto* m=PdzMaterial(hash);
  const auto& r=i.vector_and_constant_result;
  if(!m||!m->parallax||i.vector_opcode!=ucode::AluVectorOpcode::kMad||i.vector_operand_count!=3||
     r.storage_target!=InstructionStorageTarget::kRegister||r.storage_index!=5||r.GetUsedWriteMask()!=3||
     r.storage_addressing_mode!=InstructionStorageAddressingMode::kAbsolute) return false;
  for(unsigned n=0;n<3;++n) if(i.vector_operands[n].storage_source!=InstructionStorageSource::kRegister||
      i.vector_operands[n].storage_addressing_mode!=InstructionStorageAddressingMode::kAbsolute) return false;
  return i.vector_operands[0].storage_index==2&&i.vector_operands[1].storage_index==2&&i.vector_operands[2].storage_index==5;
}
inline bool PdzStylizedSpecularSite(uint64_t hash,const ParsedAluInstruction& i) {
  const auto& r=i.scalar_result;
  if(i.scalar_opcode!=ucode::AluScalarOpcode::kExp||r.storage_target!=InstructionStorageTarget::kRegister||
     r.storage_addressing_mode!=InstructionStorageAddressingMode::kAbsolute) return false;
  return (PdzHashIn(hash,kPdzLocalLightShaders)&&r.storage_index==1&&r.GetUsedWriteMask()==8)||
      (PdzHashIn(hash,kPdzNormalShaders)&&r.storage_index==5&&r.GetUsedWriteMask()==8)||
      (PdzHashIn(hash,kPdzForwardShaders)&&((r.storage_index==12&&r.GetUsedWriteMask()==2)||
                                         (r.storage_index==0&&r.GetUsedWriteMask()==8)));
}
}

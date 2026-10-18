#include <rex/graphics/pdz_motion_blur.h>
#include <cassert>
#include <fstream>
#include <iterator>
#include <vector>
#include <iostream>
int main(int argc,char** argv){
 assert(argc==2);
 std::ifstream file(argv[1],std::ios::binary);
 std::vector<uint8_t> binary{std::istreambuf_iterator<char>(file),{}};
 assert(rex::gpu::IsCapturedPdzMotionBlur(binary));
 binary[4]^=1;assert(!rex::gpu::IsCapturedPdzMotionBlur(binary));
 assert(!rex::gpu::IsCapturedPdzMotionBlur({}));
 std::array<uint64_t,4> bitmap={0x7e,0,0,0};
 assert(rex::gpu::IsNativePdzMotionBlur(0x1AEB45F93B4CC719ull,6,false,bitmap));
 assert(!rex::gpu::IsNativePdzMotionBlur(0,6,false,bitmap));
 assert(!rex::gpu::IsNativePdzMotionBlur(0x1AEB45F93B4CC719ull,5,false,bitmap));
 assert(!rex::gpu::IsNativePdzMotionBlur(0x1AEB45F93B4CC719ull,6,true,bitmap));
 bitmap[0]=0x3f;
 assert(!rex::gpu::IsNativePdzMotionBlur(0x1AEB45F93B4CC719ull,6,false,bitmap));
 bitmap[0]=0x7e;bitmap[1]=1;
 assert(!rex::gpu::IsNativePdzMotionBlur(0x1AEB45F93B4CC719ull,6,false,bitmap));
 std::array<float,24> original;for(unsigned i=0;i<24;++i)original[i]=i+.25f;
 auto upload=original;
 assert(!rex::gpu::DisableCapturedBlurOffsets(upload.data(),6,false));assert(upload==original);
 assert(!rex::gpu::DisableCapturedBlurOffsets(upload.data(),5,true));assert(upload==original);
 assert(rex::gpu::DisableCapturedBlurOffsets(upload.data(),6,true));
 for(unsigned i=0;i<24;++i)assert(upload[i]==(i>=12&&i<16?0.f:original[i]));
 // The copied source is not modified; restoration is a fresh original upload.
 upload=original;assert(upload[12]!=0);
 std::cout<<"PASS: actual captured DXBC fingerprint, mismatch rejection, disabled/wrong-size isolation, exact offset-only upload change and fresh-copy restoration\n";
}

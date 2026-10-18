#include <rex/audio/sdl/sdl_audio_driver.h>
#include <rex/logging.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
int main(int argc, char** argv) {
  using namespace std::chrono;
  rex::InitLogging("out/test-silent-audio.log", spdlog::level::debug);
  auto semaphore=rex::thread::Semaphore::Create(0,16);
  rex::audio::sdl::SDLAudioDriver driver(nullptr,semaphore.get());
  if(argc>1 && std::string_view(argv[1])=="normal") {
    std::cerr<<"normal: initializing\n";
    try {
      if(!driver.Initialize())throw std::runtime_error("normal SDL startup failed");
    } catch (const std::exception& error) {
      driver.Shutdown();
      std::cerr<<"normal initialization exception: "<<error.what()<<'\n';
      return 1;
    }
    std::cerr<<"normal: initialized; shutting down\n";
    driver.Shutdown();
    std::cout<<"PASS: normal SDL stream initialization/shutdown\n";
    return 0;
  }
  if(argc>1 && std::string_view(argv[1])=="failed") {
    if(driver.Initialize())throw std::runtime_error("expected SDL failure absent");
    driver.Shutdown();
  }
  const auto wait=[&](milliseconds timeout){return rex::thread::Wait(semaphore.get(),false,timeout);};
  if(!driver.InitializeSilent())throw std::runtime_error("silent startup failed");
  if(wait(milliseconds(20))!=rex::thread::WaitResult::kTimeout)throw std::runtime_error("idle emitted callback");
  const auto begin=steady_clock::now();
  for(int i=0;i<8;++i)driver.SubmitFrame(0); // Null guest memory proves no sample read.
  for(int i=0;i<8;++i)if(wait(milliseconds(200))!=rex::thread::WaitResult::kSuccess)throw std::runtime_error("frame credit missing");
  const auto elapsed=duration_cast<milliseconds>(steady_clock::now()-begin).count();
  if(elapsed<35||elapsed>1000)throw std::runtime_error("unpaced or stalled consumption");
  if(wait(milliseconds(20))!=rex::thread::WaitResult::kTimeout)throw std::runtime_error("extra frame credit");
  driver.SubmitFrame(0);
  driver.Shutdown();
  driver.Shutdown();
  if(!driver.InitializeSilent())throw std::runtime_error("reinitialization failed");
  driver.SubmitFrame(0);
  if(wait(milliseconds(200))!=rex::thread::WaitResult::kSuccess)throw std::runtime_error("reinitialized sink stalled");
  driver.Shutdown();
  std::cout<<"PASS: idle, eight paced credits ("<<elapsed<<"ms), no guest sample read, pending shutdown, double shutdown, reinitialize\n";
}

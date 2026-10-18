#include <rex/audio/sdl/sdl_audio_driver.h>
#include <rex/logging.h>
#include <atomic>
#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>

// Exercises the real SubmitFrame and SDLCallback with memory-only streams.
// No Windows output device, game image, guest execution or SDL device callback.
class QueueDriver final : public rex::audio::sdl::SDLAudioDriver {
 public:
  using SDLAudioDriver::SDLAudioDriver;
  ~QueueDriver() override { Shutdown(); }
  bool OpenMemoryStream() {
    SDL_AudioSpec spec{SDL_AUDIO_F32LE,6,48000};
    sdl_device_channels_=6;
    sdl_stream_=SDL_CreateAudioStream(&spec,&spec);
    return sdl_stream_!=nullptr;
  }
  bool Pump() {
    SDLCallback(this,sdl_stream_,6*256*sizeof(float),0);
    std::array<float,6*256> discarded{};
    return SDL_GetAudioStreamData(sdl_stream_,discarded.data(),sizeof(discarded))>=0;
  }
};

int main() {
  rex::InitLogging("out/test-audio-queue.log",spdlog::level::debug);
  rex::memory::Memory memory;
  if(!memory.Initialize())throw std::runtime_error("test memory initialization failed");
  const auto frame=memory.SystemHeapAlloc(6*256*sizeof(float));
  if(!frame)throw std::runtime_error("test frame allocation failed");
  std::memset(memory.TranslateVirtual<void*>(frame),0,6*256*sizeof(float));
  auto a=rex::thread::Semaphore::Create(0,2048);
  auto b=rex::thread::Semaphore::Create(0,2048);
  QueueDriver first(&memory,a.get()),second(&memory,b.get());
  if(!first.OpenMemoryStream()||!second.OpenMemoryStream())
    throw std::runtime_error("memory-only SDL stream unavailable");
  std::atomic<unsigned> failures{0};
  const auto exercise=[&](QueueDriver& driver,rex::thread::Semaphore* semaphore) {
    constexpr unsigned count=1000;
    std::jthread producer([&]{for(unsigned i=0;i<count;++i)driver.SubmitFrame(frame);});
    unsigned received=0;
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(received<count&&std::chrono::steady_clock::now()<deadline) {
      if(!driver.Pump()){++failures;break;}
      while(rex::thread::Wait(semaphore,false,std::chrono::milliseconds(0))==
            rex::thread::WaitResult::kSuccess)++received;
    }
    producer.join();
    if(received!=count)++failures;
  };
  std::thread consumerA([&]{exercise(first,a.get());});
  std::thread consumerB([&]{exercise(second,b.get());});
  consumerA.join();consumerB.join();
  first.Shutdown();second.Shutdown();
  memory.SystemHeapFree(frame);
  if(failures)throw std::runtime_error("concurrent audio frame accounting failed");
  std::cout<<"PASS: two real drivers, concurrent submit/callback, 2000 frame credits; memory-only SDL streams\n";
}

#include <rex/input/input_system.h>
#include <atomic>
#include <cassert>
#include <iostream>
#include <thread>
using rex::X_RESULT;
using rex::X_STATUS;
using namespace rex::input;
class Pad final : public InputDriver {
 public:
  Pad() : InputDriver(nullptr, 0) {}
  X_STATUS Setup() override { return X_STATUS_SUCCESS; }
  void EnumerateDevices(std::vector<DeviceInfo>& out) override {
    DeviceInfo d; d.id=static_cast<DeviceId>(1); d.name=std::string(512,'p'); out.push_back(d);
    std::this_thread::yield();
  }
  X_RESULT GetDeviceState(DeviceId, X_INPUT_STATE* out) override {
    *out={}; out->gamepad.buttons=X_INPUT_GAMEPAD_A; return X_ERROR_SUCCESS;
  }
  X_RESULT GetDeviceCapabilities(DeviceId,uint32_t,X_INPUT_CAPABILITIES* out) override {
    *out={}; return X_ERROR_SUCCESS;
  }
  X_RESULT SetDeviceVibration(DeviceId,X_INPUT_VIBRATION*) override { return X_ERROR_SUCCESS; }
  X_RESULT GetDeviceKeystroke(DeviceId,uint32_t,X_INPUT_KEYSTROKE*) override { return X_ERROR_EMPTY; }
};
int main() {
  InputSystem input(nullptr);
  input.AddDriver(std::make_unique<Pad>());
  input.SetDeviceAssignment(std::make_unique<SharedAssignment>());
  std::atomic<unsigned> filtered=0;
  input.SetDeviceFilter([&](uint32_t,const DeviceInfo& info,X_INPUT_STATE&) {
    std::this_thread::yield();
    assert(info.name==std::string(512,'p'));
    input.GetDeviceExtraButtons(info.id); // recursive filter access must not deadlock
    filtered++;
  });
  input.AddUIInputBlocker();
  std::thread ui([&] { for(int i=0;i<10000;i++) {
    X_INPUT_STATE s={}; assert(input.GetStateForUI(0,&s)==X_ERROR_SUCCESS);
    assert(s.gamepad.buttons==X_INPUT_GAMEPAD_A);
  }});
  std::thread guest([&] { for(int i=0;i<10000;i++) {
    X_INPUT_STATE s={}; assert(input.GetState(0,&s)==X_ERROR_SUCCESS); assert(!s.gamepad.buttons);
    X_INPUT_CAPABILITIES c={}; input.GetCapabilities(0,0,&c);
    X_INPUT_VIBRATION v={}; input.SetState(0,&v);
    X_INPUT_KEYSTROKE k={}; assert(input.GetKeystroke(0,0,&k)==X_ERROR_EMPTY);
  }});
  ui.join();guest.join();
  input.RemoveUIInputBlocker();
  X_INPUT_STATE s={}; input.GetState(0,&s); assert(!s.gamepad.buttons);
  assert(filtered>=10000);
  std::cout<<"PASS: concurrent UI/guest polling, reentrant filters, gameplay capture and release masking\n";
}

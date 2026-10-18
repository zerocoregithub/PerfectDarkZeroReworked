#include <rex/input/input_system.h>
#include <rex/cvar.h>
#include <iostream>
#include <stdexcept>
using rex::X_RESULT;
using rex::X_STATUS;
using namespace rex::input;
class Pad final:public InputDriver {
 public:
  Pad():InputDriver(nullptr,0){}
  X_INPUT_VIBRATION last{};
  unsigned calls=0;
  X_STATUS Setup() override{return X_STATUS_SUCCESS;}
  void EnumerateDevices(std::vector<DeviceInfo>& out) override {
    DeviceInfo d;d.id=static_cast<DeviceId>(1);out={d};
  }
  X_RESULT GetDeviceState(DeviceId,X_INPUT_STATE* out) override{*out={};return X_ERROR_SUCCESS;}
  X_RESULT GetDeviceCapabilities(DeviceId,uint32_t,X_INPUT_CAPABILITIES* out) override{*out={};return X_ERROR_SUCCESS;}
  X_RESULT SetDeviceVibration(DeviceId,X_INPUT_VIBRATION* v) override{last=*v;++calls;return X_ERROR_SUCCESS;}
  X_RESULT GetDeviceKeystroke(DeviceId,uint32_t,X_INPUT_KEYSTROKE*) override{return X_ERROR_EMPTY;}
};
void Check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){
  rex::cvar::SetFlagByName("vibration","true");
  rex::cvar::SetFlagByName("vibration_intensity","100");
  InputSystem system(nullptr);
  auto pad=std::make_unique<Pad>();auto* physical=pad.get();
  system.AddDriver(std::move(pad));
  system.SetDeviceAssignment(std::make_unique<SharedAssignment>());
  X_INPUT_VIBRATION request{};request.left_motor_speed=12345;request.right_motor_speed=54321;
  Check(system.SetState(0,&request)==X_ERROR_SUCCESS,"Initial vibration failed");
  Check(physical->last.left_motor_speed==12345&&physical->last.right_motor_speed==54321,"Enabled rumble changed");
  unsigned before=physical->calls;
  system.ToggleVibration();
  Check(!system.GetVibrationEnabled(),"Toggle did not disable vibration");
  Check(physical->calls>before&&!physical->last.left_motor_speed&&!physical->last.right_motor_speed,"Active motors not stopped");
  system.SetState(0,&request);
  Check(!physical->last.left_motor_speed&&!physical->last.right_motor_speed,"Disabled rumble leaked to driver");
  Check(request.left_motor_speed==12345&&request.right_motor_speed==54321,"Guest request mutated");
  system.ToggleVibration();system.SetState(0,&request);
  Check(physical->last.left_motor_speed==12345&&physical->last.right_motor_speed==54321,"Re-enable failed");
  for(int percent:{0,25,50,75,100}) {
    Check(rex::cvar::SetFlagByName("vibration_intensity",std::to_string(percent)),"Intensity setting rejected");
    system.SetState(0,&request);
    Check(physical->last.left_motor_speed==12345u*percent/100&&
          physical->last.right_motor_speed==54321u*percent/100,"Motor intensity scaling incorrect");
  }
  Check(request.left_motor_speed==12345&&request.right_motor_speed==54321,"Scaled request mutated guest data");
  std::cout<<"PASS: real InputSystem stops active motors, blocks later requests and restores both motors when re-enabled\n";
}

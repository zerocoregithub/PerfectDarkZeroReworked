#pragma once
/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2013 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <cstddef>
#include <functional>
#include <vector>

#include <rex/input/device.h>
#include <rex/input/input.h>
#include <rex/kernel.h>
#include <rex/ui/window.h>

namespace rex::ui {
class Window;
}

namespace rex::input {

class InputSystem;
struct MotionSample {
  // Driver timestamps share a monotonic nanosecond domain, not wall-clock time.
  // Motion is angular velocity, not a frame delta; game policy integrates it.
  // A supported device may have no fresh sample yet (timestamp_ns == 0).
  float pitch = 0, yaw = 0, roll = 0; // radians/second, SDL controller axes
  uint64_t timestamp_ns = 0;
  float accel_x = 0, accel_y = 0, accel_z = 0; // m/s^2, SDL axes
  uint64_t accel_timestamp_ns = 0;
  float rumble_strength = 0; // Successfully requested motor amplitude, not measured motion.
  uint64_t rumble_timestamp_ns = 0;
};
struct NativeInputCapabilities {
  bool gyro = false, accelerometer = false, steam_controller = false, quick_access = false;
  uint64_t extra_buttons = 0;
};

class InputDriver {
 public:
  virtual ~InputDriver() = default;

  virtual X_STATUS Setup() = 0;

  /// Devices this driver has open, in its own arrival order. InputSystem
  /// assigns the cross-driver ordinal, so leave DeviceInfo::ordinal at zero.
  virtual void EnumerateDevices(std::vector<DeviceInfo>& out) = 0;

  virtual X_RESULT GetDeviceState(DeviceId id, X_INPUT_STATE* out_state) = 0;
  virtual X_RESULT GetDeviceCapabilities(DeviceId id, uint32_t flags,
                                         X_INPUT_CAPABILITIES* out_caps) = 0;
  virtual X_RESULT SetDeviceVibration(DeviceId id, X_INPUT_VIBRATION* vibration) = 0;
  virtual X_RESULT GetDeviceKeystroke(DeviceId id, uint32_t flags,
                                      X_INPUT_KEYSTROKE* out_keystroke) = 0;

  virtual void OnWindowAvailable(rex::ui::Window* /*window*/) {}
  virtual bool GetDeviceMotion(DeviceId, MotionSample& out) { out = {}; return false; }
  // Non-consuming history newer than after. Readers keep their own watermark;
  // an overlay poll must not drain samples needed by guest-side camera updates.
  virtual void GetDeviceMotionSamples(DeviceId id,uint64_t after,std::vector<MotionSample>& out) {
    out.clear();MotionSample sample;
    if(GetDeviceMotion(id,sample)&&sample.timestamp_ns>after)out.push_back(sample);
  }
  virtual uint64_t GetDeviceExtraButtons(DeviceId) { return 0; }
  virtual NativeInputCapabilities GetDeviceNativeCapabilities(DeviceId) { return {}; }

  void set_is_active_callback(std::function<bool()> is_active_callback) {
    is_active_callback_ = is_active_callback;
  }

 protected:
  explicit InputDriver(rex::ui::Window* window, size_t window_z_order)
      : window_(window), window_z_order_(window_z_order) {}

  rex::ui::Window* window() const { return window_; }
  size_t window_z_order() const { return window_z_order_; }

  bool is_active() const { return !is_active_callback_ || is_active_callback_(); }

 private:
  rex::ui::Window* window_;
  size_t window_z_order_;
  std::function<bool()> is_active_callback_ = nullptr;
};

}  // namespace rex::input

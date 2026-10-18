#pragma once
#include <chrono>
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

namespace pdz::diagnostic {
using Clock=std::chrono::steady_clock;
inline int64_t Microseconds(Clock::time_point point) {
  return std::chrono::duration_cast<std::chrono::microseconds>(point.time_since_epoch()).count();
}
struct TimingRow {int64_t time,interval,cpu;bool hud_off,menu,transition;};
class HudTiming {
 public:
  static constexpr size_t capacity=240000;
  HudTiming(){rows.reserve(capacity);}
  void Record(int64_t now,int64_t cpu,bool off,bool menu) {
    const bool changed=have_previous&&off!=previous_mode;
    const auto interval=have_previous?now-previous_time:0;
    previous_time=now;previous_mode=off;have_previous=true;
    if(rows.size()<capacity)rows.push_back({now,interval,cpu,off,menu,changed});
    else ++dropped;
  }
  bool Export(const std::filesystem::path& file) const {
    std::error_code error;std::filesystem::create_directories(file.parent_path(),error);
    if(error)return false;
    std::ofstream out(file);
    if(!out)return false;
    out<<"# overlay CPU/arrival timings only; NOT GPU, Present or guest update FPS\n# dropped="<<dropped<<"\n";
    out<<"steady_us,overlay_interval_us,overlay_cpu_us,hud_off,menu_open,transition\n";
    for(const auto& row:rows)out<<row.time<<','<<row.interval<<','<<row.cpu<<','<<row.hud_off<<','<<row.menu<<','<<row.transition<<'\n';
    out.flush();return bool(out);
  }
  std::vector<TimingRow> rows;
  size_t dropped=0;
 private:
  bool have_previous=false,previous_mode=false;
  int64_t previous_time=0;
};
struct DrawSample {
  HudTiming& timing;const bool& off;const bool& menu;
  Clock::time_point start=Clock::now();
  ~DrawSample(){timing.Record(Microseconds(start),Microseconds(Clock::now())-Microseconds(start),off,menu);}
};
}

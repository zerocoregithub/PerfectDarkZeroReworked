#include <rex/logging.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
int main() {
  rex::InitLoggingEarly();
  auto before=rex::RegisterLogCategory("quiet_before");
  assert(rex::GetLogger(before)->sinks().empty());
  assert(!rex::GetLogger(before)->should_log(spdlog::level::critical));
  rex::LogConfig config;
  config.default_level=spdlog::level::trace;
  config.category_levels["quiet_before"]=spdlog::level::trace;
  config.log_to_console=true;
  config.log_file="quiet-logging-must-not-exist.log";
  config.app_name="quiet-logging-must-not-exist";
  config.log_dir="quiet-logging-must-not-exist";
  config.flush_interval=std::chrono::seconds(1);
  assert(!std::filesystem::exists(config.log_file));
  assert(!std::filesystem::exists(config.log_dir));
  rex::InitLogging(config);
  auto after=rex::RegisterLogCategory("quiet_after");
  rex::SetCategoryLevel(before,spdlog::level::trace);
  rex::SetRootLevel(before,spdlog::level::trace);
  rex::SetAllLevels(spdlog::level::trace);
  rex::AddSink(spdlog::sink_ptr{});
  rex::AddSink(after,spdlog::sink_ptr{});
  rex::ReplaceConsoleSink(spdlog::sink_ptr{});
  for(auto category:{before,after}) {
    auto logger=rex::GetLogger(category);
    assert(logger->sinks().empty());
    assert(!logger->should_log(spdlog::level::critical));
  }
  assert(rex::LoggingConfig().flush_interval.count()==0);
  assert(rex::LoggingConfig().category_levels.empty());
  assert(!std::filesystem::exists(config.log_file));
  assert(!std::filesystem::exists(config.log_dir));
  rex::ShutdownLogging();
  std::puts("PASS: early/full initialization, sink/level overrides, no log files or flush interval");
}

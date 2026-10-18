$ErrorActionPreference='Stop'
$sdk=Join-Path (Split-Path $PSScriptRoot) '../rexglue-sdk-v13'
$source=Get-Content (Join-Path $sdk 'src/audio/sdl/sdl_audio_driver.cpp') -Raw
foreach($retired in @('sdl_submit_count','sdl_callback_count','queued_count={}')) {
  if($source.Contains($retired)){throw "Unsafe shared diagnostic remains: $retired"}
}
$submit=$source.Substring($source.IndexOf('void SDLAudioDriver::SubmitFrame('))
$submit=$submit.Substring(0,$submit.IndexOf('void SDLAudioDriver::Shutdown('))
$prefix=$submit.Substring(0,$submit.IndexOf('frames_queued_.push'))
$lastLock=$prefix.LastIndexOf('std::unique_lock<std::mutex> guard(frames_mutex_);')
if($lastLock -lt 0 -or $prefix.Substring(0,$lastLock).Contains('frames_queued_.')) {
  throw 'Queue access before submission lock'
}
foreach($preserved in @('++silent_pending_','silent_thread_.join()','semaphore_->Release(1, nullptr)','frames_queued_.push(output_frame)','frames_queued_.pop()')) {
  if(!$source.Contains($preserved)){throw "Missing preserved audio operation: $preserved"}
}
'PASS: unsafe queue diagnostic/shared counters absent; submission lock and audio operations retained (structural only)'

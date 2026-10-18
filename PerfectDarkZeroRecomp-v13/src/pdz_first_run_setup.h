#pragma once
#include <commdlg.h>
#include <bcrypt.h>
#include "pdz_iso_setup.h"
namespace pdz::setup {
inline void ValidateExtractedExecutable(const fs::path& path) {
  BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;
  struct Handles {BCRYPT_ALG_HANDLE& a;BCRYPT_HASH_HANDLE& h;~Handles(){if(h)BCryptDestroyHash(h);if(a)BCryptCloseAlgorithmProvider(a,0);}} handles{algorithm,hash};
  if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)<0||BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)<0)
    throw std::runtime_error("Windows SHA-256 validation could not be initialized.");
  std::ifstream input(path,std::ios::binary);if(!input)throw std::runtime_error("Extracted executable is missing.");
  std::array<char,65536> buffer{};
  while(input){input.read(buffer.data(),buffer.size());const auto size=input.gcount();if(size&&BCryptHashData(hash,reinterpret_cast<PUCHAR>(buffer.data()),ULONG(size),0)<0)throw std::runtime_error("Executable hash failed.");}
  if(!input.eof())throw std::runtime_error("Executable validation read failed.");
  std::array<unsigned char,32> digest{};
  if(BCryptFinishHash(hash,digest.data(),ULONG(digest.size()),0)<0)throw std::runtime_error("Executable hash failed.");
  constexpr std::array<unsigned char,32> expected={0xe5,0x01,0x92,0xf4,0x37,0xbd,0x1f,0xf7,0x0a,0x65,0xfa,0xe8,0xb5,0x56,0x1a,0xa8,0x22,0xab,0x91,0x05,0x2f,0xe3,0x5f,0xd5,0x00,0x59,0x46,0x89,0xe1,0x30,0x85,0x9e};
  if(digest!=expected)throw std::runtime_error("Extracted executable does not match the accepted build's required input; setup was not installed.");
}
class Progress {
  HWND window_=nullptr,label_=nullptr;bool cancelled_=false;
  static LRESULT CALLBACK Proc(HWND w,UINT m,WPARAM a,LPARAM b) {
    auto* self=reinterpret_cast<Progress*>(GetWindowLongPtrW(w,GWLP_USERDATA));
    if(m==WM_NCCREATE){self=static_cast<Progress*>(reinterpret_cast<CREATESTRUCTW*>(b)->lpCreateParams);SetWindowLongPtrW(w,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(self));}
    if(self&&(m==WM_CLOSE||(m==WM_COMMAND&&LOWORD(a)==1))){self->cancelled_=true;return 0;}
    return DefWindowProcW(w,m,a,b);
  }
 public:
  Progress() {
    WNDCLASSW wc{};wc.lpfnWndProc=Proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"PDZLocalSetup";wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));wc.hbrBackground=reinterpret_cast<HBRUSH>(COLOR_WINDOW+1);
    RegisterClassW(&wc);
    window_=CreateWindowW(wc.lpszClassName,L"Perfect Dark Zero Reworked — local ISO setup",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU, CW_USEDEFAULT,CW_USEDEFAULT,560,180,nullptr,nullptr,wc.hInstance,this);
    if(!window_)throw std::runtime_error("Cannot create setup progress window.");
    label_=CreateWindowW(L"STATIC",L"Validating your ISO...",WS_CHILD|WS_VISIBLE,20,20,510,60,window_,nullptr,wc.hInstance,nullptr);
    CreateWindowW(L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP,425,95,100,28,window_,reinterpret_cast<HMENU>(1),wc.hInstance,nullptr);
    ShowWindow(window_,SW_SHOW);UpdateWindow(window_);
  }
  ~Progress(){if(window_)DestroyWindow(window_);}
  bool Update(uint64_t done=0,uint64_t total=0) {
    if(total){const auto text=L"Extracting locally: "+std::to_wstring(done*100/total)+L"%\nYour ISO, existing settings and saves are not modified.";SetWindowTextW(label_,text.c_str());}
    MSG msg{};while(PeekMessageW(&msg,nullptr,0,0,PM_REMOVE)){TranslateMessage(&msg);DispatchMessageW(&msg);}
    return cancelled_;
  }
};
inline bool EnsureGameData(const fs::path& data) {
  const auto assets=data/L"assets";
  if(fs::is_regular_file(assets/L"default.xex")&&fs::is_directory(assets/L"pdz"))return true;
  if(fs::exists(assets))throw std::runtime_error("Incomplete assets folder already exists. Move it aside manually before retrying setup; it will not be overwritten.");
  if(MessageBoxW(nullptr,L"First-run test: supply your own legally obtained Perfect Dark Zero ISO.\n\nSetup reads the ISO locally, validates the supported executable and extracts into Data. No game content is downloaded. Allow at least 10 GB free. The runtime is already compiled; this step extracts game data, not source code.\n\nContinue?",L"Perfect Dark Zero Reworked — private setup test",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK)return false;
  wchar_t selected[32768]{};
  const auto rom=data.parent_path()/L"ROM";
  // Probe local candidates without extracting or modifying them. Only use one
  // unambiguous supported image; multiple matches retain explicit user choice.
  fs::path automatic;
  unsigned matches=0;
  std::error_code scan_error;
  for(fs::directory_iterator it(rom,scan_error),end;!scan_error&&it!=end;it.increment(scan_error)) {
    if(!it->is_regular_file(scan_error))continue;
    auto extension=it->path().extension().wstring();
    for(auto& ch:extension)if(ch>=L'A'&&ch<=L'Z')ch+=L'a'-L'A';
    if(extension!=L".iso")continue;
    try {
      Image candidate(it->path());
      const auto candidate_plan=candidate.Inspect([]{return false;});
      Validate(candidate_plan,candidate,[]{return false;});
      automatic=it->path();++matches;
    } catch(const std::exception&) { /* Unsupported/unreadable: offer picker below. */ }
  }
  OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.lpstrFile=selected;dialog.nMaxFile=DWORD(std::size(selected));
  dialog.lpstrFilter=L"Xbox disc images (*.iso)\0*.iso\0\0";dialog.lpstrInitialDir=rom.c_str();dialog.lpstrTitle=L"Choose your legally obtained Perfect Dark Zero ISO";
  dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR|OFN_EXPLORER;
  if(matches!=1&&!GetOpenFileNameW(&dialog)){if(CommDlgExtendedError())throw std::runtime_error("ISO selection dialog failed.");return false;}
  Progress progress;Image image{matches==1?automatic:fs::path(selected)};
  const auto cancelled=[&]{return progress.Update();};
  const auto plan=image.Inspect(cancelled);Validate(plan,image,cancelled);
  if(fs::space(data).available<plan.bytes+1024ull*1024*1024)throw std::runtime_error("Not enough free space for safe extraction (plus 1 GB headroom).");
  const auto staging=data/(L"setup-staging-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));
  image.Extract(plan,staging,[&](uint64_t done,uint64_t total){return progress.Update(done,total);});
  ValidateExtractedExecutable(staging/L"default.xex");
  if(progress.Update())throw std::runtime_error("Setup cancelled; staged files were not installed.");
  // Promotion is one same-volume directory rename. Existing installations are never merged.
  if(fs::exists(assets))throw std::runtime_error("Assets appeared during setup; refusing to replace them.");
  fs::rename(staging,assets);
  return true;
}
}

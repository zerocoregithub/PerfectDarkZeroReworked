// perfectdarkzerorecomp - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once
#include "pdz_numeric_font.h"

#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/memory/utils.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/system/function_dispatcher.h>
#include <rex/system/gpu_plugin.h>
#include <rex/ppc.h>

#include "pdz_overlay.h"
#include "pdz_input.h"
#include "pdz_settings_file.h"
#include "local_profiles.h"
#include "mod/third_person.h"
#include "pdz_presets.h"
#include "pdz_ui_mouse.h"
#include <rex/input/input_system.h>

namespace GameConstants
{
  constexpr uint32_t kCodeBase = 0x82000000;
  constexpr uint32_t kCodeEnd = 0x82E6C540;
}

REXCVAR_DEFINE_STRING(graphics_backend, "any", "GPU",
                      "Graphics API backend: any, d3d12, vulkan")
    .allowed({"any", "d3d12", "vulkan"});

REXCVAR_DEFINE_BOOL(pdz_internal_1080p, false, "GPU",
                    "Experimental 1920x1080 scene rendering using 960x540 targets and 2x GPU scaling")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

// Experimental ports of the community xenia-canary "60 FPS" / "Aspect Ratio"
// Both off by default to match upstream.
REXCVAR_DEFINE_BOOL(pdz_fps60_unlock, false, "Gameplay",
                    "Experimental: unlock 60 FPS update rate (ported from xenia-canary "
                    "game-patches, may affect physics/animation timing)");
REXCVAR_DEFINE_BOOL(pdz_skip_intro, false, "Gameplay",
                    "Skip startup intro video using the normal completion path");
REXCVAR_DEFINE_BOOL(pdz_stub_sweep, false, "Developer",
                    "Install fallback stubs for missing guest functions (slow diagnostic)");
REXCVAR_DEFINE_BOOL(pdz_aspect_ratio_16_9, false, "Gameplay",
                    "Experimental: force a 16:9 aspect ratio constant (ported from "
                    "xenia-canary game-patches)");
REXCVAR_DEFINE_BOOL(pdz_direct_mouse, false, "Input",
                    "Feed relative mouse motion after PDZ's analog-stick conditioner");
REXCVAR_DEFINE_BOOL(pdz_modern_movement, true, "Gameplay/Movement",
                    "Faster local-player walking response outside cover");
REXCVAR_DEFINE_BOOL(pdz_classic_movement, false, "Gameplay/Movement",
                    "Perfect Dark / GoldenEye-style walking response outside cover");
REXCVAR_DEFINE_BOOL(pdz_unarmed_run_boost, false, "Gameplay/Movement",
                    "Classic forward-run peak is 125 percent when unarmed");
REXCVAR_DEFINE_BOOL(pdz_hide_unarmed_hands, false, "Gameplay/Viewmodel",
                    "Hide idle and locomotion unarmed hands; retain melee/disarm and unknown actions");
REXCVAR_DEFINE_BOOL(pdz_hold_y_holster, false, "Input",
                    "Hold Y for half a second to holster; tap retains native Y behavior");
REXCVAR_DEFINE_DOUBLE(pdz_weapon_bob_amount, 1.0, "Gameplay/Viewmodel",
                      "Native procedural weapon bob amplitude, independent of aim sway")
    .range(0.0, 1.0);
REXCVAR_DEFINE_BOOL(pdz_dodge_roll, true, "Gameplay/Movement",
                    "Enhanced dodge roll with fixed fatigue and recovery");
REXCVAR_DEFINE_BOOL(pdz_reload_interact, true, "Input",
                    "X reloads when possible; otherwise taps action. Hold A for cover");
REXCVAR_DEFINE_BOOL(pdz_classic_npc_durability, false, "Gameplay/Combat",
                    "Reduce standard humanoid NPC effective health; player and armor unchanged");
REXCVAR_DEFINE_DOUBLE(pdz_npc_health_scale, .5, "Gameplay/Combat",
                      "Effective NPC health fraction when Classic durability is enabled")
    .range(.25, 1.0);
REXCVAR_DEFINE_DOUBLE(pdz_movement_response, 4.0, "Gameplay/Movement",
                      "Walking acceleration/braking response multiplier; 1 is vanilla")
    .range(1.0, 8.0);
REXCVAR_DEFINE_DOUBLE(pdz_weapon_action_speed, 2.0, "Gameplay/Weapons",
                      "Local weapon equip, holster and reload animation rate; 1 is vanilla")
    .range(1.0, 3.0);
REXCVAR_DEFINE_BOOL(pdz_dpad_zoom, false, "Gameplay/Weapons",
                    "Use D-pad left/right for magnification; LT still aims");
REXCVAR_DEFINE_DOUBLE(pdz_mouse_sensitivity, 1.0, "Input",
                      "Sensitivity of PDZ's direct mouse path")
    .range(0.01, 10.0);
REXCVAR_DEFINE_DOUBLE(pdz_fov_scale, 1.0, "Gameplay",
                      "Scale PDZ perspective-frustum extents to widen field of view")
    .range(0.5, 2.0);
REXCVAR_DEFINE_DOUBLE(pdz_horizontal_fov, 105.0, "Gameplay",
                      "Horizontal gameplay field of view in degrees at 16:9")
    .range(40.0, 140.0);
REXCVAR_DEFINE_DOUBLE(pdz_display_gamma, 1.0, "GPU",
                      "Presentation gamma multiplier; 1 is original, higher lifts midtones")
    .range(0.5, 2.0);
REXCVAR_DEFINE_DOUBLE(pdz_display_contrast, 1.0, "GPU",
                      "Game-image contrast about mid-grey (includes HUD)")
    .range(0.0, 2.0);
REXCVAR_DEFINE_BOOL(pdz_scene_only_grading, true, "GPU",
                    "Apply gamma and color grading only to proven scene composites on D3D12/Vulkan, preserving HUD");
REXCVAR_DEFINE_BOOL(pdz_stylized_lighting, false, "GPU",
                    "Experimental DX12 flat materials, preserved color, softer toon thresholds,zero normal/specular influence,15% glow/material emission");
REXCVAR_DEFINE_DOUBLE(pdz_display_saturation, 1.0, "GPU",
                      "Game-image saturation; 0 is greyscale (includes HUD)")
    .range(0.0, 2.0);
REXCVAR_DEFINE_DOUBLE(pdz_display_tint, 0.0, "GPU",
                      "Game-image tint: negative green, positive magenta (includes HUD)")
    .range(-0.5, 0.5);
REXCVAR_DEFINE_DOUBLE(pdz_viewmodel_horizontal_shift, 0.0, "Gameplay/Viewmodel",
                      "Horizontal offset applied to PDZ's first-person gun visual")
    .range(-6.0, 6.0);
REXCVAR_DEFINE_BOOL(pdz_classic_weapon_sway, false, "Gameplay/Viewmodel",
                    "Optional Perfect Dark / GoldenEye-style turn sway for the first-person weapon");
REXCVAR_DEFINE_DOUBLE(pdz_classic_weapon_sway_amount, 1.0, "Gameplay/Viewmodel",
                      "Classic weapon turn-sway strength")
    .range(0.0, 8.0);
REXCVAR_DEFINE_BOOL(pdz_classic_sway_aim, true, "Gameplay/Viewmodel",
                    "Move the crosshair and firing aim point with Classic weapon sway");
REXCVAR_DEFINE_BOOL(pdz_classic_free_aim, false, "Input",
                    "Optional mouse free aim; edge turns camera");
REXCVAR_DEFINE_BOOL(pdz_free_aim_ads_only, true, "Input",
                    "Restrict free aim to ADS/zoom; disable to include hip fire");
REXCVAR_DEFINE_BOOL(pdz_sway_during_ads, true, "Input",
                    "Allow manual camera/free aim and weapon turn sway during ADS");
REXCVAR_DEFINE_DOUBLE(pdz_cursor_visual_smoothing, .04, "Input",
                      "Render-only cursor easing time in seconds; firing remains raw")
    .range(0.0, .15);
REXCVAR_DEFINE_DOUBLE(pdz_zoom_transition, .08, "Input",
                      "Three-stage D-pad zoom easing time in seconds")
    .range(0.0, .25);
REXCVAR_DEFINE_DOUBLE(pdz_cursor_sway_deadzone, 0.0, "Gameplay/Viewmodel",
                      "Central quiet threshold for coupled weapon/cursor turn sway, in radians")
    .range(0.0, 0.06);
REXCVAR_DEFINE_BOOL(pdz_auto_aim, true, "Input",
                    "Allow native aim-point attraction; disabling retains manual aim and spread");
REXCVAR_DEFINE_BOOL(pdz_weapon_accuracy, false, "Gameplay/Weapons",
                    "Halve random spread for verified pistols and RCP90; recoil unchanged");
REXCVAR_DEFINE_DOUBLE(pdz_crosshair_edge_deadzone, 0.5, "Input",
                      "Free-aim camera turn threshold: 0=center, 1=no edge turning")
    .range(0.0, 1.0);
REXCVAR_DEFINE_DOUBLE(pdz_viewmodel_vertical_shift, 0.0, "Gameplay/Viewmodel",
                      "Vertical offset applied to PDZ's first-person gun visual")
    .range(-3.0, 3.0);
REXCVAR_DEFINE_DOUBLE(pdz_viewmodel_far_threshold, 100.0, "Developer",
                      "Maximum far plane used to identify a viewmodel projection")
    .range(1.0, 10000.0);
REXCVAR_DEFINE_DOUBLE(pdz_mouse_zoom_compensation, 1.0, "Input",
                      "Scoped mouse compensation: 0=off, 1=full")
    .range(0.0, 1.0);
REXCVAR_DEFINE_DOUBLE(pdz_mouse_zoom_sensitivity_scale, 1.0, "Input",
                      "Additional sensitivity multiplier applied only while zoomed")
    .range(0.001, 2.0);
REXCVAR_DEFINE_BOOL(pdz_trace_camera_state, false, "Developer",
                    "Log PDZ camera mode and zoom state changes");
REXCVAR_DEFINE_BOOL(pdz_hide_original_crosshair, false, "Gameplay/Crosshair",
                    "Hide the game's original crosshair drawing");
REXCVAR_DEFINE_BOOL(pdz_custom_crosshair, false, "Gameplay/Crosshair",
                    "Legacy custom overlay crosshair setting (ignored; retained for old configs)");
REXCVAR_DEFINE_BOOL(pdz_half_native_hud, true, "Gameplay/HUD", "Compact native gameplay HUD at 75% size (Classic HUD unchanged)");
REXCVAR_DEFINE_BOOL(pdz_classic_hud, false, "Gameplay/HUD",
                    "Replace native health, armour and ammunition drawing with Classic HUD geometry");
REXCVAR_DEFINE_BOOL(pdz_auto_aim_range_boost, false, "Input",
                    "Optional wider native mouse auto-aim attraction area at close range");
REXCVAR_DEFINE_DOUBLE(pdz_auto_aim_close_boost, 1.5, "Input",
                      "Close-range attraction envelope multiplier; tapers back to normal with distance")
    .range(1.0,2.0);
REXCVAR_DEFINE_BOOL(pdz_ammo_auto_hide, false, "Gameplay/HUD",
                    "Hide Classic ammo HUD after inactivity");
REXCVAR_DEFINE_DOUBLE(pdz_ammo_hide_delay, 5.0, "Gameplay/HUD",
                      "Seconds without ammo activity before hiding Classic ammo HUD")
    .range(1.0, 30.0);
REXCVAR_DEFINE_DOUBLE(pdz_ammo_hud_scale, 1.0, "Gameplay/HUD",
                      "Classic ammo HUD size multiplier; health and crosshair unchanged")
    .range(0.25, 2.0);
REXCVAR_DEFINE_BOOL(pdz_classic_hud_hide_objective, true, "Gameplay/HUD",
                    "Hide in-game objective HUD and update icon independently of Classic HUD");
REXCVAR_DEFINE_BOOL(pdz_hide_interaction_prompts, false, "Gameplay/HUD",
                    "Hide the native interaction action text; actions remain usable");
REXCVAR_DEFINE_STRING(pdz_settings_preset,"custom","Gameplay/Presets",
                    "Last applied enhancements preset; individual edits become custom")
    .allowed({"custom","classic","gamecube","modernized","vanilla","user1","user2","user3"});
REXCVAR_DEFINE_BOOL(pdz_disable_warehouse_door_autoclose,true,"Gameplay",
                    "Disable the seven identified Warehouse door auto-close timers");
REXCVAR_DEFINE_BOOL(pdz_classic_scope, false, "Gameplay/HUD",
                    "Replace weapon scope artwork, including RCP90, with Perfect Dark-style brackets and aiming lines");
REXCVAR_DEFINE_BOOL(pdz_classic_hud_crosshair, true, "Gameplay/HUD",
                    "Use a geometric Classic crosshair with Classic HUD; size zero is a square dot");
REXCVAR_DEFINE_STRING(pdz_crosshair_style, "outlined_dot", "Gameplay/Crosshair",
                      "Crosshair style")
    .allowed({"dot", "outlined_dot", "circle", "cross", "cross_dot", "chevron"});
REXCVAR_DEFINE_DOUBLE(pdz_crosshair_size, 3.0, "Gameplay/Crosshair",
                      "Dot radius or cross arm length")
    .range(0.0, 32.0);
REXCVAR_DEFINE_DOUBLE(pdz_crosshair_gap, 4.0, "Gameplay/Crosshair",
                      "Gap between center and cross arms")
    .range(0.0, 32.0);
REXCVAR_DEFINE_DOUBLE(pdz_crosshair_thickness, 2.0, "Gameplay/Crosshair",
                      "Crosshair line thickness")
    .range(1.0, 8.0);
REXCVAR_DEFINE_DOUBLE(pdz_crosshair_outline, 1.0, "Gameplay/Crosshair",
                      "Black outline thickness")
    .range(0.0, 4.0);
REXCVAR_DEFINE_INT32(pdz_crosshair_opacity, 255, "Gameplay/Crosshair",
                     "Crosshair opacity from 0 to 255")
    .range(0, 255);
REXCVAR_DEFINE_INT32(pdz_crosshair_red, 255, "Gameplay/Crosshair", "Crosshair red channel")
    .range(0, 255);
REXCVAR_DEFINE_INT32(pdz_crosshair_green, 255, "Gameplay/Crosshair", "Crosshair green channel")
    .range(0, 255);
REXCVAR_DEFINE_INT32(pdz_crosshair_blue, 255, "Gameplay/Crosshair", "Crosshair blue channel")
    .range(0, 255);

class PerfectdarkzerorecompApp : public rex::ReXApp
{
public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext &ctx)
  {
    return std::unique_ptr<PerfectdarkzerorecompApp>(new PerfectdarkzerorecompApp(ctx, "perfectdarkzerorecomp",
                                                                                  PPCImageConfig));
  }

  // Override virtual hooks for customization:
  void OnPostInitLogging() override { LoadSettingsFiles(); }
  void OnPreSetup(rex::RuntimeConfig &config) override
  {
    std::string backend = REXCVAR_GET(graphics_backend);
    if (backend != "any" && !config.gpu_plugin.empty())
    {
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, backend);
      if (!config.graphics)
      {
        REXLOG_WARN("graphics_backend '{}' unavailable, falling back to automatic selection",
                    backend);
      }
    }
  }
  // void OnLoadXexImage(std::string& xex_image) override {}
  void OnPostLoadXexImage() override
  {
    PDZ_InitializeInput();
    auto *rt = rex::Runtime::instance();
    uint8_t *base = rt ? rt->virtual_membase() : nullptr;
    if (!base)
    {
      return;
    }
    // Original TU0 forwarding thunk: lwz r3,1384(r3); lwz r11,0(r3);
    // lwz r11,16(r11); mtctr r11; bctr. Register only when codegen missed it.
    // This executes the original virtual call, not a no-op crash suppressor.
    auto* dispatcher = rt->function_dispatcher();
    if (dispatcher && !dispatcher->GetFunction(0x825A8080)) {
      constexpr uint8_t expected[] = {
          0x80,0x63,0x05,0x68,0x81,0x63,0x00,0x00,0x81,0x6B,
          0x00,0x10,0x7D,0x69,0x03,0xA6,0x4E,0x80,0x04,0x20};
      if (std::memcmp(base + 0x825A8080, expected, sizeof(expected)) != 0) {
        REXLOG_ERROR("PDZ forwarding thunk differs from verified TU0 image; refusing repair");
      } else {
        dispatcher->SetFunction(0x825A8080, [](PPCContext& ctx, uint8_t* memory) {
          ctx.fpscr.InitHost();
          const auto load = [memory](uint32_t address) {
            const uint8_t* p = memory + address;
            return (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) |
                   (uint32_t(p[2]) << 8) | uint32_t(p[3]);
          };
          ctx.r3.u64 = load(ctx.r3.u32 + 1384);
          ctx.r11.u64 = load(ctx.r3.u32);
          ctx.r11.u64 = load(ctx.r11.u32 + 16);
          ctx.ctr.u64 = ctx.r11.u64;
          rex::runtime::ResolveIndirectFunction(ctx.ctr.u32)(ctx, memory);
        });
        REXLOG_INFO("PDZ restored original forwarding thunk at 825A8080");
      }
    }
    auto patch_be32 = [base](uint32_t address, uint32_t value) {
      uint8_t *p = base + address;
      rex::memory::PageAccess old_access{};
      rex::memory::Protect(p, 4, rex::memory::PageAccess::kReadWrite, &old_access);
      p[0] = static_cast<uint8_t>(value >> 24);
      p[1] = static_cast<uint8_t>(value >> 16);
      p[2] = static_cast<uint8_t>(value >> 8);
      p[3] = static_cast<uint8_t>(value);
      rex::memory::Protect(p, 4, old_access, nullptr);
    };

    if (REXCVAR_GET(pdz_aspect_ratio_16_9))
    {
      patch_be32(0x820EC158, 0x4018E38E);
    }

    // PDZ TU0's camera constructors all load their default field of view from
    // this shared 58-degree constant. Patching the source constant changes the
    // world camera without touching the HUD projection, avoiding the displaced
    // crosshair caused by scaling every projection frustum.
    constexpr double kPi = 3.14159265358979323846;
    constexpr double kAspect16By9 = 16.0 / 9.0;
    const double horizontal_radians =
        REXCVAR_GET(pdz_horizontal_fov) * kPi / 180.0;
    const float vertical_fov = static_cast<float>(
        2.0 * std::atan(std::tan(horizontal_radians * 0.5) / kAspect16By9) *
        180.0 / kPi);
    patch_be32(0x820EC228, std::bit_cast<uint32_t>(vertical_fov));
  }
  void OnPostSetup() override
  {
    LoadSettingsFiles();
    if (auto* rt = rex::Runtime::instance(); rt && rt->input_system()) {
      // Mouse ownership blocks synthetic gameplay input. Physical controllers
      // remain readable for UI navigation; dialog blockers gate guest reads.
      static_cast<rex::input::InputSystem*>(rt->input_system())->SetActiveCallback(
          [] { return !pdz::UiMouseCaptured(); });
    }

    REXLOG_INFO("PDZ quality-of-life settings: fps60_unlock={}, direct_mouse={}, "
                "mouse_sensitivity={}, zoom_compensation={}, zoom_sensitivity_scale={}, "
                "fov_scale={}, horizontal_fov={}",
                REXCVAR_GET(pdz_fps60_unlock), REXCVAR_GET(pdz_direct_mouse),
                REXCVAR_GET(pdz_mouse_sensitivity),
                REXCVAR_GET(pdz_mouse_zoom_compensation),
                REXCVAR_GET(pdz_mouse_zoom_sensitivity_scale),
                REXCVAR_GET(pdz_fov_scale), REXCVAR_GET(pdz_horizontal_fov));

    if (!REXCVAR_GET(pdz_stub_sweep))
    {
      return;
    }
    PerformStubSweep();
  }
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override
  {
    // F7 leaves the existing V right-stick-click gameplay binding intact.
    third_person::RegisterBinds();
    local_profiles::Init(drawer);
    new PDZOverlayDialog(drawer);
  }
  void OnConfigureFonts(ImFontAtlas* atlas) override
  {
    const auto font_path=SettingsDir()/"pd_numeric.pdnf";
    if(pdz::hud::numeric_font.Load(atlas,font_path))
      REXLOG_INFO("Classic HUD: original Perfect Dark numeric font loaded");
    else
      REXLOG_WARN("Classic HUD: numeric font missing/invalid; using menu font fallback");
  }
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  void OnShutdown() override { local_profiles::Shutdown(); }
  void OnConfigurePaths(rex::PathConfig &paths) override
  {
    // --game_data_root / REX_GAME_DATA_ROOT still win: this cvar is read
    // before any config file loads, so it's already non-empty here if set.
    if (paths.game_data_root.empty())
    {
      paths.game_data_root = RepoRoot() / "assets";
    }

    // Portable distribution: keep saves/achievements beside the game. The SDK
    // supplies a Documents fallback before this hook; override only that default,
    // never an explicit command-line/environment user-data or cache selection.
    if (REXCVAR_GET(user_data_root).empty())
    {
      paths.user_data_root = RepoRoot() / "user-data";
      if (REXCVAR_GET(cache_root).empty())
        paths.cache_root = RepoRoot() / "cache";
    }

    // Hardware/rendering settings become the SDK's primary config file, so
    // the in-game Settings overlay's "Save to config" also lands here.
    paths.config_path = SettingsDir() / "hardware.toml";

    // Input mapping lives in its own file, loaded manually below (the SDK
    // only auto-loads the single path above).
    LoadSettingsFiles();
    // Upstream profile zero preserves the portable root and existing saves.
    // Other profiles change saves only; settings and cache remain shared.
    paths.user_data_root = local_profiles::Load(paths.user_data_root);
  }

private:
  static std::filesystem::path RepoRoot()
  {
    namespace fs = std::filesystem;
    fs::path dir = rex::filesystem::GetExecutableFolder();
    for (int i = 0; i < 6 && !dir.empty() && dir.has_parent_path(); ++i)
    {
      if (fs::exists(dir / "perfectdarkzerorecomp_manifest.toml") || fs::exists(dir / "assets"))
      {
        break;
      }
      dir = dir.parent_path();
    }
    return dir;
  }

  static std::filesystem::path SettingsDir() {
    return pdz::SettingsDirectory(rex::filesystem::GetExecutableFolder());
  }

  static void LoadSettingsFiles()
  {
    namespace fs = std::filesystem;
    fs::path dir = SettingsDir();
    // A clean release starts in Vanilla PDZ. Never reset a returning
    // player's saved preset, sensitivities or preferred inversion.
    if(!fs::exists(dir / "enhancements.toml")) {
      for(const auto& value:pdz::settings::kPresetValues)
        rex::cvar::SetFlagByName(value.name,pdz::settings::Value(value,pdz::settings::Preset::Vanilla));
      toml::table bindings;
      PDZ_DefaultPresetBindings(bindings);
      for(const auto& [key,node]:bindings) {
        std::string value;
        if(auto v=node.value<std::string>()) value=*v;
        else if(auto v=node.value<bool>()) value=*v?"true":"false";
        else if(auto v=node.value<int64_t>()) value=std::to_string(*v);
        if(!value.empty()) rex::cvar::SetFlagByName(std::string(key.str()),value);
      }
      rex::cvar::SetFlagByName("pdz_stick_invert_y","false");
      rex::cvar::SetFlagByName("pdz_gyro_invert_y","false");
      rex::cvar::SetFlagByName("pdz_settings_preset","vanilla");
    }
    for (const char *file : {"hardware.toml", "mapping.toml", "enhancements.toml"})
    {
      if (fs::path p = dir / file; fs::exists(p))
      {
        rex::cvar::LoadConfig(p);
      }
    }
      // Perspective is session state: keep switching available, but always
      // begin in first person even if a previous session saved third person.
      // Saved shoulder offsets, bindings and all other preferences are retained.
      rex::cvar::SetFlagByName("pdz_tp_enable", "false");
#if !defined(PDZ_ENABLE_DEVELOPER_DIAGNOSTICS)
      rex::cvar::SetFlagByName("pdz_timing_log", "false");
      rex::cvar::SetFlagByName("pdz_stub_sweep", "false");
      rex::cvar::SetFlagByName("pdz_trace_camera_state", "false");
#endif
  }

  void PerformStubSweep()
  {
    auto *rt = rex::Runtime::instance();
    auto *fd = rt ? rt->function_dispatcher() : nullptr;
    uint8_t *base = rt ? rt->virtual_membase() : nullptr;
    if (!fd || !base)
    {
      return;
    }

    static FILE *stub_log = std::fopen("logs/stub_sweep.txt", "w");
    static std::mutex stub_mutex;
    static std::unordered_map<uint32_t, uint32_t> stub_hits;

    static PPCFunc *stub = [](PPCContext &ctx, uint8_t *) noexcept
    {
      uint32_t addr = ctx.ctr.u32;
      uint32_t lr = ctx.lr;
      std::lock_guard<std::mutex> lock(stub_mutex);
      uint32_t &count = stub_hits[addr];
      if (count == 0 && stub_log)
      {
        std::fprintf(stub_log,
                     "[stub] addr=0x%08X LR=0x%08X r3=0x%08X r4=0x%08X r5=0x%08X r6=0x%08X\n",
                     addr, lr, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32);
        std::fflush(stub_log);
      }
      ++count;
    };

    uint32_t stubbed = 0;
    for (uint32_t addr = GameConstants::kCodeBase; addr < GameConstants::kCodeEnd; addr += 4)
    {
      if (!fd->GetFunction(addr))
      {
        fd->SetFunction(addr, stub);
        ++stubbed;
      }
    }
    if (stub_log)
    {
      std::fprintf(stub_log, "=== stub sweep: scanned %u addresses, stubbed %u ===\n",
                   (GameConstants::kCodeEnd - GameConstants::kCodeBase) / 4, stubbed);
      std::fflush(stub_log);
    }
  }
};

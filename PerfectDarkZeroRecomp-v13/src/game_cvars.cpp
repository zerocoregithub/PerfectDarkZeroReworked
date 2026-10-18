#include "game_cvars.h"
REXCVAR_DEFINE_BOOL(pdz_cover_camera_stabilization, false, "Experimental/Camera",
                    "Experimental horizontal cover camera anchor; native body, height and exits unchanged")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

// Our app owns backend/FPS/aspect cvars and their patch application.
// Do not compile upstream game_patches.cpp's competing mouse path.

REXCVAR_DEFINE_INT32(pdz_gpu_wait_mode, 1, "Performance",
                     "Pause between polls while the game waits for free command-buffer space: "
                     "0 = busy spin (original), 1 = yield the core, 2 = sleep 200us")
    .range(0, 2)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(dev_debug_runtime, false, "Debug",
                    "Enable runtime debug tools (stub sweep, missing function scan)");

REXCVAR_DEFINE_BOOL(pdz_tp_enable, false, "Gameplay",
                    "Third-person camera (toggle with bind_third_person)")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_DOUBLE(pdz_tp_offset_x, -80.0, "Gameplay",
                      "Third-person camera offset, camera-local X")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_DOUBLE(pdz_tp_offset_y, 10.0, "Gameplay",
                      "Third-person camera offset, camera-local Y")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_DOUBLE(pdz_tp_offset_z, -320.0, "Gameplay",
                      "Third-person camera offset, camera-local Z")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(pdz_tp_auto_fp_gadgets, true, "Gameplay",
                    "Third person: switch to first person while the Data Thief is in use")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_INT32(pdz_tp_zoom_mode, 2, "Gameplay",
                     "Third-person camera while zoomed (scopes, binoculars): "
                     "0 = stay behind, 1 = first person, 2 = slide forward along the view line")
    .range(0, 2)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

REXCVAR_DEFINE_BOOL(pdz_tp_binoculars_fp, false, "Gameplay",
                    "Third person: look through the binoculars in first person "
                    "instead of like scopes (pdz_tp_zoom_mode)")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

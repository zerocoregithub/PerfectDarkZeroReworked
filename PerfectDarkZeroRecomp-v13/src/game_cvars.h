// game_cvars.h - Game-specific CVAR declarations
#pragma once

#include <rex/cvar.h>

REXCVAR_DECLARE(std::string, graphics_backend);
REXCVAR_DECLARE(bool, pdz_fps60_unlock);
REXCVAR_DECLARE(bool, pdz_aspect_ratio_16_9);
REXCVAR_DECLARE(int32_t, pdz_gpu_wait_mode);
REXCVAR_DECLARE(bool, dev_debug_runtime);
REXCVAR_DECLARE(bool, pdz_tp_enable);
REXCVAR_DECLARE(double, pdz_tp_offset_x);
REXCVAR_DECLARE(double, pdz_tp_offset_y);
REXCVAR_DECLARE(double, pdz_tp_offset_z);
REXCVAR_DECLARE(bool, pdz_tp_auto_fp_gadgets);
REXCVAR_DECLARE(int32_t, pdz_tp_zoom_mode);
REXCVAR_DECLARE(bool, pdz_tp_binoculars_fp);

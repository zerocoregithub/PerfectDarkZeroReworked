// perfectdarkzerorecomp - ReXGlue Recompiled Project

#include "generated/default/perfectdarkzerorecomp_init.h"

#include "perfectdarkzerorecomp_app.h"

#include "pdz_restart.h"

static std::unique_ptr<rex::ui::WindowedApp> CreatePDZ(rex::ui::WindowedAppContext& context) {
  pdz::HandleRestartHelper();
  return PerfectdarkzerorecompApp::Create(context);
}
REX_DEFINE_APP(perfectdarkzerorecomp, CreatePDZ)

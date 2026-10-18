# Known limitations — v0.9.0-rc1

- Windows x64 / DX12 is the primary tested path. Vulkan, native Linux/macOS and
  Proton/Steam Deck feature parity are not qualified. An earlier Windows build
  ran on Deck via Proton; native Deck gyro and HDR are not promised.
- Steam desktop mappings may duplicate input when launching outside Steam while
  Steam runs. No universal desktop-input suppression or Steamworks action-set
  integration. Steam and in-game settings are not synchronized.
- Gyro needs an exposed motion sensor. Standard/virtual Xbox input has no gyro.
  Connection/firmware support varies; unsupported Steam touch gates are ignored
  on other selected controllers. Original stick response uses a linear fallback
  when native gyro requires the modern input route.
- Third-person upper-body pose does not track the free-aim cursor. Clearance is
  based on native-eye obstruction, not a new muzzle/projectile-origin simulation.
- GameCube shading is experimental; baked detail and some light/material paths
  remain. Cover-camera stabilization remains experimental. No transparency,
  pickup outline, hologram-distance or working weapon-attachment hiding included.
- HDR is Windows/DX12 SDR-derived scRGB output, not native reconstructed lighting.
- Supported disc executable is US/NTSC-U CRC66DC74B6; USA Rev1 is unsupported.
- No-audio startup has a paced silent fallback. Re-enabling a device can require
  restart; automatic silent-session device recovery isn't implemented.
- Exact rapid-grenade/jungle-projectile crash-trigger regression still needs
  confirmation. No blanket crash-free or measured performance-improvement claim.
- Fresh generation, an isolated full source build and a complete build from the
  delivered source archives have passed engineering checks. Gameplay regression
  and final corresponding-source/notices distribution review remain release gates.
  This candidate is for testing, not approved for upload.

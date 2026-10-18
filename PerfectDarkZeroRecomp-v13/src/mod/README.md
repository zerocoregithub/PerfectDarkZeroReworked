# Third-person camera mod

Perfect Dark Zero is a first-person game, but it already has a third-person
camera: you get it for a moment when you roll. This mod keeps that camera on.
The code is in `third_person.cpp`.

Reworked scoped-camera candidate (16 October 2026): weapon scope zoom overrides
the legacy keep/slide-forward mode with a short eased return to the native eye.
Releasing zoom restores the selected third-person shoulder. Binocular preferences
remain separate. The older design notes below describe the original implementation.

## Current Reworked usage

Press **F7** to switch between first and third person. To use another key,
change `bind_third_person` in `settings/mapping.toml`.
Both stick clicks together toggle perspective; right-stick click/F6 changes
shoulder while third person is active. The game always starts in first person,
even if the previous session saved third person. Shoulder changes are eased.

Use the F8/View+Menu overlay for normal changes. Overlay values are saved under
Data/settings/enhancements.toml in the portable layout; advanced hardware settings
remain separate and enhancement values load later. The older design/debug notes
below are retained as historical upstream context, not current control policy.

| Setting | Default | What it does |
|---|---|---|
| `pdz_tp_enable` | `false` | Current perspective toggle; startup resets it to first person |
| `pdz_tp_offset_x/y/z` | `-80, 10, -320` | Camera-local offsets; negative Z is behind the player |
| `pdz_tp_zoom_mode` | `2` | Legacy zoom preference; weapon-scoped ADS overrides it with the native-eye transition |
| `pdz_tp_binoculars_fp` | `false` | Use first person while looking through the binoculars |
| `pdz_tp_auto_fp_gadgets` | `true` | Use first person while hacking with the Data Thief |

## Original upstream design notes (historical)

These notes explain the camera foundation and earlier experiments. In particular,
the old instant-transition observation is not the current scoped/shoulder policy.

### How it works

- **The gameplay camera.** It is camera mode 0, updated by `sub_8220D9C8`. It
  stores an offset vector at `+160`. When the offset isn't zero, the camera
  moves by it and a collision ray stops it from going through walls. Rolls fill
  this vector in for about a second. The mod writes its own offset into it every
  frame.
- **Where the camera looks.** It doesn't look straight ahead. It looks at a
  point 10 m in front of the eyes (`+16`), so the crosshair lines up with where
  the bullets go.
- **Only the player's camera.** The CamSpy uses the same camera mode. The mod
  checks the camera's target and only changes the player's camera.

## Problems we hit, and fixes

- **The Data Thief screen was missing.** Held items draw either a first-person
  or a third-person model, and the game decides which by checking whether the
  camera is at the eyes. The Data Thief's screen only exists on the
  first-person model. Fix: switch to first person while the Data Thief is
  updating.
- **The CamSpy view flickered.** The offset was applied to the CamSpy's camera
  as well. Fix: only change cameras whose target is the player.
- **The crosshair jumped when zooming.** Jumping into the eyes changes the
  angle you see things from. Moving straight forward also failed, because the
  camera looks at the 10 m point, not straight ahead. Fix: when zoomed, move
  the camera along the line from the camera to that point. The crosshair then
  stays where it was.
- **Binocular lock-on was off to the side.** The lock keeps the target on a
  line from the camera that runs parallel to the player's view. The camera
  looks at the 10 m point instead, which is about 3.5° off that line, and at
  binocular zoom that is a third of the screen. Fix: while locked, aim the
  camera at the target's position (`+48` in the target object). After the lock
  ends, keep looking along that parallel line until the zoom ends.

## What didn't work

- **Firing our own ray along the player's view to find the distance.** The
  binoculars' ray passes through characters, so the distance kept jumping
  between the target and the background, and the camera shook.
- **Always using the real distance instead of 10 m.** With the camera 80 cm to
  the side, every jump in distance, say from a nearby wall to the far
  background, made the camera turn on its own by up to about 5°.
- **Smooth transitions.** We switch views instantly on purpose: it felt better.

## How we found all this

1. Using memory dump.
2. Searched it for strings and followed them to code. Task names like
   `CameraControlUpdate` sit in a table next to their functions.
3. Read the generated C++, which keeps the original PowerPC instructions as
   comments.
4. Hooked functions with `REX_HOOK_RAW`, logged what they did while playing,
   and compared the logs with what we did in game. Most of the real answers
   came from these logs, not from reading code: for example the lock flag
   (`+1384` in the binoculars object) and where the target's position is kept.

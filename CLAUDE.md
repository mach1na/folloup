# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Folloup is an ESP-IDF C++17 firmware application for the [Waveshare
ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97) (800x480
mono e-paper, buttons only — no touch controller, ES8311 audio codec + NS4150B
amp, AXP2101 PMIC, QMI8658 IMU, PCF85063 RTC, microSD over SDMMC). Followup is
a voice-first thought-capture app: press and hold to record, tag it as
Idea/To-do/Note, Gemini transcribes/summarizes it over Wi-Fi, everything is
stored on the SD card, and follow-ups are pinned to the e-paper as stickies.

The codebase began as a port targeting the Seeed reTerminal Sticky (see the
`folloup-sticky` branch/fork for that target) and parts of the docs still
describe that lineage where the design rationale carried over, but **Waveshare
is the only target this firmware builds for today**.

Read `docs/app-architecture.md` before changing firmware architecture,
component boundaries, AXP2101/PMIC integration, board wiring, partition
layout, or ESP-IDF configuration — it is the authoritative, very detailed spec
(component boundaries, screen/overlay model, refresh policy, task mapping,
GPIO map, dependency direction, and a Sticky-vs-Waveshare differences table up
top). This CLAUDE.md is a condensed map; when the two disagree,
`docs/app-architecture.md` wins. Board-specific pin/electrical detail also
lives in `docs/waveshare-epaper-hardware-spec.md`.

## Commands

This is an ESP-IDF project; there is no unit test suite. **Do not automatically
run builds** — only build when the user explicitly asks, and reuse the
existing `build/` directory rather than creating a new one (see `AGENTS.md`).

An ESP-IDF environment must be sourced first (`idf.py` on `PATH`, or set
`IDF_PATH` to a v5.x ESP-IDF checkout). Helper scripts under
`.agents/skills/esp32-firmware-engineer/scripts/` wrap `idf.py` with
environment auto-detection, port auto-detection, and target sync:

```bash
./.agents/skills/esp32-firmware-engineer/scripts/build.sh              # idf.py build
./.agents/skills/esp32-firmware-engineer/scripts/build.sh --strict-warnings  # fail if build.log has any "warning:"
./.agents/skills/esp32-firmware-engineer/scripts/flash.sh              # build + flash (auto-detects serial port; PORT=... to override)
./.agents/skills/esp32-firmware-engineer/scripts/monitor.sh            # idf.py monitor
```

That build wrapper also runs a generic ESP-ADF/ESP-SR compatibility preflight
that false-positives on this repo (it matches keywords inside its own bundled
skill docs, not this project's source — this project uses neither ESP-ADF nor
ESP-SR). Pass `SKIP_PLUGIN_COMPAT_CHECK=1` to skip it. On a fresh checkout
with no `sdkconfig` yet, also pass `ESP_TARGET=esp32s3` (or the wrapper's
`set-target` step will fail to auto-clean a `build/` dir that already has a
`build.log` in it).

Equivalent raw commands (target is `esp32s3`):

```bash
idf.py build
idf.py -p <PORT> -b 460800 flash
idf.py monitor
idf.py menuconfig   # project options live under "Folloup Settings"
```

Asset regeneration (icons/logos/fonts are generated C++ compiled into
`components/project_assets`; **never hand-edit the generated files**):

```bash
python3 scripts/generate_epaper_project_assets.py   # manifest-driven, regenerates everything from assets/epaper_assets.json
```

See `scripts/README.md` for the lower-level per-asset-type generator scripts
and `docs/asset-generation.md` for the pipeline.

### Setup portal (`webserver/`)

A separate TypeScript + Vite web app, embedded into firmware and served by
`wifi_service`'s HTTP server while the device is in AP setup mode (Wi-Fi
scan/connect, Gemini API key, time/timezone). It is *not* part of the ESP-IDF
build graph — after any frontend change it must be built and its output
manually copied into the component:

```bash
cd webserver
npm install
npm run dev          # vite dev server at http://localhost:5173 (no backend; point at a real device's AP for live API calls)
npm run build         # tsc -b && vite build -> dist/
npm run lint
cp dist/index.html dist/index.js dist/index.css ../components/wifi_service/portal/
```

`webserver/` is the source of truth; `components/wifi_service/portal/` is
tracked, generated build output — don't hand-edit it. See `webserver/README.md`
for the source layout (`src/components/` web components, `src/portal/`
feature controllers/API helpers).

## Architecture

### Layout

- `main/` — product composition only (not a reusable component). `main.cpp` is
  a one-line `app_shell::Run()` call. `app_shell.cpp` is an **orchestration
  layer only**: startup ordering, event wiring, policy composition — no
  hardware/protocol/display/power logic (see `AGENTS.md`). Everything else
  under `main/` is an "app-runtime helper" that composes service state into
  neutral UI/interaction contracts; each feature page has a
  `{runtime, coordinator, interactions}` trio (older pages — settings/wifi/time
  — predate the coordinator split).
- `components/` — generic hardware drivers (`pcf85063`, `qmi8658`, `sd_card`,
  `epaper_panel`) are board-agnostic and must not depend on `board`. `board`
  (`waveshare_board`) centralizes Waveshare-specific pin/bus assignments, PMIC
  rail bring-up, and the shared audio codec instance. App-facing `*_service`
  components compose `board` + a driver into product policy (e.g.
  `display_service` = `board` + `epaper_panel`; `power_service` = `board` +
  `axp2101` + `pcf85063`).
- `epaper_ui` — reusable e-paper presentation primitives (status bar, footer,
  modals, toast, keyboard, timeline list, sticky note, ~55 widgets) *and* all
  full page renderers. It depends on `design_tokens` + `project_assets` only,
  never on app services — page renderers live here (not in `main/`) because
  `display_service`, a component, draws them and cannot depend on `main`.
- `design_tokens` — header-only shared spacing/grayscale/typography/sizing
  constants.

### Dependency direction

```
app/main -> power_service -> board -> axp2101 / pcf85063 -> ESP-IDF I2C/GPIO drivers
         -> feedback_service -> system_sound_service -> audio_hal -> ESP-IDF I2S driver
         -> storage_service -> sd_card -> ESP-IDF SDMMC/FATFS drivers
         -> recording_service -> board (GetAudioCodec) -> audio_hal
         -> playback_service -> board (GetAudioCodec) -> audio_hal
         -> display_service -> board -> epaper_panel -> ESP-IDF SPI/GPIO drivers
         -> imu_service -> board -> qmi8658 -> ESP-IDF I2C driver
```

`epaper_ui` -> `design_tokens` + `project_assets` (never app services).
Full detail: "Dependency Direction" in `docs/app-architecture.md`.

### Screens, overlays, and the refresh system (the trickiest part of this codebase)

- Screens (`ScreenId`) are mutually exclusive full-screen underlays: `kHome`
  (dashboard), `kOnboarding`, `kVibeCheck`, `kSummarize`, `kNotes`/`kTodos`/
  `kFollowUp` (two-level timelines), `kDetails`, `kSettings`, `kWifi`, `kTime`,
  `kLockScreen`.
- Overlays composite on top, z-order keyboard → toast → select modal → card
  modal → sticky note. Every page-owned screen implements a shared touch
  contract: `resolve_touch_target` / `focus_touch_target` /
  `activate_touch_target`, dispatched centrally in `main/page_input_runtime.cpp`
  — this contract is vestigial UI-focus plumbing inherited from the Sticky
  port (**this board has no touch controller**, so nothing dispatches real
  touch events; all input is buttons only).
- All repaints route through `main/ui_refresh_runtime.cpp`, a keyed
  latest-wins worker (one `SurfaceKey` per page + `kOverlay`/`kLockScreen`/
  `kStatusBar`/`kFooter`). **The overlay rule**: while `overlay_runtime::
  IsInputCaptured()` is true, underlay (page/status/footer) refreshes are
  suppressed — state still updates, only the screen repaint is skipped — so a
  modal/keyboard doesn't cause laggy underlay rebuilds or starve the idle task.
  Every new `SurfaceKey` needs its own `SurfaceIndex`, or it silently aliases
  onto slot 0 and can clobber another surface's pending refresh.
- **Boot refresh policy**: the very first panel paint must be a single full
  refresh of the first screen (onboarding or home). Every service-event
  handler that could repaint during startup must gate on `s_startup_complete`
  (via `ScreenActiveForRefresh`) so nothing partial-refreshes before that.
- **The SSD1677 panel cannot do windowed/region partial refresh** — this was
  proven empirically and against the datasheet. "Partial refresh" here always
  means change-detected *whole-screen* partial (diff against a retained shadow
  framebuffer, drive only changed pixels). Do not attempt to reintroduce
  windowed refresh. Portrait `x` maps onto the panel's gate line
  (`raw_y = height - 1 - x`), which is why a large fill with a gate-periodic
  dither pattern produces visible banding on a partial refresh.
- Input precedence: overlay focus first, then footer, then page targets.
  `UP`/`DOWN` press-down move roving focus; long-press `DOWN` is the app-wide
  "exit entered control" gesture; `ACTION`/BOOT or the rocker-middle `FN`
  single-click activates/submits; press-and-hold `ACTION` arms/starts/stops a
  recording. `PWR` is not a GPIO button — it's wired to the AXP2101 and
  surfaces as PMIC interrupts (short press, long press, and a hardware-forced
  6s rail cut).

### Recording flow (press-and-hold, with review playback)

`recording_session_service` owns the whole take, as a phase machine:

```
kIdle -> kArmed -> kStartCue -> kRecording -> kStopCue -> kPlayingBack
      -> kAwaitingTagSelection -> kSaving -> kTranscribing -> kComplete
```

Capture starts *before* the start cue finishes playing (waiting would swallow
the speaker's first word) — a release during `kStartCue` defers the finish
instead of dropping it. After the stop cue, the take is replayed to the user
from PSRAM via `playback_service::PlayClip` *before* it's saved — this is why
review comes before save: the `Discard` option in the tag menu can throw away a
bad take without it ever reaching SD. Auto-sleep is blocked while
`playback_service::IsPlaying()`.

### No touch controller

This board has no touch hardware. Some widget code still carries `kTouch*`
hit-slop constants and a `kTouchContact` feedback cue inherited from the
Sticky port — they're vestigial and nothing dispatches real touch events. Do
not build new interaction paths on the touch contract; all real input on this
board comes through `button_service`.

### Power / shutdown (AXP2101)

There is no power latch on this board and no shared SPI bus to arbitrate — the
AXP2101 PMIC holds every rail (DC1/ALDO1-3), and the e-paper panel owns a
dedicated `SPI3_HOST` outright (no bus guard needed, unlike the Sticky's shared
SPI2). `power_service::RequestShutdown()` clears PCF85063 alarm/timer
interrupts, then calls `Axp2101::PowerOff()`, which cuts every rail. On battery
the board goes dark immediately; while VBUS (USB) is present the PMIC keeps
the rail fed and the call can return with the board still powered — unplugging
USB completes the power-down. Light sleep needs no latch preservation either:
the AXP2101 holds rails across `esp_light_sleep_start()` on its own.

### Task/core mapping

App FreeRTOS tasks use the shared table in
`components/task_config/include/followup_task_config.h`. CPU0 = system/network
(Wi-Fi, timezone sync — alongside ESP-IDF's own main task/esp_timer/Wi-Fi
driver); CPU1 = product hardware/UI (audio capture, storage, sound feedback,
sleep-driven display transitions, PMIC IRQ servicing). Add new tasks to
`task_config` with a one-line ownership rationale rather than hardcoding
priority/core literals. Full priority table: "Task Mapping" in
`docs/app-architecture.md`.

### Auto-sleep

`device_sleep_service` (component) owns policy/state/timers only — no display,
GPIO, or ESP sleep hardware. `main/device_sleep_runtime.cpp` owns the actual
IMU-polling (QMI8658, via `imu_service`), display-sleep, and light-sleep
mechanics. See `docs/auto-sleep.md`.

### Audio (ES8311 codec, full duplex)

`audio_hal` wraps the ES8311 codec over I2S0 at a single 16 kHz full-duplex
clock, chosen to match the recording/Gemini pipeline so nothing needs
resampling. `board` keeps codec output (and the NS4150B power-amp enable) on
for the codec's lifetime — per-event PA toggling was rejected because cues and
clip playback share the one output and toggling clipped whichever stream
started second. `system_sound_service` owns the decoded sound-cue catalog and
serializes cue playback; `feedback_service` maps app-level events (startup,
lock/unlock, button clicks, shutdown, errors) onto those cues without exposing
codec details to `app_shell`. `playback_service` streams a recorded clip to
the codec (from SD or straight from `recording_service`'s PSRAM chunks) for
the Details-page Play button and the post-recording review step.

### Gemini integration

`gemini_service` owns API key precedence/readiness; `transcription_service`
and `summary_service` are separate components (not nested inside
`gemini_service`) that own the actual Gemini-backed flows. See
`docs/gemini-service.md`.

### NVS namespaces / build-time Kconfig

Runtime settings persist under service-owned NVS namespaces: `wifi`
(`ssid`, `password`), `timezone` (`enabled`, `tz_name`, `location`, `time_src`,
`ntp_sync`, `ntp_epoch`). Build-time defaults live under `Folloup Settings`
(`idf.py menuconfig`): `CONFIG_FOLLOWUP_WIFI_*`, `CONFIG_FOLLOWUP_TIME_SYNC_DEFAULT_ENABLED`,
`CONFIG_FOLLOWUP_DEFAULT_TIMEZONE_NAME`, `CONFIG_FOLLOWUP_GEMINI_API_KEY`,
`CONFIG_FOLLOWUP_AUTO_SLEEP_*_TIMEOUT_SECONDS`. Saved NVS Wi-Fi credentials
always take precedence over built-in sdkconfig credentials.

## Working conventions

- Keep `main/app_shell.cpp` an orchestration layer — before adding logic
  there, ask whether it belongs in a service/component or a focused runtime
  helper instead (`AGENTS.md`).
- Generic drivers under `components/` (`pcf85063`, `qmi8658`, `sd_card`,
  `epaper_panel`) must stay board-agnostic: take an already-initialized
  bus/device handle and pins/config from the caller, never read `board`
  directly. `axp2101` and `audio_hal` stay app-agnostic in the same sense —
  what a power-key press or a sound cue *means* belongs in `main/` or
  `feedback_service`, not the driver.
- Don't hand-edit generated files: `components/project_assets/generated_*`,
  `components/epaper_ui/generated_epaper_fonts.*`, and
  `components/wifi_service/portal/*` (regenerate via the scripts/build steps
  above instead).
- `GPIO0` (`ACTION`/BOOT button) is the boot/download strap pin and must read
  high at reset — it's only ever pulled low by a button press, never held
  during startup.
- `docs/user-manual.md` is the end-user manual and must stay in sync with
  behavior: whenever a change adds, removes, or changes user-visible
  functionality (a new screen, a changed control/gesture, a renamed
  button, a different flow), update the relevant section of the manual as
  part of that change, not as a follow-up. It describes actual behavior,
  not code — write it in plain, user-facing terms.
- Every fix, feature, or `docs/todo.md` item gets its own branch — don't
  stack unrelated work onto an existing branch or work directly on `main`
  (the user-manual and todo-cleanup docs-only pushes are the narrow,
  explicitly-requested exception, not the default).
- Followup uses Semantic Versioning — see `docs/versioning.md`. Every fix or
  feature branch that changes firmware/webapp behavior bumps `version.txt`
  (MINOR for a feature, PATCH for a bug fix) and adds a `CHANGELOG.md` entry
  under `## [Unreleased]` as part of that same change.
- When a `docs/todo.md` item is resolved, move its entry to
  `docs/todo-archive.md` (verbatim, with a resolution note) as part of the
  same change that resolves it, rather than just deleting or checking it
  off in place.

# TODO

## ~~Defer the ghosting flush to idle instead of firing mid-interaction~~ — resolved

The SSD1677 driver used to force a full-waveform refresh inline the moment
the 8th consecutive partial refresh was requested (`kMaxPartialRefreshesBeforeFlush`
in `components/epaper_panel/ssd1677_driver.cpp`), so arrowing through more
than 8 items in a menu/timeline triggered a full, flashy refresh
mid-navigation.

Split into a soft budget and a hard ceiling:
- `EpaperPanel::NeedsGhostingFlush()` turns true once the soft budget (still
  8) is reached, but no longer forces anything by itself.
- `display_service::DisplayTask` polls its command queue with a short timeout
  (`kGhostingFlushIdleMs`, 500ms) once a flush is pending; a gap that long
  with no new command means input has paused, and that's when the deferred
  full refresh (`RefreshCurrentScreenLocked(RefreshMode::kFull)`) actually
  runs — invisible to active navigation.
- `kHardPartialRefreshCeiling` (2.5x the soft budget, 20) still forces the
  flush inline in `RefreshPartialFullScreen` if input never pauses long
  enough, so ghosting can't grow unbounded under continuous input.

Verified on-device: idle-deferral fired during a real pause after 8 partials
(full-refresh signature: `reset≈50ms`/`init≈9.9ms` vs `0`/`0` for a partial),
and the hard ceiling fired inline during a sustained run where presses never
left a 500ms gap.

Also fixed as a drive-by: the stale comment on `kMaxPartialRefreshesBeforeFlush`
claiming "the flush is now on the fast waveform" (it isn't — reverted in
`ae15b27` back to the slow mode-1 waveform since the fast OTP waveform
settled at visibly lower contrast).

## ~~Display sleep never actually blanks the panel~~ — resolved, docs corrected

Investigated: `docs/auto-sleep.md` used to document the display-sleep
sequence as refresh-to-blank -> wait -> sleep. The actual code
(`SleepPanelLocked` in `components/display_service/display_service.cpp`) only
ever repaints the status-bar sleep indicator (the little leaf icon) before
calling `panel.Sleep()` — it never blanks the page content, so the panel
freezes on its last screen and powers down in that state.

Craig confirmed this is the preferred behavior, not a bug: a frozen screen
signals "powered down but holding state" more usefully than a blank one,
which is indistinguishable from off/broken, and it avoids spending an extra
refresh cycle against the panel's ghosting budget
(`kMaxPartialRefreshesBeforeFlush`). Docs updated to describe this as
intentional instead of a spec the code fails to meet — no code change needed.

## ~~Battery usage investigation~~ — closed for now

Looked at overall battery efficiency: active draw (audio codec kept on for
its lifetime per `docs/app-architecture.md`, Wi-Fi, display refresh
frequency) and sleep-path draw (light sleep / display sleep via
`device_sleep_service` + `main/device_sleep_runtime.cpp`, AXP2101 rail
behavior).

Investigated: the AXP2101 driver has no current-sense ADC at all (only
voltage/percent/temperature readback — `getBatteryPercent()` is a pure
voltage-curve estimate, not a coulomb counter), so there's no way to get real
mA numbers from software. Actual before/after measurement needs external
hardware (a USB inline power meter or multimeter).

Fixed: Wi-Fi never entered any power-save mode, including during light
sleep. `wifi_service.cpp` hard-disables Wi-Fi power save
(`esp_wifi_set_ps(WIFI_PS_NONE)`), and light sleep never disconnected or
stopped Wi-Fi first — so for the whole light-sleep window (up to 30 minutes
by default) the radio stayed fully associated at full power, undermining much
of the point of that sleep state. `wifi_service::PrepareForLightSleep()` now
stops the Wi-Fi radio before `esp_light_sleep_start()`
(`main/device_sleep_runtime.cpp`'s `EnterLightSleep()`); the existing
`RecoverAfterLightSleep()` reconnect path (already built to handle a
dropped-during-sleep link) reconnects on wake, unmodified. Verified on-device:
serial log confirms real `esp_wifi_stop()` hardware teardown right before
sleep, and the status bar's Wi-Fi/Gemini icon (frozen during sleep like
everything else on an e-paper display, per the resolved item above) shows
connected again once awake.

Closed here, not because there's nothing left, but because Craig is going to
use the device with the fix above for a while and re-raise if battery life
still doesn't feel right. Known not-done, for whenever this reopens:
- Audio codec + amp + I2S DMA stay powered for their entire lifetime, even
  sitting idle — this is a deliberate, already-reasoned tradeoff (see
  "Audio (ES8311 codec, full duplex)" in `docs/app-architecture.md`,
  per-event PA toggling was rejected), not touched here.
- Display refresh frequency / any further sleep-path tuning.
- Actual measured runtime/current numbers, pending external measurement
  hardware.

## Reorder footer icons for usability

The bottom footer icon order isn't intuitive — revisit the order to make it
more user-friendly. Needs a proposed new order (from Craig or from usage
patterns) before implementing; footer rendering lives in `epaper_ui`.

Own branch/PR.

## ~~Tone down the summarize/vibe-check prompts~~ — resolved

Found two spots with a "chummy" tone:

- `summary_service::BuildSummaryInstructionText` (`components/summary_service/summary_service.cpp`):
  the final (non-intermediate) notes/todos prompts told Gemini to write in an
  "encouraging and optimistic tone" that's "motivating to look back on" /
  "celebrates progress" — dropped in favor of "concise and factual."
  Intermediate/rollup prompts were already factual and untouched.
- `vibe_check_page_coordinator.cpp`: the Vibe Check page's static UI copy
  (`kMessageText`, `kEmptyStateMessage`) had the same overly-cute tone
  ("Some thoughts are passing vibes... what still hits", "Get the ball
  rolling!") — reworded to plain, neutral copy.

## ~~Offline transcription queue doesn't actually work~~ — resolved

Auto-retry for offline-recorded notes (commit `3366004`) had three separate
bugs stacked on top of each other:

1. **Offline recordings were never flagged as pending in the first place.**
   `recording_session_service` decided "was this saved offline" by checking
   `gemini_service::GetSnapshot().runtime.ready` alone
   (`recording_session_service.cpp`) — but `ready` means "configured and has
   authenticated at some point," not "reachable right now"; it never resets
   on a later Wi-Fi disconnect. So a note recorded with Wi-Fi off (after any
   prior successful boot-time auth) looked "ready," transcription was
   attempted anyway, failed on the dead network, and — since a failed
   attempt never sets `pending_transcription` — the recording ended up
   indistinguishable from a generic API failure: not counted, not retried.
   Fixed: added `recording_session_service::SetNetworkConnected()` (mirrors
   `gemini_service`/`timezone_service`'s existing network hooks), called from
   `app_shell::HandleWifiEvent`, and ANDed with `gemini_ready` at the one
   decision point that already gated both the pending-flag and the
   attempt-now-or-not choice.
2. **No automatic retry trigger when Wi-Fi reconnects.**
   `transcription_retry_service::RetryPending()` had exactly one caller,
   gated on a Gemini-ready false→true edge in `app_shell.cpp` — but that
   edge only ever fires once, at the first boot-time auth, since `ready`
   doesn't reset on disconnect (see above). Fixed: `HandleWifiEvent` now also
   calls `RetryPending()` directly on a Wi-Fi disconnected→connected edge
   (`RetryPending()` is already a safe no-op if nothing's pending or a batch
   is already running).
3. **Retry batch always "failed" even when the transcript came through.**
   Found while verifying fix #2 on-device: every retried item took exactly
   the 40s poll timeout and was marked failed, then its transcript showed up
   moments later anyway. Root cause: `RunTranscriptionRetryJob` ran on
   `gemini_service`'s single shared worker task and then *blocked* on that
   same task waiting for `BeginTranscription`'s result — but
   `BeginTranscription`'s actual HTTP work is itself queued onto that same
   worker, so it could never get a turn to run until the blocking wait gave
   up. Fixed: the retry batch now runs on its own dedicated one-shot task
   (`kPriorityTranscriptionRetry` in `followup_task_config.h`, mirroring
   `vibe_check_page_runtime`'s `TranscribeWorker` pattern) — `BeginTranscription`'s
   own `request_in_flight` guard (set synchronously before it enqueues
   anything) still keeps it from running concurrently with a live user
   transcription.

Also added the missing badge: a `kFile` icon in the status bar, visible only
while `pending_transcription_count > 0`, with the count rendered directly
inside the icon's blank interior rather than a separate corner-badge pill —
a first pass overlaid a badge widget at the icon's corner, but on the
status bar's outermost icon that pushed part of it off-screen; there's also
an existing, fully-wired but never-shown badge on the footer's "folder"
button (`footer_runtime.cpp`) that was considered and rejected in favor of
this, since that button has no destination screen and enabling it made a
permanently-visible dead button.

Verified on-device end-to-end: two notes recorded with Wi-Fi off were
correctly flagged and not attempted (`gemini_ready=0`); on reconnect, retry
started within 1s (previously would never have retried at all) and both
items completed and saved their transcripts within ~5-6s each — no more
40s timeout, `attempted=2 succeeded=2 failed=0`.

# Whole-codebase review findings (2026-09-06)

A full-codebase security + best-practices pass (security audit clean — no
findings cleared the exploitability bar; checked the AP portal's payload
handling, TLS cert validation, SD/JSON parsing, path handling, secrets).
Everything below is from the best-practices/correctness/efficiency/reuse
side. Each item below is its own branch/PR.

## ~~Dead "has audio" guard on Play buttons~~ — resolved

Notes/Todos/Follow-up's "Play recording" modal option
(`main/notes_page_runtime.cpp:291`, `main/todos_page_runtime.cpp:293`,
`main/follow_up_page_runtime.cpp:282`) and the Details page's "Play" button
(`main/details_page_coordinator.cpp:224`, `main/details_page_runtime.cpp:305-330`)
all guarded on `recording_path.empty()` — but `recording_path` is a
*constructed* path (`base_path + ".wav"`, set unconditionally in
`recording_archive_service.cpp:797`, `435`, `515`), never checked against
the filesystem, so it was never empty for any listed entry.

Failure scenario: the device supports USB-OTG SD access (Settings ->
storage). If a user deletes/moves a `.wav` off-device over OTG but leaves
the `.json`/`.txt` behind, on reconnect the recording still listed with a
non-empty but dangling `recording_path`. "Play"/"Play recording" still
showed; selecting it called `playback_service::PlayFile` on a missing
file, which failed silently (`ESP_LOGW` only, no user-facing toast).

Fixed: added `RecordingEntry::has_audio_file`, checked once via `stat()`
during the archive scan (`recording_archive_service.cpp`, reusing the
existing `FileExists` helper already used elsewhere in that file) rather
than trusting path non-emptiness. Threaded through each page's own
`TimelineEntry` copy (notes/todos/follow-up) and the Details page
coordinator; the Details page's primary action button is now hidden
entirely (both in render state and in the roving-focus navigation model)
when a transcript exists but the audio itself doesn't, since "Play" would
otherwise have nothing to do. Verified on-device: normal playback from
both the Notes-list "Play recording" action and the Details page "Play"
button still complete cleanly (no regression).

## ~~Two `s_startup_complete` gating gaps~~ — resolved

Every sibling event handler in `main/app_shell.cpp` (`HandleRecordingEvent`,
`HandleTimezoneEvent`, `HandleGeminiEvent`) wraps its display update in
`s_startup_complete.load(...) ? UpdateDisplayStateAndRequestRefresh(...) :
UpdateDisplayState()` to avoid a partial refresh racing the mandatory first
full-screen paint. Two places didn't:

- `HandleRecordingArchiveEvent` (`app_shell.cpp:1571-1585`) called the
  partial-refresh variant unconditionally whenever
  `pending_transcription_count` changes. `ShowHomeScreen`'s
  `recording_archive_service::RefreshAsync()` (kicked off at
  `app_shell.cpp:213-224`, before `s_startup_complete` is set at line 1852)
  runs on its own task and could call this handler before or concurrently
  with the boot's first full refresh.
- `lock_screen_runtime::SyncClockState(true)`, called unconditionally from
  `HandleTimezoneEvent` (`app_shell.cpp:1122`, no `s_startup_complete`
  check at all, unlike the status-bar refresh a few lines later in the same
  function), schedules its own partial refresh
  (`lock_screen_runtime.cpp:108-121`) gated only on the module's local
  `s_active` flag. `power_key_runtime::Init()` (wired to the PMIC IRQ) is
  live well before `s_startup_complete` flips, so a power-key press
  followed by an early NTP-driven timezone event could trigger this.

Fixed: gated both the same way every other handler already does —
`HandleRecordingArchiveEvent`'s status-bar refresh now uses the same
`s_startup_complete` ternary as its siblings, and `SyncClockState`'s
`request_refresh_if_active` argument is now `s_startup_complete.load(...)`
instead of a bare `true` (passing `false` still rebuilds/pushes state, it
just skips requesting a repaint — the same "apply state now, paint later"
semantics the other handlers already rely on). Verified on-device: clean
boot with no regressions.

## ~~Auto-sleep's playback blocker isn't re-checked right before sleeping~~ — resolved

`GetAutoSleepBlocker` (`main/device_sleep_runtime.cpp:132-172`) correctly
checks `playback_service::IsPlaying()`, but once light sleep is dispatched,
`EnterLightSleep()`'s `WaitForPowerButtonReleased()`
(`device_sleep_runtime.cpp:243-270`) could poll for up to 5s (250 samples x
20ms) before `esp_light_sleep_start()` actually ran, without re-checking
`IsPlaying()`. Starting playback in that window meant the device could
enter light sleep (killing Wi-Fi, suspending buttons) mid-playback.

Fixed: re-check `GetAutoSleepBlocker(nullptr)` immediately before the
actual `esp_light_sleep_start()` call (after the Wi-Fi stop and display
transition, right before the point of no return), aborting entry via the
existing `AbortLightSleepEntry` if anything now blocks it — that function
already unwinds the Wi-Fi stop and display transition via
`RestoreAfterLightSleep()`, so it's safe to call this late in the
sequence. Verified on-device: normal light-sleep entry still proceeds
without a spurious abort. The exact race (playback starting in the ~ms
window right before the check) wasn't specifically forced/reproduced.

## ~~Unsynchronized interrupt callbacks in axp2101.cc and qmi8658.cc~~ — resolved

Both `components/axp2101/axp2101.cc` and `components/qmi8658/qmi8658.cc`
stored their interrupt callbacks as bare `std::function` members
(`interrupt_callback_` at `axp2101.cc:145-147`; `interrupt2_callback_` /
wake-on-motion / tap callbacks at `qmi8658.cc:484-486,983-1017`), set from
one task and read/invoked from each driver's own `InterruptTask` with no
lock — a genuine data race if a callback is ever re-attached at runtime.
For `qmi8658` specifically, `Qmi8658::Update()` (a public polling method,
not just the INT2 task) also dispatches these same callbacks, so two
different tasks really could race on them.

Separately, both interrupt tasks start during early construction/`Initialize()`
(`axp2101.cc:51-65`; `qmi8658.cc:183-192`) — well before the app code that
attaches the real callback (`power_key_runtime::Init()` at
`app_shell.cpp:1756` for the PMIC). Any power-key press or VBUS event in
that window was read, decoded, and `clearIrqStatus()`'d with no callback
attached — silently lost.

Fixed: added a dedicated `callback_mutex_` to each driver guarding every
callback member; every `SetXxxCallback()` setter now locks to assign, and
every read site copies the callback out under the lock into a local
before invoking it (rather than holding the lock for the call, since the
callback's duration isn't bounded by the driver). For `qmi8658`, this
meant threading local copies of all nine `EventCallback` members through
`DecodeStatus()` (copied once under one lock acquisition at entry when
`dispatch_callbacks` is true) rather than reading the members inline
throughout that function. Also added a log line for a dropped early-PMIC
IRQ (the "at minimum logging" half of the suggested fix; did not add
event buffering/replay — a bigger change for a boot-window-only gap).

Verified on-device: clean build, clean boot, no regressions. Confirmed via
grep that none of `qmi8658`'s callback setters are actually called
anywhere in the app today (`imu_service` only uses `Qmi8658::ReadSample()`
directly) — that half of the fix is preventive/correctness-only for
currently-unused API surface, not independently testable right now. The
`axp2101` power-key callback path is live but unchanged in behavior (pure
synchronization wrapper).

## ~~`volatile bool` used for cross-task signaling in timezone_service.cpp~~ — resolved

`s_sntp_sync_seen` (`timezone_service.cpp:134`) was a plain `volatile bool`,
set from the SNTP/LWIP callback task (`OnSntpTimeSync`, line 506) and read
from whatever task calls `SyncNow` (~line 1052). `volatile` gives no
cross-thread visibility/ordering guarantee in the C++ memory model, unlike
the `std::atomic`/mutex pattern used everywhere else in this codebase for
the same purpose (e.g. `recording_session_service.cpp:70`'s
`s_network_connected`). Could cause a spurious "time sync failed" report
immediately after a real sync succeeds.

Fixed: changed to `std::atomic<bool>` (relaxed ordering, matching the
existing pattern elsewhere). Verified on-device: NTP sync still succeeds
normally.

## ~~Latent abort-on-error hazard in i2c_device.cc (currently dead code)~~ — resolved

`WriteRegOrDie`/`ReadRegOrDie` (`components/i2c_device/i2c_device.cc:58-66`)
wrapped register access in `ESP_ERROR_CHECK`, which calls `abort()` on any
non-OK result — contradicting the "transient I2C contention is expected"
handling used elsewhere on the same shared bus (e.g.
`power_service.cpp`'s `FillRtcStatus` downgrades a failed read to
`ESP_LOGD` rather than crashing). Confirmed unused by both
`axp2101`/`qmi8658` (and everything else in the tree), so this was a
foot-gun rather than an active bug.

Fixed: removed both functions (declaration + implementation) rather than
softening them, since nothing used them.

## ~~Hardcoded task priority literal in wifi_service.cpp~~ — resolved

`xTaskCreate(CaptiveDnsTask, "captive_dns", 3072, nullptr, 5, &s_dns_task)`
(`components/wifi_service/wifi_service.cpp:696`) passed a raw priority
literal instead of a named `followup_task_config::kPriority*` constant —
the one outlier against CLAUDE.md's "add new tasks to task_config with a
one-line ownership rationale" rule; every other task in the tree did this
correctly.

Fixed: added `kPriorityCaptiveDns = 5` (same value, now named, with a
one-line rationale) to `followup_task_config.h` and used it at the call
site. Pure rename — no behavior change.

## ~~Onboarding-viewed flag persisted directly in app_shell.cpp~~ — resolved

`main/app_shell.cpp:358-388` (`kOnboardingNvsNamespace`, `kOnboardingNvsKey`,
`OnboardingViewed()`, `MarkOnboardingViewed()`) called `nvs_open`/`nvs_get_u8`/
`nvs_set_u8`/`nvs_commit` directly, unlike every other piece of persisted
app state (Wi-Fi credentials, timezone settings, Gemini key), which goes
through a `*_service` component that owns its NVS namespace. Low urgency (a
small, self-contained two-function flag) but set a precedent worth
correcting before the next feature flag copied it.

Fixed: extracted a new minimal `app_state_service` component (just the two
functions, moved verbatim — same NVS namespace `"app_state"` and key
`"onboarded"`, so existing on-device state carries over unchanged) and
updated `app_shell.cpp`'s two call sites. `main/CMakeLists.txt` now depends
on it; `nvs.h` dropped from `app_shell.cpp`'s includes since nothing else
there used it. Verified on-device: boots straight to Home (not onboarding),
confirming the flag round-trips through the new component correctly.

## ~~Setup portal has no fetch timeout anywhere~~ — resolved

`webserver/src/portal/api.ts:20-46` (`fetchApiJson`, used by every API
helper including `wifi.ts`'s scan/connect/disconnect and
`providerKeys.ts`'s save/clear) had no `AbortController`/timeout on its
`fetch` call, confirmed via grep across `webserver/src/`. If a request to
the device's single HTTP server never resolved (e.g. mid Wi-Fi-scan), the
busy flag each caller sets before the `await` (`isScanning`/`isConnecting`/
`isCheckingStatus`/`geminiState.isBusy`) never cleared in its `finally`,
permanently disabling that button until the page was manually reloaded.

Fixed: `fetchApiJson` now races the fetch against a 10s `AbortController`
timeout (respecting a caller-supplied `signal` instead, though nothing
passes one today), surfacing a clear timeout error like any other
failure. Rebuilt and copied into `components/wifi_service/portal/`.
Verified: `tsc -b`/`vite build`/`eslint` all pass, and firmware builds
clean with the updated embedded portal. Not live-tested against the
device's AP portal in a browser.

## ~~Setup portal doesn't enforce the firmware's Wi-Fi credential length limit client-side~~ — resolved

`webserver/index.html:43-49` (`<ui-input id="password" variant="password">`,
no `maxlength`) and `webserver/src/portal/wifi.ts:275-284` (`connect()`)
only checked for a non-empty password — no upper bound — while
`wifi_service.cpp:1786` rejects `ssid.size() >= 65 || password.size() >= 65`
server-side. A too-long password got a full round trip to the device and
landed on a generic "Failed to start Wi-Fi connection" toast instead of an
immediate, specific client-side message. (No separate SSID input exists —
SSID comes from tapping a scanned network, and 802.11 SSIDs are hardware-
capped at 32 bytes anyway, well under the 64-char limit, so only the
password field needed this.)

Fixed: added a `WIFI_CREDENTIAL_MAX_LENGTH = 64` constant
(`webserver/src/portal/constants.ts`, matching the firmware's `>= 65`
rejection), wired `maxlength` support into the `ui-input` custom element
(`Input.ts`'s `observedAttributes`/`syncAttributes`), added
`maxlength="64"` to the password input, and added a matching client-side
length check in `connect()` alongside the existing empty-password check,
using the same `setFieldError`/focus pattern. Rebuilt and copied into
`components/wifi_service/portal/`. Verified: `tsc -b`/`vite build`/`eslint`
all pass, firmware builds clean, and the built `maxlength="64"` attribute
is confirmed present on the compiled portal's password input. Not
live-tested in a browser against the device's AP.

## Page trio duplication: Notes/Todos/Follow-up

`main/notes_page_{coordinator,runtime,interactions}.cpp`,
`todos_page_*.cpp`, and `follow_up_page_*.cpp` (9 files, ~1,900 lines) are
~90% copy-pasted — the entire class body differs only by class name, one
tag-filter predicate, an icon/label string, and one accessory field
(`follow_up`/`completed`). A fix to group-focus clamping or item-list
enter/exit logic has to be manually repeated 3x; already showing drift
(`follow_up_page_interactions.cpp`'s `HandlePrimaryActivate` diverges
subtly from notes' copy).

Fix: extract a generic templated/policy-based `TwoLevelTimelineCoordinator`
that each page configures, rather than three parallel implementations.
Biggest, riskiest item on this list — worth planning carefully rather than
doing opportunistically.

## `timeline_format` helpers reimplemented independently twice

`main/details_page_coordinator.cpp:19-88` and
`main/vibe_check_page_coordinator.cpp:22-89` each locally redefine
byte-for-byte-or-close copies of `timeline_format`'s `FormatDateLabel`/
`FormatTimeLabel`/`FormatDurationLabel`/`TrimTranscript`/`TagText`
(`main/timeline_format.h/.cpp`, already used correctly by
notes/todos/follow_up). They've already drifted: details' date formatter
is missing the "Today" comparison the shared one has, and vibe_check's
duration formatter uses a different `<=60` vs `<60` second boundary with
different padding.

Fix: replace both local copies with calls to `timeline_format`'s existing
helpers.

## Small duplicated helpers (3 more instances)

- `ForEachOutlineOffset` duplicated verbatim in
  `components/epaper_ui/checkbox.cpp:13-23`, `list_item.cpp:15-25`, and
  `list_item_header.cpp:49-60` — belongs in `render_utils.h`, which all
  three already include.
- `ApplyPrimaryActivateResult`'s dispatch switch
  (`if (callbacks.show_home) callbacks.show_home(); return;` per intent) is
  duplicated near-verbatim across 11 `*_page_interactions.cpp` files.
  `shared_page_interactions.h` already generalizes the lookup half of this
  pattern but not the dispatch half.
- Scroll-position clamp-by-step logic
  (`main/details_page_coordinator.cpp:166-171`,
  `main/summarize_page_coordinator.cpp:45-52`, and
  `main/overlay_runtime.cpp:826`) is independently reimplemented 3 times,
  with `overlay_runtime.cpp` even using a differently-named constant for
  the same 10% step.

Fix: three small, independent extractions — could be one branch or three,
lowest risk of the reuse findings.

## Redundant derived state: icon/checked fields duplicate their source bool

`TimelineEntry` (in `notes_page_coordinator.h:16-21` and mirrored in
todos/follow_up) stores both a source-of-truth bool (`follow_up`/
`completed`) and a separately-computed rendered projection
(`tag_icon_asset`/`accessory.checked`), which has to be kept in sync by
hand at 2 call sites per page (`notes_page_coordinator.cpp:81,303`,
matching lines in todos/follow_up). A future third place that flips
`follow_up`/`completed` (e.g. a bulk "mark all done" action) could easily
forget to update the mirror field.

Fix: derive the icon/checked state in `BuildState()` at render time instead
of storing it. Natural to fold into the page-trio refactor above rather
than doing separately.

## SD-card mount blocks the mandatory first paint

`components/storage_service/storage_service.cpp:578-611` (`Init()`) mounts
the SDMMC card and FAT filesystem synchronously, even though the same
function spins up a worker task for everything else. `app_shell.cpp:1810-1847`
calls this near the very start of `Run()`, before the first screen paint —
so every boot pays full SD detect+mount latency (and any retry) before the
panel's first pixel. `recording_archive_service::Init()` already
demonstrates the right pattern (cached NVS snapshot + deferred
`RefreshAsync()` scan) that could apply to the mount itself now that a
worker task exists.

## IMU auto-sleep motion detection polls instead of using the hardware interrupt path

`main/device_sleep_runtime.cpp:608-637` (`MotionPollingTask`) wakes every
200ms to do a full I2C read + float-math classification purely to detect
stillness for auto-sleep — but `components/qmi8658/qmi8658.cc` already
implements complete hardware any-motion/no-motion detection via INT2
(`ConfigMotion`, `EnableWakeOnMotion`, callbacks at lines ~877-1470), the
same mechanism already wired for the light-sleep wake gesture.
`components/imu_service/imu_service.cpp` never surfaces any of it — only
`ReadSample`. Continuous I2C polling on a battery-powered device where an
event-driven equivalent already exists elsewhere in the same driver.

## E-paper SPI path uses a needlessly small bounce buffer

`components/epaper_panel/epaper_panel.cpp:16` chunks every SPI write into
1KB pieces (`kSpiDmaChunkSizeBytes`) despite the bus being configured for
up to 48KB transfers (`kSpiBusMaxTransferSizeBytes`, line 17). A
whole-screen partial refresh always writes both ~48000-byte planes in
full, producing ~94 sequential blocking SPI round-trips and ~96KB of extra
bounce-buffer copying per partial refresh. A larger DMA-capable staging
buffer (or pipelined transfers) would cut both without touching the
necessary full-plane rewrite itself.

## Glyph rendering goes through `std::function` indirection on the hottest path

`components/epaper_ui/font_renderer.cpp:110-165` (`DrawText`) calls
`draw_pixel(...)` through a type-erased `std::function<void(int,int,uint8_t)>`
(`include/epaper_ui/font_renderer.h:12`) inside a 4-level nested loop —
every glyph of every string on every screen redraw. Making `DrawPixelFn` a
template parameter (or a small concrete struct) would let the compiler
inline the actual pixel write.

## Vestigial touch contract still alive in footer/carousel hit-testing

`main/app_shell.cpp:106-112` still switches on
`FeedbackCue::kTouchContact`, and `components/epaper_ui/global_footer.cpp:17-18,192-202`
/ `include/epaper_ui/carousel.h:35-39` still compute touch hit-slop
inflation on every hit-test, on a board with no touch controller where
nothing ever produces a touch event. Matches CLAUDE.md's own note that some
widget code still carries vestigial touch plumbing — this is the
mechanism-level version of that, kept alive in 5 files rather than dropped.

# TODO Archive

Resolved and closed items moved out of `docs/todo.md` to keep the active
list scannable. Full history — investigation notes, what was fixed, and
on-device verification — is preserved here in original order. See
`docs/todo.md` for what's still open.

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

## ~~Footer icons should be context-dependent, not a fixed always-visible set~~ — resolved

Originally framed as just reordering the bottom footer icons, but Craig's
actual complaint is visibility, not order: it's a pain getting back to Home
from within another screen (too many other icons competing for attention),
and there's no need to see Settings/Wifi/Time/Sticky while already inside
Todos/Notes/etc.

`FooterLayoutForScreen` (`main/app_shell.cpp`) already takes the current
`ScreenId` as a parameter but currently ignores it — every screen gets the
same fixed set (`show_settings`/`show_wifi`/`show_time`/`show_home`/
`show_sticky`/`show_mic` all unconditionally `true`). New design:

- **On the Home screen**: show everything except Home itself (Settings,
  Wifi, Time, Sticky, Mic) — no point showing a "go Home" icon when you're
  already there.
- **On every other screen**: show only Home and Mic. Settings/Wifi/Time/
  Sticky hide — Home becomes the single, obvious way back, and Mic (the
  core press-and-hold-to-record action) stays reachable from anywhere, but
  the sticky-note overlay and quick-nav shortcuts to Settings/Wifi/Time
  don't need to be reachable mid-task from every other screen.

Fixed: `FooterLayoutForScreen` (`main/app_shell.cpp`) now switches on
`screen == ScreenId::kHome` and sets the two visibility sets above instead
of the previous unconditional `true`s.

Investigating the roving-focus side confirmed the suspected gap: hiding an
icon only in `footer_runtime::LayoutState` wouldn't have been enough on its
own — every one of the 10 pages with footer items in their `NavigationModel`
(`components/page_navigation/navigation_model.cpp`) unconditionally added
all 5 footer roles (`kFooterSettings/kFooterWifi/kFooterTime/kFooterSticky/
kFooterHome`), so UP/DOWN roving focus would still have landed on and
activated an icon the user couldn't see. `NavigationModel`/`RovingFocus`
have no visibility concept at all — they only know about item presence.
Fixed the same way, at the source: extracted the previously-duplicated
5-line footer block (identical across all 10 `Build*PageNavigationModel()`
functions) into a single new `AddFooterItems(model, is_home_screen)` helper
that adds Settings/Wifi/Time/Sticky when `is_home_screen` and only Home
otherwise — mirroring `FooterLayoutForScreen`'s own rule as a single source
of truth. `BuildDashboardPageNavigationModel()` (the Home screen) now omits
`kFooterHome` from its model entirely, and the other 9 builders omit the
other four roles, so an invisible icon is never reachable by roving focus
either. No changes needed anywhere else: `IndexOfRole`/
`FooterSelectedIndexForFocus`/`FocusFooterItem`/`HandleFooterPrimaryActivate`
already tolerate an absent role gracefully (confirmed via investigation
before implementing, not just assumed), and `global_footer.cpp`'s rendering
already filtered on `visible` correctly. Mic needed no `NavigationModel`
change since it was never part of roving focus/`NavigationItemRole` to
begin with. Onboarding is unaffected (it never calls `FooterLayoutForScreen`
or includes footer items — its own Close/Prev/Next control row replaces the
footer entirely).

Verified: clean build with zero warnings, flashed to hardware, Craig
confirmed on-device it looks and behaves as intended.

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
side. Each item below was its own branch/PR.

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

## ~~`timeline_format` helpers reimplemented independently twice~~ — resolved

`main/details_page_coordinator.cpp:19-88` and
`main/vibe_check_page_coordinator.cpp:22-89` each locally redefined
byte-for-byte-or-close copies of `timeline_format`'s `FormatDateLabel`/
`FormatTimeLabel`/`FormatDurationLabel`/`TrimTranscript`/`TagText`
(`main/timeline_format.h/.cpp`, already used correctly by
notes/todos/follow_up). They'd already drifted: details' date formatter
was missing the "Today" comparison the shared one has, and vibe_check's
duration formatter used a different `<=60` vs `<60` second boundary with
different padding.

Fixed: both local copies removed, replaced with calls to `timeline_format`'s
existing helpers. Two deliberate, intended behavior changes fall out of
this (the drift the finding itself flagged, not preserved):
- Details page: a recording made today now shows "Today" as its title
  instead of the literal weekday/date, matching notes/todos/follow-up. Its
  own "no date at all -> 'Details'" fallback (different from
  `timeline_format`'s own "Today" fallback for that case) is preserved via
  an explicit empty check before calling the shared helper.
- Vibe Check page: durations now show unpadded seconds ("5s" not "05s")
  and use the same `<60` minute boundary as everywhere else, instead of
  its own `<=60` with zero-padding.

Verified on-device: confirmed both changes render correctly (Details page
"Today" label, Vibe Check unpadded duration).

## ~~Small duplicated helpers (3 more instances)~~ — two resolved, one skipped

- ~~`ForEachOutlineOffset` duplicated verbatim in
  `components/epaper_ui/checkbox.cpp:13-23`, `list_item.cpp:15-25`, and
  `list_item_header.cpp:49-60`~~ — resolved. Moved into `render_utils.h`
  (all three already included it) as a header-only template; the three
  local copies removed.
- **Skipped, not the simple fix it looked like**: `ApplyPrimaryActivateResult`'s
  dispatch switch across 11 `*_page_interactions.cpp` files. On closer
  inspection each page's `ActivateIntent`/`ActivateCallbacks` are genuinely
  different types with different shapes — different callback counts,
  `onboarding_page_interactions` has a completely unrelated single-callback
  set, and `wifi_page_interactions` has a callback that takes parameters
  (`toggle_selected_network_connection(bool, ssid, password)`). A real
  shared dispatcher would mean restructuring every page's callback contract
  into a uniform shape first — a bigger, riskier redesign than this "quick
  win" item implied, closer in scope to the page-trio refactor tracked in
  `docs/todo.md`. Left as-is; `shared_page_interactions.h`'s existing
  lookup-half generalization (`HandleFooterPrimaryActivate`) still stands
  on its own.
- ~~Scroll-position clamp-by-step logic
  (`main/details_page_coordinator.cpp`, `main/summarize_page_coordinator.cpp`,
  and `main/overlay_runtime.cpp`)~~ — resolved. Added
  `shared_page_interactions::StepScrollPercent(current, delta, step_percent)`
  (clamps into `[0,100]`, reports whether it actually changed) and switched
  all three call sites to it, including `overlay_runtime.cpp`'s
  differently-named `kStickyScrollStepPercent` constant, which now just
  passes its own step size into the shared function.

Verified on-device: transcript scrolling (Details page) still scrolls
smoothly and stops correctly at 0%/100%.

## ~~SD-card mount blocks the mandatory first paint~~ — resolved

`components/storage_service/storage_service.cpp:578-611` (`Init()`) mounts
the SDMMC card and FAT filesystem synchronously, even though the same
function spins up a worker task for everything else. `app_shell.cpp:1810-1847`
calls this near the very start of `Run()`, before the first screen paint —
so every boot pays full SD detect+mount latency (and any retry) before the
panel's first pixel. `recording_archive_service::Init()` already
demonstrates the right pattern (cached NVS snapshot + deferred
`RefreshAsync()` scan) that could apply to the mount itself now that a
worker task exists.

Investigated making `storage_service::Init()` itself async on that pattern,
but found a much lower-risk fix: `Run()`'s comment claiming "the SD card
needs to enter and stay in SPI mode before the shared-bus display path is
brought up" turned out to be stale, copy-pasted from the Sticky-board port
(`git show 8d39d55`, which introduced a `shared_bus_service` component to
arbitrate a *shared* SPI2 bus between SD and display on that board). On
Waveshare there is no shared bus — the SD card is on SDMMC (4-bit,
`components/sd_card/sd_card.cpp` uses `SDMMC_HOST_DEFAULT()`, confirmed
against `docs/waveshare-epaper-hardware-spec.md:231`) and the panel owns
SPI3 outright; `shared_bus_service` doesn't exist in this codebase at all
anymore. `display_service::Init()` already does its full startup-splash
refresh synchronously before returning, so simply reordering `Run()` to call
`InitDisplayService()` before `InitStorageService()` moves the first pixel
ahead of the SD mount without touching `storage_service`'s internals or its
synchronous-mount contract. Fixed by swapping the two calls and replacing the
stale comment. Verified on-device across 5 full reboots (1 fresh flash + 4
EN-pin resets): `DisplayService: Display initialized with startup splash`
now logs consistently before `StorageService: Mount /sdcard: ESP_OK` on every
boot, SD mount/listing and the home-screen paint both still succeed
identically each time, no errors or warnings in any capture.

## ~~E-paper SPI path uses a needlessly small bounce buffer~~ — resolved

`components/epaper_panel/epaper_panel.cpp:16` chunks every SPI write into
1KB pieces (`kSpiDmaChunkSizeBytes`) despite the bus being configured for
up to 48KB transfers (`kSpiBusMaxTransferSizeBytes`, line 17). A
whole-screen partial refresh always writes both ~48000-byte planes in
full, producing ~94 sequential blocking SPI round-trips and ~96KB of extra
bounce-buffer copying per partial refresh. A larger DMA-capable staging
buffer (or pipelined transfers) would cut both without touching the
necessary full-plane rewrite itself.

Investigated going all the way to the full 48KB the bus supports, but this
board has a documented history of internal-DRAM crash-loops (commit
`8848341`): internal RAM was so scarce that ESP-IDF's own lazy TLS
hardware-crypto lock allocation failed outright, breaking Gemini
authentication, until the fix moved ~96KB of framebuffers off internal RAM
onto PSRAM specifically to reclaim headroom there. This bounce buffer is
one of the few things on this board that still has to be internal-RAM +
DMA-capable (`MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA`), so a 48KB bump would
have clawed back most of what that fix deliberately freed. Went with a
conservative 4KB instead (`kSpiDmaChunkSizeBytes`, 1024 -> 4096): cuts a
full plane write from ~47 blocking round-trips down to ~12.

Added a log line reporting free internal RAM right after this allocation,
for future visibility into this exact tradeoff. Verified on-device across 3
reboots: internal RAM free measured a consistent 195463 bytes after the
new 4KB buffer (nowhere near the scarcity that caused the earlier
crash-loop), Wi-Fi connected and Gemini authenticated successfully every
time (the two things that broke during that incident), and
`DisplayService: refresh metrics`'s `spi=` figure dropped from ~40.5-40.6ms
to ~39.3-39.5ms per refresh -- a modest, real ~1.2ms win. The SPI bus
itself turned out to be bandwidth-bound at 20MHz (48000 bytes x 2 planes
≈ 38.4ms theoretical minimum, matching the ~39-40ms baseline almost
exactly), so the chunking overhead this item complained about was only
ever that ~1-1.5ms of extra per-transaction software cost, not a
proportionally large fraction of refresh time. Craig confirmed the screen
still looks correct (no banding/corruption) after the change.

## ~~Glyph rendering goes through `std::function` indirection on the hottest path~~ — resolved

`components/epaper_ui/font_renderer.cpp:110-165` (`DrawText`) calls
`draw_pixel(...)` through a type-erased `std::function<void(int,int,uint8_t)>`
(`include/epaper_ui/font_renderer.h:12`) inside a 4-level nested loop —
every glyph of every string on every screen redraw. Making `DrawPixelFn` a
template parameter (or a small concrete struct) would let the compiler
inline the actual pixel write.

Made `DrawText` a function template on the callable type instead of taking
`std::function`. This required moving its definition (and the private
`FontSelection`/`ScaleMetric`/`FontForRole` helpers it depends on) from
`font_renderer.cpp` into the header as `inline` functions under a new
`font_renderer_detail` namespace, since a template's definition has to be
visible at every call site's translation unit. `bitmap_font.h`/
`generated_epaper_fonts.h` (now included by the header) are both just
extern struct declarations, not the actual bitmap data, so this doesn't
duplicate font data across translation units or meaningfully affect
compile times. All 8 existing call sites (`render_utils.cpp`,
`toast.cpp`, `lock_screen.cpp` x5, `status_bar.cpp`) already passed
inline lambdas directly rather than a stored `DrawPixelFn`, so template
argument deduction just works with no call-site changes needed. The
now-unused `DrawPixelFn` type alias was removed (had no other references
anywhere).

Verified: clean build, zero warnings, binary size grew by only 336 bytes
across the 8 template instantiations (no runaway template bloat — the
lambdas are all structurally similar). Flashed to hardware: clean boot,
home screen paints correctly, Craig confirmed all on-screen text (status
bar, footer labels, welcome message) still renders correctly with no
garbling or spacing regressions.

## ~~Vestigial touch contract still alive in footer/carousel hit-testing~~ — resolved

`main/app_shell.cpp:106-112` still switches on
`FeedbackCue::kTouchContact`, and `components/epaper_ui/global_footer.cpp:17-18,192-202`
/ `include/epaper_ui/carousel.h:35-39` still compute touch hit-slop
inflation on every hit-test, on a board with no touch controller where
nothing ever produces a touch event. Matches CLAUDE.md's own note that some
widget code still carries vestigial touch plumbing — this is the
mechanism-level version of that, kept alive in 5 files rather than dropped.

Investigating turned up more dead code than the item assumed: the hit-test
functions themselves (not just their slop math) had zero callers anywhere —
`HitTestGlobalFooterItem`, `HitTestCarouselClose/Prev/Next`,
`HitTestOnboarding`, and the "for touch diagnostics" `CarouselControlBounds`/
`OnboardingControlBounds` pair were all unreachable; real button-driven
navigation goes through a completely separate path
(`OnboardingPageCoordinator::FocusedControl()` etc., which was left alone).
Removed the whole unreachable chain: `FeedbackCue`/`FeedbackEvent`'s
`kTouchContact` enumerators and their switch cases (`app_shell.cpp`,
`feedback_service.{h,cpp}`); `kTouchHitSlopX/Y`, `ExpandTouchBounds`, and
`HitTestGlobalFooterItem` (`global_footer.{h,cpp}`); `touch_slop_x/y`,
`InflateForTouch`, `HitTestCarouselClose/Prev/Next`, `CarouselControlBounds`,
and the `CarouselControlRects` struct (`carousel.{h,cpp}`); `HitTestOnboarding`
and `OnboardingControlBounds` (`onboarding_page.{h,cpp}`); and the now-orphaned
`kTouchSlopX/Y` design tokens. `OnboardingControl` (the enum) stays — it's
genuinely load-bearing for button focus, just no longer touch-reachable.
Verified: clean build with zero warnings, clean on-device boot with the same
healthy display-before-storage ordering as the previous fix.

## ~~Lock screen should trigger display/light sleep on entry~~ — resolved

Currently, locking the device (`lock_screen_runtime::Toggle()`, wired to
the POWER_OK short-press gesture in `app_shell.cpp:1398-1399`) only swaps
the active screen to `kLockScreen` — it's entirely independent of
`device_sleep_service`'s auto-sleep stage machine, which keeps running its
own inactivity timers regardless of whether the lock screen is showing.
So locking the device doesn't itself put it to sleep; it just shows a
different full-screen view that then sits awake until the normal
display-sleep/light-sleep timeouts elapse on their own.

Want locking to force entry into sleep (display sleep at minimum, likely
light sleep as well) immediately rather than waiting out the normal
inactivity timers. Needs a decision on exactly which stage(s) to force and
whether `lock_screen_runtime::Show()` should call into
`device_sleep_service` directly or through `device_sleep_runtime`.

Fixed: added `device_sleep_service::ForceDisplaySleep()`, which — only from
`Stage::kAwake` and only while auto-sleep and the display-sleep stage are
both enabled — transitions straight to `kDisplaySleeping` by dispatching
`Action::kEnterDisplaySleep` the same way the normal inactivity timer
would, tagged with a new `TransitionReason::kLockScreen`. Light sleep is
deliberately *not* forced along with it (that would mean paying its
Wi-Fi-teardown/SD-remount cost on every lock/unlock, not just a lock that's
actually left alone) — instead `ForceDisplaySleep()` arms the inactivity
clock itself (`s_inactivity_armed`/`s_inactivity_started_us`, from the lock
moment), so the existing monitor tick still carries it into `kLightSleeping`
on its own after the usual configured `light_sleep_timeout_seconds`, without
needing a fresh stillness detection first. The dispatch goes through the
existing event queue/`device_sleep_runtime` handler, so the real hardware
sequence (sleep-indicator repaint, panel sleep) is identical to a normal
timeout-driven entry; forcing it just skips the wait. Went with
`lock_screen_runtime::Show()` calling `device_sleep_service` directly
(non-blocking — the dispatch only enqueues onto `device_sleep_runtime`'s
auto-sleep task) rather than routing through `device_sleep_runtime`, matching
how `power_key_runtime.cpp`/`app_shell.cpp` already query
`device_sleep_service::GetSnapshot()` directly. Verified on-device: Craig
confirmed pressing `PWR` to lock puts the display to sleep immediately.

## ~~Waking from a locked+asleep device should only be triggered by a button press~~ — resolved

Related to the item above: once locking forces sleep, motion alone
shouldn't wake the display back up while locked — pocket/bag handling of a
locked device would otherwise keep flashing the screen awake, which is
both a battery drain and (since the plan above would make locking +
sleeping routine) a minor privacy concern. `device_sleep_service.cpp`
already distinguishes motion-sourced wake from interaction-sourced wake
(`NotifyUserActivity`'s `ActivitySource::kMotion` is explicitly excluded
from waking `Stage::kLightSleeping`, and only wakes `Stage::kDisplaySleeping`
when `motion_wake_enabled` is set) — this item wants that same exclusion
to also apply to `Stage::kDisplaySleeping` specifically while the lock
screen is the active/restore screen, so only a real button press (an
interaction source) can wake the display when locked, regardless of stage.

Fixed: `device_sleep_service` stays board/product-agnostic (it has no
notion of the lock screen), so the exclusion lives in
`main/device_sleep_runtime.cpp`'s `ClassifyMotionSample` instead — before
forwarding a detected-motion transition to
`device_sleep_service::NotifyMotionDetected()`, it now checks
`lock_screen_runtime::IsActive()` and drops the notification (just logging
it) when locked, regardless of which sleep stage the device is currently
in. This closes two gaps the force-display-sleep fix above would otherwise
leave open: without it, IMU noise from pocket/bag motion would (a), with
the default `motion_wake_enabled`, keep waking `Stage::kDisplaySleeping`
straight back to `kAwake`, and (b) — since *any* call into
`NotifyUserActivity()` unconditionally resets the inactivity clock, whether
or not it causes a wake — keep restarting the light-sleep countdown that
fix armed, so it could never elapse while the device was being carried.
Real button presses are unaffected — they still wake the device via the
existing `ConsumeAsWake()` (`power_key_runtime.cpp`) / wake-gesture paths.
(As first shipped, that wake press then required a second, separate press
to actually unlock; see the follow-up below — fixed the same day after
on-device testing showed the lock screen was left showing after the wake
press.)

## ~~Waking a locked device required a second press to actually unlock~~ — resolved

Found on-device the same day as the two items above: `ConsumeAsWake()`
correctly consumed the waking `PWR` press so it wouldn't also toggle the
lock, but that meant the press that woke the device from `display_sleeping`
just redrew the *lock screen* (via the normal `Action::kWakeDisplay` ->
`display_service::WakeDisplay()` path) — actually unlocking still needed a
second, separate press to run `HandlePowerKeyPress()`'s `lock_screen_
runtime::Toggle()`. Craig wanted the wake press itself to land straight on
Home.

Fixed with three pieces: (1) a new `device_sleep_runtime::
RequestUnlockOnWake()` one-shot flag, set by `ConsumeAsWake()` (`main/
power_key_runtime.cpp`) immediately before the `NotifyUserActivity()` that
wakes the device, but only when the lock screen is active — scoping the
behavior to the deliberate lock/unlock key so a different wake source (the
`ACTION` button is also a light-sleep wake source, but has no lock-toggle
meaning) still just wakes to the lock screen; (2) `WakeDisplayRespectingLock()`/
`RecoverDisplayRespectingLock()` (`main/device_sleep_runtime.cpp`) consume
that flag in the `kWakeDisplay` action and in `RestoreAfterLightSleep()`
respectively, calling the new `lock_screen_runtime::HideWaking()` instead
of the plain wake call when it's set; (3) a new `display_service::
WakeDisplayToScreen(ScreenId)`, since setting the current screen (the way
`Hide()`'s `SetCurrentScreen()` does) and then separately waking the panel
would race — the async display command queue and the direct,
mutex-guarded wake path have no ordering guarantee between them, and a
losing race draws the lock screen first and the intended screen a moment
later as a second, redundant full refresh. `WakeDisplayToScreen` sets the
screen and does the one wake-refresh atomically under the same lock, so
`HideWaking()` (`main/lock_screen_runtime.cpp`, sharing its body with
`Hide()` via a new internal `HideImpl(bool waking)`) produces exactly one
full refresh, directly to the restore screen.

For the light-sleep-wake case specifically, the fix is best-effort rather
than guaranteed: the AXP2101 interrupt-decoding task that calls
`ConsumeAsWake()` runs at a lower FreeRTOS priority
(`kInterruptTaskPriority` = 2 in `components/axp2101/axp2101.cc`) than the
auto-sleep task running the light-sleep wake/restore sequence
(`kPriorityAppSleep` = 4), so in practice the restore sequence — and
therefore `RecoverDisplayRespectingLock()`'s check — normally runs to
completion before `ConsumeAsWake()` gets scheduled to set the flag. The end
result is still correct in that case (the pre-existing `ConsumeAsWake()` /
`HandlePowerKeyPress()` path notices the device is already awake by the
time it runs and falls through to the normal lock-toggle handler instead),
just via a redraw-then-redraw rather than the single clean refresh the
display-sleep case gets. Verified on-device for the display-sleep case:
Craig confirmed the `PWR` wake press now lands on Home directly. The
light-sleep path is reasoned from the code, not separately observed.

## ~~Replace the lock screen's full-screen clock with a todo summary~~ — resolved

`epaper_ui::LockScreenState` (`include/epaper_ui/lock_screen.h`) and
`DrawLockScreen` currently only carry/render `hour_text`/`minute_text`/
`weekday_text`/`date_text` — a big digital clock, built and refreshed once
a minute by `lock_screen_runtime.cpp`'s `RebuildClockStateLocked`/
`OnClockTimer`. Craig's observation: once the item above makes locking
routine, the clock stops being useful ~30 seconds in anyway once the
display sleeps and freezes on whatever was last drawn — an always-on
summary of pending todos would be more useful to see at a glance than a
clock that's usually stale.

Todos data already exists via `recording_archive_service::RecordingEntry`
(tagged `RecordingTag`, consumed by `TodosPageCoordinator` for the Todos
page) — this would need a lock-screen-specific summary view sourced from
the same data, plus deciding how much of the clock (if any) to keep
alongside it. Own branch/PR; needs a design pass on what the summary
actually shows (counts? titles? both?) before implementing.

Design settled through a wireframe discussion before any code was written
(see chat history, not reproduced here): drop the clock entirely (`hour_text`/
`minute_text` removed from `LockScreenState`, since it's the part that
actually goes stale) and keep only weekday+date on one line. Below a thin
divider, a "TO-DO - N PENDING" heading (or "TO-DO" + "All caught up" when
N=0), then up to the first 3 pending todos as checkbox rows -- **follow-up
flagged todos surface first** (`RecordingMetadata::follow_up`, an existing
cross-tag flag independent of `tag`/`completed`, already toggleable from the
Todos page's own item-actions menu), each newest-first within its group.
Each row shows the *full* transcript text (untruncated, since 10s recordings
keep them short) at the default `kBody` role, word-wrapped, capped to 3
lines with an ellipsis on overflow -- not the single-line-truncated "title"
the Todos page itself uses. If there are more than 3 pending, one line below
them reads "+N more pending" (title-less; the design deliberately doesn't
fall back to a partial list once past the cap). A `kLock` icon (an asset
that already existed, previously used only for private-Wi-Fi-network rows)
renders at 2x scale, centered near the bottom of the screen, as a persistent
"this is locked" anchor now that nothing else on the screen visually says so.

Fixed: `LockScreenState` gained `pending_todo_titles`
(`std::vector<std::string>`, the top ≤3 already selected/prioritized) and
`pending_todo_count` (the true total). `DrawLockScreen`
(`components/epaper_ui/lock_screen.cpp`) was rewritten for the new layout,
and in the process migrated off its own long-stale local duplicates of
`DrawPortraitPixel`/`ShouldDrawBlackForTone`/`DrawPortraitMonoAsset` onto
the shared `render_utils.h` versions -- worth flagging on its own: the local
`ShouldDrawBlackForTone` still had the *old*, buggy gray-dither lattice
(`(x%2==0)&&(y%2==0)`) that caused the WiFi-page banding fixed elsewhere
(see the "SSD1677 partial refresh" note in `docs/auto-sleep.md`'s history) --
`render_utils.h`'s version already carries that fix
(`(x+y)&3)==0`), so this migration was a genuine bug fix for any gray-toned
content on this screen, not just a dedup.

The actual pending-todo data comes from `recording_archive_service::
ListRecordings()`, which does blocking SD I/O -- new `lock_screen_runtime::
RefreshTodoSummary()` does that scan, filters to `tag==kTask && !completed`,
partitions/sorts as above, and caches the result in `s_state`, deliberately
**not** called from `Show()` (which runs on the PMIC/power-key interrupt
task -- blocking that task on an SD scan would be a real responsiveness
risk for a task that also has to notice a shutdown long-press). Instead
`main/app_shell.cpp`'s `HandleRecordingArchiveEvent()` calls it
unconditionally on every archive-changed event (mirroring how
`dashboard_page_runtime::SyncFromService` already runs unconditionally
there), plus once at boot from `InitRecordingArchiveService()` to seed it —
so by the time a user actually locks the device, the summary is already
current rather than being computed at lock time.

Also promoted `list_item_header.cpp`'s private `FitLabelText` (truncate-
with-ellipsis) helper to the shared `render_utils.h`, since the new lock
screen needed the exact same operation. Doing so surfaced that
`FitLabelText` was independently reimplemented in **five** places
(`list_item.cpp`, `list_item_header.cpp`, `network_item.cpp`,
`timeline_list.cpp`, `select_item.cpp`), two with a different parameter
order than the other three. Removed the two whose signature was an exact
match (`list_item.cpp`, `timeline_list.cpp`) once they collided with the
newly-shared version (the build caught this immediately as an ambiguous
overload); left `network_item.cpp`/`select_item.cpp` alone since fixing
their differing parameter order was out of scope for this change. (The
remaining two-place duplication is tracked separately in `docs/todo.md`.)

On-device testing caught a real bug the build couldn't: recording an item
and tagging it as a todo crashed the device (before the transcript dialogue
appeared) with `***ERROR*** A stack overflow in task input_callbacks has
been detected`. `main/input_callback_dispatcher.cpp`'s `input_callbacks`
task has a 4096-word stack sized for lightweight callback dispatch — the
recording-save flow already runs its archive-write + `NotifyHandler()` ->
`HandleRecordingArchiveEvent()` chain on it, and `RefreshTodoSummary()`'s
synchronous `ListRecordings()` scan plus this file's own filtering/sorting
on top of that was enough to overflow it. `HandleRecordingArchiveEvent()`
can run on genuinely different caller tasks depending on what triggered
it (a dedicated `arc_refresh` worker sometimes, `input_callbacks` other
times), so tuning that one task's stack size would have been fragile.

Fixed by making `RefreshTodoSummary()` fire-and-forget: it now kicks off a
dedicated one-shot task (`lockscr_todos`, 6144 words, `kPriorityStorage` on
`kSystemCore` — mirroring `recording_archive_service::RefreshAsync()`'s own
pattern) that does the actual scan/filter/sort/cache-update/push, guarded
by an atomic in-flight flag so a call that lands while one is already
running is a harmless no-op rather than a second concurrent scan. The
public function's contract changed accordingly (now non-blocking, safe
from any caller stack) — see the updated doc comment in
`lock_screen_runtime.h`. Side benefit: the boot-time seed call in
`InitRecordingArchiveService()` no longer adds a blocking SD scan to the
pre-first-paint startup path either.

Verified: clean build, zero warnings, flashed, and Craig confirmed the
same record-then-tag-as-todo steps that crashed before no longer do. The
recording saved during the original crash (SD write completes before the
crash point, so it survived) was left without a transcript, since the
crash landed after the save but before transcription kicked off, and
Gemini was already authenticated at the time so it isn't a
`pending_transcription` auto-retry case — needs a manual Transcribe from
its Details page, a one-off cleanup rather than a bug. The visual layout
itself (spacing/wrapping/icon placement) has not yet been separately
checked against the physical panel.

## ~~Completed todos never leave the Todos list or get archived~~ — resolved

Marking a todo complete (`main/todos_page_runtime.cpp:361-369` ->
`recording_archive_service::MarkRecordingCompleted`,
`components/recording_archive_service/recording_archive_service.cpp:1271-1285`)
only flipped a `completed` bool on the recording's metadata sidecar. Nothing
else changed: `TodosPageCoordinator::BuildGroups`
(`main/todos_page_coordinator.cpp:36-98`) listed every `kTask`-tagged
recording regardless of `completed`, so the Todos count/list included
completed items forever, shown checked. There was no expiry/retention/archive
concept anywhere in the codebase (confirmed via grep) — the only removal path
was the unrelated, explicit Delete action, which moves the sidecar files to
`/trash/todos`.

Design settled through a planning conversation before any code was written
(see chat history): scope limited to Tasks (not Notes/Follow-up); the age
check runs lazily inside `recording_archive_service`'s existing scan pass
rather than a new periodic sweep task (none exists anywhere in this codebase);
archiving is both automatic (age-based) and manual ("Archive now"); and rather
than a 4th near-identical page trio (see the page-trio duplication item in
`docs/todo.md`), the Archived view is a mode within the existing Todos
screen/trio. The Active/Archived switch itself went through two iterations:
the first build wired it to the footer's previously wired-but-never-shown
Folder icon; after trying that on-device, Craig asked for a segment control
at the top of the screen instead, matching the Notes/Todos switch already on
the Summarize page, which is what actually shipped (see "Implemented" below).

Implemented:
- `RecordingMetadata` gained `completed_unix_seconds` (stamped the moment
  `completed` first flips true, in `MutateMetadataOnMountedFilesystem` --
  age is measured from actual completion time, not `created_unix_seconds`),
  `archived`, and `archived_unix_seconds`. A legacy completed todo with no
  `completed_unix_seconds` gets it stamped to "now" the first time the sweep
  sees it, rather than being treated as infinitely old and archiving
  immediately on upgrade.
- New `ApplyTodoArchiveAgingOnMountedFilesystem` sweep runs at the start of
  every archive scan (`ScanArchiveOnMountedFilesystem`, already invoked by
  `RefreshAsync`/`Refresh` -- no new task/timer): for each completed,
  non-archived Task past the configured delay, it flips `archived`, stamps
  `archived_unix_seconds`, and deletes just the `.wav` in place via the new
  `DeleteRecordingAudioFile` (unlike Delete, nothing moves to `/trash` --
  `.json`/`.txt` stay under `/todos`, so `has_audio_file`, already computed
  live via `stat()`, naturally goes false and the Details page's Play button
  already hides on that condition).
- New `MarkRecordingArchived(id, archived)` mutator (same
  `MutateContext`/`RunWithMountedFilesystem` shape as the existing
  mutators) backs both the sweep and a new "Archive now" item action, shown
  only for a completed-but-not-yet-archived row. Un-completing an archived
  row (the existing "Mark incomplete" action) doubles as Restore -- it clears
  `archived` too, though the deleted audio does not come back.
- Archive-after-days setting: `CONFIG_FOLLOWUP_TODO_ARCHIVE_AFTER_DAYS`
  Kconfig default (7 days), NVS-overridable (new `rec_archive_cfg`/`days`
  namespace, deliberately separate from the existing `rec_archive`/`counts`
  cached-counts blob) via `recording_archive_service::GetArchiveAfterDays()`/
  `SetArchiveAfterDays()`, following `timezone_service`'s Kconfig-default +
  NVS-override pattern rather than the auto-sleep timeouts' compile-time-only
  one, since this needed to be user-adjustable on-device.
- `TodosPageCoordinator` gained a `TodosPageViewMode {kActive, kArchived}`;
  `BuildGroups` filters on `archived` matching the mode. The switch is a
  `SegmentControlState` ("Current"/"Archived") drawn between the title and
  the timeline -- same widget, spacing, and enter-move-exit interaction
  (OK enters it, UP/DOWN switches segments live, OK or hold-DOWN exits) as
  the Summarize page's existing Notes/Todos segment control, right down to
  reusing its `RovingFocus`-based `EnterSegmentControl`/`ExitSegmentControl`/
  `MoveFocus` pattern. It's always the first focusable item on the page
  (`NavigationItemRole::kTodosPageSegmentControl`, item index 0 in
  `BuildTodosPageNavigationModel`), so a background archive-changed refresh
  never silently moves focus off of it. The coordinator now caches the last
  `ListRecordings()` result (`recordings_`) so switching segments live-
  rebuilds the visible groups with no extra SD read.

  First iteration wired this toggle to the footer's previously
  wired-but-never-shown Folder icon (`GlobalFooterItemId::kFolder`) instead
  -- that wiring (`NavigationItemRole::kFooterFolder`, `show_folder` on
  `ScreenId::kTodos`, etc.) was fully reverted in favor of the segment
  control after on-device feedback. Worth keeping in mind for future work:
  the Folder icon's stale pending-transcription badge wiring, found and
  removed from `footer_runtime.cpp` while building the first iteration,
  stays removed -- it was already dead (`show_folder` is false everywhere
  again) and wrong for either purpose (see the "Offline transcription
  queue" item above for why pending-transcription counts live on the
  status bar's `kFile` icon instead).
- Settings screen gained an "Archive todos after" picker (7/14/30/60/90 days
  or Never), reusing the Time page's `SelectInput`+`SelectModal` timezone-
  picker pattern rather than free-form numeric entry (no numeric-input
  precedent existed on Settings).
- A pre-existing, unrelated bug surfaced while wiring the Restore path:
  `ResolveExistingBasePath` only matched a recording via its `.wav`, so any
  mutation (`MarkRecordingCompleted`, `MarkRecordingFollowUp`, etc.) on an
  audio-less archived entry would have failed to resolve at all. Fixed to
  match on either `.wav` or `.json`, mirroring how
  `DeleteRecordingOnMountedFilesystem` already anchors on any sidecar.
- `docs/app-architecture.md` updated: the new NVS namespace, the new Kconfig
  default (and its NVS-override relationship to `rec_archive_cfg`), and a
  note that archiving deletes the `.wav` in place rather than moving
  anything to `/trash`.

Verified on-device across both iterations: Craig confirmed the core
archiving functionality (completion, the automatic sweep path, "Archive
now," and the Settings picker) worked as intended on the first flash
("functionality seems to work so far"), then asked for the Folder-icon
toggle to be reworked into the segment control described above; the
reworked version was flashed and confirmed working too ("looks much
better").

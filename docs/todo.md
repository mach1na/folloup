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

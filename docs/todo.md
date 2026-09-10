# TODO

Open items only. Resolved/closed items (with full investigation and
verification history) have moved to `docs/todo-archive.md`.

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
of storing it. The page-trio refactor (see `docs/todo-archive.md`) left
`TimelineEntry` and its mutators page-specific, so this is still open and
would now touch each page's `BuildState()`/`SetEntryFollowUpState`/
`SetEntryChecked` individually rather than a single shared spot.

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

## Lock screen todo summary sometimes doesn't appear after PWR forces sleep

Craig noticed on-device: pressing `PWR` sometimes locks the device and
forces display sleep (per the archived "Lock screen should trigger
display/light sleep on entry" item), but the todo summary doesn't render on
the frozen lock screen — putting the display back to sleep and waking it
again usually fixes it.

Likely cause, from reading the code (not yet reproduced/confirmed on-device):
`lock_screen_runtime::Show()` (`main/lock_screen_runtime.cpp:278-323`) paints
synchronously from whatever `s_state.pending_todo_titles` currently holds,
then immediately calls `device_sleep_service::ForceDisplaySleep()` (line
321). Unlike the normal inactivity-driven sleep path, `ForceDisplaySleep()`
(`components/device_sleep_service/device_sleep_service.cpp:471-504`) has no
`BlockerReason::kDisplayRefresh` check, so it doesn't wait for a
still-in-flight repaint. If an archive-changed event's
`RefreshTodoSummary()` (`lock_screen_runtime.cpp:370-389`, worker at
141-200) is mid-scan when the lock happens, `Show()` paints stale/empty
data; by the time the worker finishes and pushes the corrected state with a
partial-refresh request (line 190-195), the panel has very likely already
gone to sleep, and `DisplayTask` (`components/display_service/display_service.cpp:1103-1192`)
silently drops any queued command while `s_display_sleeping` (logs
"suppressed while display sleeping" only) — so the correction never paints
until the next real wake, which does an unconditional full refresh. This
matches "sleep/wake fixes it" exactly.

Secondary contributing factor: `RefreshTodoSummary()`'s atomic in-flight
guard (`lock_screen_runtime.cpp:372,386-388`) drops a newer archive-change
trigger that lands while a scan is already running, so the running task
still pushes its already-stale snapshot rather than the latest one.

Fix likely needs `ForceDisplaySleep()` (or its caller) to wait for/avoid
racing an in-flight todo-summary refresh, and/or `Show()` to kick a
synchronous-enough refresh before painting rather than relying on
whatever's cached. Needs on-device reproduction to confirm before
implementing.

## `FitLabelText` still independently reimplemented in two places

`network_item.cpp` and `select_item.cpp` each still have their own private
copy of `FitLabelText` (truncate-with-ellipsis), with the parameters in a
different order (`text, role, max_width`) than the shared version already
promoted to `render_utils.h` (`role, text, max_width`) — which is exactly
why they didn't collide with it and get caught by the build the way two
other duplicates already were (see the archived "Replace the lock screen's
full-screen clock with a todo summary" item). Finishing the dedup means
normalizing one of the two parameter orders and updating call sites in
whichever files change.

## Onboarding carousel's button-gesture slides describe the wrong hardware

Found while writing `docs/user-manual.md`: the onboarding carousel's slide
copy (`main/onboarding_page_coordinator.cpp:19-42`, `kSlides`) describes a
button layout that doesn't match this board. Craig's read: it's modeled on
the original reTerminal Sticky's controls, not the Waveshare's — Waveshare
has a rocker switch (up/down, with a center press) plus two separate
buttons, not three discrete equal buttons.

Specifically wrong/misleading, verified against the actual button code
(`components/button_service/`, `main/button_input_runtime.cpp`,
`main/power_key_runtime.cpp`, `components/board/waveshare_board.cpp:70-75`)
and confirmed while researching `docs/user-manual.md`:

- Slide 2 ("Capture in a tap"): "Double-press to lock the screen." Locking
  is actually a **short press of `PWR`** — there's no double-press gesture
  anywhere in the button code, on any button.
- Slide 3 ("Navigate with keys"): "Key 1 selects, key 2 navigates up, and
  key 3 navigates down." This frames navigation as three separate,
  equal-weight keys. The real layout is one rocker (`UP`/`DOWN`, GPIO4/6)
  with a center-press button (`FN`, GPIO5) for select, plus a separate
  `ACTION` button that *also* selects on a quick press but records on a
  press-and-hold — none of which this slide mentions. ("Hold key 3 to
  exit certain components" is at least directionally right — that's
  `DOWN`'s long-press "exit the current control" gesture — but it's
  presented as part of the same wrong three-key model.)
- Slide 4 ("Sleep & power"): "Hold keys 1 and 2 to shut it down; press and
  hold key 1 to turn it on." There is no two-button shutdown chord
  anywhere in the code — `app_shell.cpp` explicitly rejected that idea
  ("No UP+power shutdown chord on this board... a chord would only
  duplicate [the PMIC's own key] and can misfire"). The real gesture is
  holding `PWR` for about a second, which opens a "Shut down device?"
  confirmation modal (`components/board/waveshare_board.cpp:70-75` configures
  that ~1s short/long IRQ split; a separate 6s continuous hold is a
  hardware-forced cut, independent of firmware, as a failsafe). There's
  also no "hold key 1 to turn on" gesture in the reviewed power-on path.

Fix: rewrite slides 2-4's body text to describe the actual Waveshare
control layout — the Record button (quick press to select, press-and-hold
to record), the rocker (tilt up/down to navigate, press in — "Select" —
to confirm, hold down-tilt to exit a list/scroll/switch), and `PWR` (short
press to lock/unlock, ~1s hold for the shutdown confirmation).
`docs/user-manual.md`'s ["At a glance: the
buttons"](user-manual.md#at-a-glance-the-buttons) section already has this
written accurately and can be used as the source text. Also worth checking
whether the carousel's slide *images* (`EmbeddedImageId::kSlide2`/`kSlide3`/
`kSlide4`, generated via `scripts/generate_epaper_project_assets.py` from
`assets/epaper_assets.json`) depict the old three-button layout too, not
just the body copy — if so they need regenerating/redrawing, not just the
text.

Own branch/PR.

## Replace auto light sleep with a full auto shutdown

Today, after `FOLLOWUP_AUTO_SLEEP_LIGHT_SLEEP_TIMEOUT_SECONDS` of IMU-detected
inactivity (default 1800s / 30 minutes, `main/Kconfig.projbuild:71-79`), the
device enters light sleep: `device_sleep_service::Action::kEnterLightSleep` is
dispatched to `EnterLightSleep()` in `main/device_sleep_runtime.cpp:490-492`,
which stops Wi-Fi, puts the display to sleep, and arms a GPIO wake.

Craig's reasoning: waking back up from light sleep already costs about the
same time as booting from scratch (`RestoreAfterLightSleep()` has to redo a
real Wi-Fi reassociation and a forced SD remount — see the comment on
`ForceDisplaySleep`'s Wi-Fi-reassociation cost in `lock_screen_runtime.cpp`'s
`Show()`), so light sleep isn't actually buying much wake-latency benefit
over a full power-off at that point, while a full shutdown saves
meaningfully more battery. Wanted: once the light-sleep timeout elapses, the
device should fully shut off (same mechanism as a manual shutdown,
`power_service::RequestShutdown()`) instead of entering light sleep — and
this auto-triggered shutdown should freeze on the lock/shutdown screen the
same way a manual shutdown now does (`lock_screen_runtime::ShowForShutdown()`,
added for the "Full shutdown should freeze on the lock screen's todo
summary" item, `docs/todo-archive.md`).

Open questions for whoever designs this:
- Whether to repurpose `FOLLOWUP_AUTO_SLEEP_LIGHT_SLEEP_TIMEOUT_SECONDS` as
  the auto-shutdown timeout directly, or add a distinct Kconfig option —
  "light sleep" and "auto shutdown" are conceptually different features even
  if this item replaces one with the other at the same trigger point.
- The same `BlockerReason`s that already gate light sleep (recording active,
  recording saving, audio playback, storage write, Wi-Fi AP mode, time sync,
  display refresh — `components/device_sleep_service/include/device_sleep_service.h:39-45`,
  checked in `main/device_sleep_runtime.cpp`'s blocker-evaluation function)
  presumably should gate auto-shutdown too.
- The USB-present case: `power_service::RequestShutdown()` doesn't actually
  cut power while VBUS is present — the call returns and the board stays
  running. Auto light sleep today still works fine on USB power (Wi-Fi
  stops, display sleeps, GPIO wake still armed). If auto-shutdown fires
  while on USB, naively reusing `RequestShutdown()` would leave the device
  sitting on the frozen lock/shutdown screen, powered but not actually
  asleep or off, until a button press — a real regression from today's
  behavior on a plugged-in device. Worth deciding whether auto-shutdown
  should only trigger on battery power, falling back to today's light-sleep
  behavior while USB is present.
- This is a real, deliberate UX change beyond just "one more sleep stage":
  today, any button instantly wakes the device from light sleep; after this
  change, once the timeout elapses, waking requires a full boot cycle (PWR
  press through the AXP2101's normal power-on path) instead. That's exactly
  the tradeoff Craig wants (his read: the wake cost is already boot-cost
  today), but it's worth calling out explicitly since it's a bigger
  behavioral change than the wording ("shut off instead of light sleep")
  might suggest at a glance.

Own branch/PR.

## Surface why a transcription failed, not just that it did

Craig's had a few transcriptions fail and wants to see why.

Today the real failure detail exists but doesn't last: `gemini_service::Transcribe`'s
result flows into `transcription_service`'s in-memory snapshot
(`s_last_http_status`/`s_last_error_code`/`s_last_error_message`,
`components/transcription_service/transcription_service.cpp:20-23,91-96`), and
`app_shell.cpp`'s recording-completion toast (`kComplete` case,
`app_shell.cpp:897-919`) surfaces it as a generic, transient 2.5s toast —
"Transcription failed," or "Gemini quota exceeded" specifically for a
`RESOURCE_EXHAUSTED` error code. Once that toast clears, the detail is gone:
`RecordingMetadata` (`components/recording_archive_service/include/recording_archive_service.h:21-43`)
only tracks `has_transcript` (bool), nothing about *why* it's false. An
"Audio only" item you come back to later on Notes/Todos/Details has no way
to show the reason, and a manual retry (Details page's Transcribe button,
`main/details_page_runtime.cpp:277-299`, and Vibe Check's equivalent,
`main/vibe_check_page_runtime.cpp`) reuses this same pipeline/toast, so a
second failure is just as fleeting as the first.

Likely fix: persist the last failure onto `RecordingMetadata` itself
(e.g. `last_transcription_error_code`/`last_transcription_error_message`,
alongside `has_transcript`), set whenever `transcription_service` reports a
failure for that recording, cleared on a subsequent success. Surface it
somewhere revisitable — the Details page seems like the natural spot,
maybe as a line under the Transcribe button, or in the item-actions menu
for an audio-only item on Notes/Todos.

Worth weighing against the "Redundant derived state" item above: this is
the same class of "extra field that must be kept in sync by hand" this
codebase is already trying to reduce elsewhere, so keep the persisted
fields minimal (probably just the human-readable message, not the raw
HTTP status) and make sure every write site that flips `has_transcript`
also clears/sets the error fields consistently.

Own branch/PR.

# TODO

Open items only. Resolved/closed items (with full investigation and
verification history) have moved to `docs/todo-archive.md`.

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

## Full shutdown should freeze on the lock screen's todo summary, not whatever was on screen

Per the archived "Display sleep never actually blanks the panel" item,
this board deliberately freezes the e-paper on its last-drawn screen when
powered down rather than blanking it — a frozen screen signals "powered
down but holding state" more usefully than a blank one. Today, a full
shutdown doesn't make use of that: confirming the shutdown modal
(long-press `PWR` -> `overlay_runtime::ShowShutdownModal()` ->
`app_shell.cpp`'s `ShutdownTask`) only repaints the status bar's power
icon as a partial refresh on whatever screen happened to be active
(`status_bar_runtime::SetShutdownIndicatorVisible(true)` +
`UpdateDisplayStateAndRefreshNow(RefreshMode::kPartial)`,
`app_shell.cpp:1427-1429`) before `power_service::RequestShutdown()` cuts
power (`components/power_service/power_service.cpp:450-475`, via
`Axp2101::PowerOff()`). Whatever page the user was on when they confirmed
is what's frozen on the panel until next boot.

Craig wants the frozen screen to always be the lock screen's todo summary
instead — the same one already shown on a normal lock
(`lock_screen_runtime::Show()`), regardless of what screen was active when
shutdown was confirmed.

Investigated timing/feasibility: firmware fully controls the shutdown
sequence's timing (the only hardware-autonomous cutoff is a 6s
continuous-hold rail cut, unrelated to and not racing the modal-confirm
path — the button is released long before the modal is even shown). There
is already ~700ms of firmware-imposed delay in the existing sequence
(`kPowerButtonReleaseSettleDelay` 500ms + `kShutdownSettleDelay` 200ms)
with no hardware pushback, so sequencing one more full refresh (typically
well under a second for this panel/waveform) before `RequestShutdown()`
fits comfortably.

`lock_screen_runtime::Show()` is already the right primitive for painting
it: it reads only the already-cached `pending_todo_titles`/`pending_todo_count`
(no SD I/O — `RefreshTodoSummary()`'s actual scan is a separate, async,
fire-and-forget worker task and must not be called or waited on from a
shutdown sequence) and drives a `RefreshMode::kFull` repaint via
`display_service::SetCurrentScreen(kLockScreen, RefreshMode::kFull, ...)`.
The one piece of `Show()` that should NOT run here is its trailing
`device_sleep_service::ForceDisplaySleep()` call — redundant right before
a hard power-off, and asynchronous relative to the refresh actually
finishing.

Needs a design pass before implementing:
- Whether `ShutdownTask` should call something like
  `lock_screen_runtime::Show()` directly (skipping/making optional its
  `ForceDisplaySleep()` tail call), or a new narrower entry point.
- Whether to await the full refresh actually completing (panel `BUSY`
  line / display task ack) before calling
  `power_service::RequestShutdown()`, the same way today's status-bar
  partial refresh is presumably already awaited synchronously
  (`UpdateDisplayStateAndRefreshNow` — confirm), so power doesn't cut
  mid-refresh.
- What happens if the device is already locked (lock screen already
  active) when shutdown is confirmed — likely a no-op repaint, but worth
  confirming `Show()` handles being called while already active cleanly.
- **This directly inherits the open, unresolved race in "Lock screen todo
  summary sometimes doesn't appear after PWR forces sleep" above**: if a
  `RefreshTodoSummary()` scan is mid-flight when shutdown's `Show()` call
  reads the cache, the frozen screen could show stale/empty todo data —
  and unlike a normal lock (where sleep/wake "usually fixes it"), there's
  no recovery once power is cut; it stays wrong until next boot. Worth
  deciding whether this item should fix that race first/together, or ship
  with the known limitation called out.

Own branch/PR.

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

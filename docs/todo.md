# TODO

Open items only. Resolved/closed items (with full investigation and
verification history) have moved to `docs/todo-archive.md`.

## Full refreshes are too frequent / too visible

Craig's report (2026-09-11): full refreshes happen "rather egregiously" and
should be reduced further.

Every screen-to-screen navigation in `main/app_shell.cpp` (Home, Settings,
Wifi, Time, Notes, Todos, FollowUp, Details, Topics*, onboarding, ...) always
requests `display_service::RefreshMode::kFull` -- the slowest, most thorough
waveform (`EpaperPanel::RefreshFullBase()`), used unconditionally for every
page change, not just for ghost-clearing or wake/boot recovery.

The likely low-risk win: `RefreshMode::kFast` already exists end-to-end and
is unused. `display_service.h`/`epaper_panel.h` document it as "full-screen
redraw on the panel's fast OTP waveform: quicker than `RefreshFullBase`, but
clears accumulated ghosting less thoroughly" (`components/epaper_panel/ssd1677_driver.cpp:226-232`,
`RefreshFullBaseInternal(fast)` toggling the 0x1A temperature register between
the normal and OTP-fast waveform). `RefreshForMode()` in
`components/display_service/display_service.cpp:620-637` already dispatches
`kFast` to `panel.RefreshFastBase()` -- but nothing in `main/` or
`display_service` ever constructs a `RefreshRequest` with `kFast`. It was
built for exactly this "full-screen change, not a ghost flush" case and then
never wired up to a call site.

Open questions for whoever picks this up:
- Is `kFast` actually fast/clean enough in practice for routine page
  navigation, or was it left unused because it was tried and rejected (check
  git history/PR discussion for `RefreshFastBase`/`kFastWaveformTemperature`
  before assuming it's simply forgotten)?
- Should ordinary screen navigation switch to `kFast` while reserving `kFull`
  for the existing ghost-clear flush (`EpaperPanel::NeedsGhostingFlush()`,
  8-consecutive-partials trigger), wake/light-sleep recovery, and the
  boot/onboarding first paint (`docs/app-architecture.md`'s "Boot refresh
  policy")?
- Since `kFast` clears ghosting less thoroughly, does alternating
  navigation-triggers-kFast with the existing ghost-clear-flush-triggers-kFull
  keep visible ghosting acceptable, or does it need its own counter/ceiling
  separate from the partial-refresh ghost counter?

## After a manual shutdown, holding PWR sometimes doesn't power the device back on

Craig's report: after using manual shutdown (PWR held ~1s -> confirm on the
shutdown screen), holding PWR again to turn the device back on doesn't always
work on the first attempt -- it can take a few tries before it powers on.
Doesn't seem to be a hardware issue (same physical button, same battery).

`power_service::RequestShutdown()` (`components/power_service/power_service.cpp:450-475`)
clears the PCF85063's alarm/timer interrupts, then calls `Axp2101::PowerOff()`
(`components/axp2101/axp2101.cc:134-136`), which is a thin wrapper over the
vendored XPowersLib driver's `Axp2101Driver::shutdown()`
(`components/axp2101/xpowers_axp2101_driver.cc:181-184`) -- that just sets a
soft-shutdown bit in the AXP2101's `COMMON_CONFIG` register. Nothing in
Folloup's own code runs between that register write and the board going dark
on battery, so if the power-back-on gesture is unreliable immediately after,
it's most likely either a real AXP2101 characteristic (many PMICs enforce a
minimum off-time or a debounce window after a soft-shutdown before they'll
recognize a fresh PWRON edge -- worth checking the AXP2101 datasheet for a
documented minimum off-time) or a side effect of `kShutdownSettleDelay`
(the `vTaskDelay` immediately before `PowerOff()`) being too short or too long
for the button hold that follows.

Open questions for whoever investigates:
- Does the AXP2101 datasheet specify a minimum off-time or PWRON debounce
  after a soft-shutdown command? If so, is Folloup's shutdown-confirm-to-dark
  latency (and the settle delay above) already inside or outside that window?
- Does this reproduce identically on battery vs. USB power (the VBUS-present
  case never actually powers off per the comment in `RequestShutdown`, so if
  the bug also happens on USB it points away from an AXP2101 off-time theory
  and toward something else, e.g. the PWR key's interrupt/debounce handling
  on the way back up).
- Is there a difference between a short/quick retry vs. waiting a beat before
  the next attempt -- i.e. does waiting longer make the first retry reliable,
  which would support the "minimum off-time" theory directly?

Own branch/PR once root-caused.

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


# TODO

Open items only. Resolved/closed items (with full investigation and
verification history) have moved to `docs/todo-archive.md`.

## Add a simple .txt reader

Craig wants a small text reader (2026-09-11): browse a single fixed folder on
the SD card, open a `.txt` file, and have the device remember the last
position in each file between sessions. Deliberately scoped down: **.txt
only** (no EPUB -- zip + XHTML parsing is a much bigger lift than plain text
and isn't worth it for a first cut), one flat folder (no subfolder browsing).

**Update (2026-09-11) -- maybe not blocked after all.** Cloned Waveshare's
own reference repo for this exact board
(`github.com/waveshareteam/ESP32-S3-ePaper-3.97`, `ESP-IDF/08_ESP32-S3_e-Paper-3.97`)
and it has a working on-device text reader already:
`main/page_fiction/page_fiction.cc` + `main/file_browser/file_browser.cc` --
bookmarks, byte-offset position tracked via `fseek`, a sidecar progress file
(`fprintf(fp, "%zu\n%d\n", position, page)`), page counting. Good prior art
for the shape above, and validates the byte-offset + sidecar approach
independently.

More importantly: **their page turns don't use fast mode at all.** They call
`EPD_Display_Partial()` (the same plain differential refresh our codebase
already uses for in-page updates, via `kPartial`) for turning pages, and only
do a full-waveform refresh (`EPD_Display_Base`) as an occasional forced
flush -- not on every turn. That's a vendor-tested precedent that plain
black-text-on-white page turns might hold up fine on `kPartial`, unlike the
Wi-Fi-page list-population case that ruled `kPartial` out for large-area
changes elsewhere in this codebase. Worth just trying `kPartial` for page
turns first, on real content, before assuming this needs the `kFast` tuning
work at all -- see the update on that item below for why `kFast` turned out
to be a less certain fix than it looked.

Rough shape, following existing patterns rather than inventing new ones:

- **Reading files off SD**: no new plumbing needed -- `storage_service`
  already composes `board` + `sd_card` this way, and `playback_service`
  already streams file content off SD in chunks (for clip playback) rather
  than loading a whole file into RAM. A reader service should read the fixed
  folder the same way, and stream `.txt` content in chunks rather than
  loading an entire book into the heap -- internal RAM is the tighter budget
  on this board (see recent headroom check: DIRAM ~56% used), so keep any
  buffering PSRAM-backed like the e-paper framebuffers already are, and keep
  chunks small regardless.
- **Fixed folder path**: probably a `CONFIG_FOLLOWUP_*` Kconfig default
  (`main/Kconfig.projbuild`), matching how the other build-time paths/settings
  are exposed under "Folloup Settings".
- **Remembering position**: store a small per-file sidecar on the SD card
  itself (e.g. `<filename>.pos` next to the book, holding a plain byte
  offset) rather than NVS -- matches the project's existing "everything lives
  on the SD card" philosophy (README/CLAUDE.md), and NVS's key/value model
  doesn't fit open-ended per-filename records well.
- **Pagination**: layout is fixed (one font/size, one portrait viewport), so
  pages can be computed live rather than pre-indexed -- greedily wrap text
  from the stored byte offset using the existing font/text-wrap utilities in
  `epaper_ui` (already used for menu items and timeline text) until the page
  area fills. For backward paging, keep an in-memory stack of page-start
  offsets for the current reading session (push on forward, pop on back) --
  avoids needing a persisted page index, at the cost of only being able to
  page back within the current session (paging back before the book's last
  saved position, after reopening it, would need re-deriving from the start
  or accepting forward-only after reopen -- open question below).
- **Screens**: a new `ScreenId` and the usual page-owned
  `{runtime, coordinator, interactions}` trio in `main/`, plus a page
  renderer in `epaper_ui` (depends only on `design_tokens` +
  `project_assets`, never app services, per the existing rule). Likely two
  views: a file-list (reuse the existing list/menu container widgets) and
  the actual reading view.
- **Input**: `UP`/`DOWN` tilt for prev/next page in the reading view (fits
  the existing roving-focus convention elsewhere), long-press `DOWN` to exit
  back to the file list (the app-wide "exit entered control" gesture),
  `ACTION`/`FN` to open a file from the list.

Open questions for whoever picks this up:
- Where does this hang off the app? A new Home screen menu item (Home's menu
  is already at 4 items after the recent reorder) vs. tucked under Settings
  as a lower-priority entry -- a UX call, not an architectural one.
- Backward pagination before the saved position on a freshly reopened book:
  worth solving properly, or is forward-only-until-you've-repassed-it an
  acceptable v1 limitation?
- What happens to the `.pos` sidecar if the book file is edited/replaced
  externally (byte offset now points mid-word or past EOF)? Probably just
  clamp and re-paginate from there, but worth deciding explicitly rather than
  leaving it to whatever the clamp happens to do.

Own branch/PR. Better after the `kFast` waveform tuning below, but per the
update above, worth trying `kPartial` for page turns first rather than
treating this as strictly blocked on that.

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

**Update (2026-09-11) -- it was tried, and rejected for a real reason.** Git
history answers the first open question below: commit `2e058a9` wired `kFast`
into exactly these two "automatic refresh" paths, and commit `ae15b27`
reverted it less than an hour later -- on this panel, the fast OTP waveform
"flashes like a full refresh but finishes grey," it doesn't reach full
contrast. So this isn't simply forgotten code; it's a known, real defect.

**Also checked whether another firmware for this board already solved it.**
Waveshare's own reference repo (`github.com/waveshareteam/ESP32-S3-ePaper-3.97`,
`ESP-IDF/08_ESP32-S3_e-Paper-3.97/components/epaper_port/epaper_port.c`) has
its own fast-mode init (`EPD_Init_Fast`), and its register sequence is
essentially byte-for-byte identical to ours -- including the exact same
forced temperature value, `0x1A = 0x6A`. So this was never a case of "we
have the wrong value, they have the right one." Their code comment just says
`//Fast(1.5s)`, treating it as expected behavior with no sign they scrutinized
contrast quality. Their own on-device reader (see the `.txt` reader item
below) doesn't even use fast mode for its page turns -- it uses plain
`kPartial`-equivalent refreshes instead, which is a hint that `kFast` may
just not be trustworthy on this panel for large-area content at all, at
least not at this forced-temperature value.

That reframes the fix: per the SSD1677 datasheet, the forced temperature
byte doesn't act as a continuous speed dial -- the controller does a banded
OTP lookup (about 8 temperature bands per the datasheet's example table,
"last matching band wins"), and `0x6A` (106 "degC") almost certainly lands in
the same topmost/fastest band as any other implausibly-hot value someone
might try (a community SSD1677 driver uses `0x5A`/90 "degC" for its fast
mode, which is likely in that same band and so likely no different in
practice -- not verified, but not promising either). Getting a genuinely
faster *and* full-contrast result probably means deliberately forcing a much
cooler value to land in a different OTP band, trading away some of the speed
win, found empirically since each panel's actual band boundaries are set at
manufacture and aren't published.

Open questions for whoever picks this up:
- Build the small on-device sweep harness discussed for this (cycle through
  candidate `0x1A` values with a button press, fire a real large-area swap
  on `kFast` each time, judge contrast) -- values should span a wide range
  (e.g. down through ~30/20/10/0 "degC"), not just nudge near `0x6A`.
- Should ordinary screen navigation switch to `kFast` while reserving `kFull`
  for the existing ghost-clear flush (`EpaperPanel::NeedsGhostingFlush()`,
  8-consecutive-partials trigger), wake/light-sleep recovery, and the
  boot/onboarding first paint (`docs/app-architecture.md`'s "Boot refresh
  policy")? Still the right target shape if a working value is found.
- Since `kFast` clears ghosting less thoroughly, does alternating
  navigation-triggers-kFast with the existing ghost-clear-flush-triggers-kFull
  keep visible ghosting acceptable, or does it need its own counter/ceiling
  separate from the partial-refresh ghost counter?
- Forcing a fixed temperature bypasses the panel's real temperature
  compensation -- worth checking whether a value that looks right at room
  temperature still holds up in a noticeably colder or warmer room.

**Fallback if the `kFast` tuning doesn't pan out (2026-09-11):** switch
navigation to `kPartial` (worth trying on real screens regardless -- cheap,
no waveform research needed, see the vendor-reader precedent on the `.txt`
reader item above) and add a manual "force full refresh" gesture as the
escape hatch for whatever ghosting that leaves behind, instead of only
relying on the automatic 8-partial ghost-clear flush. Doesn't fix a
single-shot undershoot the moment a large-area `kPartial` change happens
(that's a different failure mode than the gradual fade the automatic flush
already handles), but it's a cheap, low-risk thing to have either way, and
means a user who notices ghosting isn't stuck waiting for the next automatic
flush. Two things to work out if it's built: which gesture is actually free
(the current map -- `UP`/`DOWN` tilt, `ACTION`/`FN`, long-press `DOWN` for
"exit control", `PWR` reserved for the PMIC -- doesn't have an obvious
unclaimed slot) and whether it fully replaces the automatic flush or sits
alongside it as a backstop.

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


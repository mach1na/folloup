# TODO

Open items only. Resolved/closed items (with full investigation and
verification history) have moved to `docs/todo-archive.md`.

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

## Vibe Check isn't useful -- redesign or remove, possibly reclaiming its screen real estate for topic filtering

Craig doesn't find the Vibe Check screen (Home's 3rd dashboard menu item,
`ScreenId::kVibeCheck`) useful as-is and wants to either redesign it into
something worth keeping or remove it outright. Today it's a Tinder-style
shuffle through Idea-tagged recordings one at a time (Refresh/Close/Check
actions, plus Transcribe for audio-only ideas) -- pure `recording_archive_service`
CRUD, no Gemini/`summary_service` involvement.

Worth deciding alongside the "Voice-created topics" item below: if topics
ship, Home's fixed 5-item dashboard menu (Follow up / Summarize / Vibe check
/ Notes / Todos) is a natural place to make room for a topic-filtered view,
and Vibe Check's menu slot/screen real estate is the obvious candidate to
reclaim rather than adding a 6th item.

Open questions for whoever designs this:
- Redesign vs. remove: is there a version of "surface an idea you might have
  forgotten about" that's actually useful (e.g. resurfacing old, untagged,
  or long-idle ideas), or does the topics feature already solve the
  underlying problem ("I can't find my ideas") in a better way?
- If removed: `main/vibe_check_page_{coordinator,runtime,interactions}.{h,cpp}`
  and `components/epaper_ui/vibe_check_page.{h,cpp}`/`vibe_card.{h,cpp}`
  (~1,760 lines total) are fairly self-contained and could come out cleanly
  -- no other screen depends on them. The Transcribe-retry action for
  audio-only ideas is the one behavior that'd need a new home (Details page
  already has its own Transcribe button, so may already be redundant).
- If the screen slot is reclaimed for topic filtering: does that live at
  `ScreenId::kVibeCheck`'s old dashboard position, or does the whole 5-item
  menu get rethought once topics exist (Notes/Todos/Follow-up could also
  gain a topic-filter entry point instead of/alongside a dedicated menu
  item)?

Own branch/PR.

## Voice-created topics for categorizing entries, independent of Note/Idea/Todo

Craig wants a second, orthogonal categorization axis on top of the existing
Note/Idea/Todo type tag (`RecordingMetadata::tag`, set at recording time via
the tag-selection step in `recording_session_service`): user-defined topics
(e.g. a project name) that can be attached to any entry regardless of its
type, so a Note and a Todo could both carry the same "Project X" topic. New
topics should be creatable by voice, via a feature living in Settings (the
now-hub-shaped `ScreenId::kSettings` — see `docs/todo-archive.md`'s
"Consolidate Settings, WiFi, and Time" entry — would likely gain a fifth
heading for this, alongside Network/Time/Storage/Todos). Named "topics"
rather than "labels" specifically to keep it delimited from the existing
Note/Idea/Todo tag terminology.

Also fold in Summarize's future here rather than planning it separately:
Craig doesn't find the current Summarize screen useful either, but its
underlying engine (`summary_service` — token-budgeted, chunked Gemini
summarization with SD-cached results, `docs/gemini-service.md`) is worth
keeping. Today it only summarizes two fixed, static buckets (everything
tagged Note, everything tagged Task) — a blunt cut. Once topics exist, the
more useful version is almost certainly "summarize Topic X" instead: pass
`summary_service` a topic-filtered entry set rather than a tag-filtered one,
and let the Summarize page (or a summarize action reachable from a topic
view) target a specific topic rather than a fixed Notes/Todos split.

Open questions for whoever designs this:
- Storage: `RecordingMetadata` already persists a JSON sidecar per recording
  on SD (`recording_archive_service`) — topics-per-entry could be a new
  array field there, but the topic *registry* itself (the set of topic
  names that exist, so they can be voice-matched/picked rather than
  free-typed) needs its own store, probably SD-based like the recordings
  themselves rather than NVS (NVS today only holds small config, e.g. the
  `wifi`/`timezone` namespaces).
- Voice creation flow: is this a dedicated "add a topic" voice capture
  (record a short clip, Gemini extracts just a topic name) separate from
  the normal press-and-hold recording flow, or a step folded into an
  existing flow? Needs a phase-machine home if it's the former — compare
  `recording_session_service`'s existing `kIdle -> kArmed -> ... ->
  kComplete` phase machine for the main recording flow.
- Assignment: one topic per entry or multiple? Assigned at recording time
  (extending the existing tag-selection step) or after the fact from the
  Details page?
- Browsing/filtering: Notes/Todos/Follow-up today are single-axis timelines
  grouped by day (`notes_page_coordinator`/`todos_page_coordinator`/
  `follow_up_page_coordinator`) — filtering or grouping by topic would need
  a second navigation dimension (e.g. a topic picker/segment control) on
  top of those pages' existing day-grouping and (for Todos) Current/Archived
  segment control.
- Topic lifecycle: renaming or deleting a topic needs to touch every entry
  that references it (rewriting sidecars on SD), which is a bigger
  operation than anything `recording_archive_service` does today (its
  existing mutators, e.g. `MarkRecordingCompleted`/`SaveTranscriptionFailure`,
  all touch a single recording's sidecar, not a cross-cutting rewrite).
- Summarize integration: does topic-scoped summarization replace the
  existing Notes/Todos summary buckets outright, or sit alongside them?
  `summary_service`'s existing per-kind cache (Notes/Todos) would need a
  per-topic cache shape instead of/in addition to that.

Own branch/PR.


# TODO

## Defer the ghosting flush to idle instead of firing mid-interaction

The SSD1677 driver forces a full-waveform refresh every 8 consecutive partial
refreshes (`kMaxPartialRefreshesBeforeFlush` in
`components/epaper_panel/ssd1677_driver.cpp`) to fight the contrast fade that
whole-screen partial refresh leaves on unchanged pixels. Because every
roving-focus move (menu/timeline `UP`/`DOWN`) does a partial refresh, arrowing
through more than 8 items triggers a full, flashy refresh mid-navigation.
Separately, `DetermineOverlayRefreshPolicy` in `main/overlay_runtime.cpp`
always forces a full refresh when a select modal/keyboard/sticky note is
dismissed, so picking a menu option (e.g. the Idea/To-do/Note tag picker) also
triggers one on dismiss by design.

Best practice for a panel like this (no windowed partial, must periodically
re-drive to bound ghosting): decouple "ghosting budget exhausted" from "flush
now." Set a `flush_needed` flag when the partial counter hits its threshold,
and only execute the full refresh at the next natural idle point (a pause in
input, or a screen/mode transition) instead of on whatever operation happens
to be the 8th partial. Keep a secondary hard ceiling (e.g. 2-3x the normal
budget) so continuous uninterrupted input can't defer the flush indefinitely.

Tradeoff: more state to track (debounce/idle timer alongside the existing
counter), and the ghosting bound becomes "N partials past the next idle gap"
rather than a hard cap unless that secondary ceiling is added.

Relevant files:
- `components/epaper_panel/ssd1677_driver.cpp` (`kMaxPartialRefreshesBeforeFlush`,
  `RefreshPartialFullScreen`)
- `main/overlay_runtime.cpp` (`DetermineOverlayRefreshPolicy`)
- `main/ui_refresh_runtime.cpp` (the keyed latest-wins refresh worker this
  would need to plug into)

Also worth a small drive-by fix while in this area: the comment at the top of
`ssd1677_driver.cpp` (`kMaxPartialRefreshesBeforeFlush` block) still says "the
flush is now on the fast waveform, so a tighter budget costs little" — stale
since commit `ae15b27` reverted the flush back to the slow, full mode-1
waveform because the fast OTP waveform settled at visibly lower contrast.

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

## Battery usage investigation

Look at overall battery efficiency and see where we can improve it —
covers active draw (audio codec kept on for its lifetime per
`docs/app-architecture.md`, Wi-Fi, display refresh frequency) and sleep-path
draw (light sleep / display sleep via `device_sleep_service` +
`main/device_sleep_runtime.cpp`, AXP2101 rail behavior). No investigation
done yet — needs profiling before deciding what (if anything) to change.

Own branch/PR.

## Reorder footer icons for usability

The bottom footer icon order isn't intuitive — revisit the order to make it
more user-friendly. Needs a proposed new order (from Craig or from usage
patterns) before implementing; footer rendering lives in `epaper_ui`.

Own branch/PR.

## Tone down the summarize/vibe-check prompts

The Gemini prompts used for summarizing (`summary_service`, see
`docs/gemini-service.md`) read too "chummy" — investigate the current prompt
text and adjust tone to be more neutral/professional.

Own branch/PR.

## Offline transcription queue doesn't actually work

Auto-retry for offline-recorded notes was added in commit `3366004`
("Auto-retry transcription for notes recorded offline") but it's not working
end-to-end:

- No badge/indicator showing how many notes/todos are still untranscribed.
- No automatic retry trigger when Wi-Fi reconnects — needs to actually kick
  off transcription for queued items on network-back, not just be able to
  retry if something else triggers it.

Needs investigation into why the existing auto-retry logic isn't firing
before deciding on a fix.

Own branch/PR.

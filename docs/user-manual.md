# Folloup User Manual

Folloup is a pocket voice notebook. Press a button, speak your thought, tag
it, and Folloup keeps it organized — transcribed, summarized, and sorted
into Notes, Todos, and Follow-ups — on a screen that sips power and stays
legible even when it's "asleep."

This manual describes the device as it actually behaves today. A couple of
spots in the on-device onboarding carousel describe older button gestures
that no longer apply — this manual is the accurate reference.

## Contents

- [At a glance: the buttons](#at-a-glance-the-buttons)
- [First-time setup](#first-time-setup)
- [Recording a thought](#recording-a-thought)
- [The Home screen](#the-home-screen)
- [Notes, Todos, and Follow-up](#notes-todos-and-follow-up)
- [Viewing a recording's details](#viewing-a-recordings-details)
- [Vibe Check](#vibe-check)
- [Summarize](#summarize)
- [Settings](#settings)
- [Locking, sleep, and battery](#locking-sleep-and-battery)
- [Shutting down and charging](#shutting-down-and-charging)
- [Tips and things that surprise people](#tips-and-things-that-surprise-people)

## At a glance: the buttons

Folloup has three physical controls. There is no touchscreen — everything
is driven by these.

| Control | Quick action | Press and hold |
|---|---|---|
| **Record button** | Select / confirm — same as pressing the rocker | **Record** a thought (see below) |
| **Rocker** (tilt up / tilt down / press in) | Tilt up or down to move the selection; press the rocker straight in (**Select**) to select/confirm | Hold a tilt to keep moving in that direction; **holding the down-tilt is also the universal "back out" gesture** — it exits whatever you're currently inside (an open list, a scroll view, a switch) |
| **PWR** | Short press: **lock / unlock** the screen | Hold ~1 second: open a **shut down?** confirmation |

A few things worth knowing up front:

- **The rocker is one control, not three buttons.** Tilting it up or down
  moves your selection; pressing it straight in — referred to as
  **Select** in this manual — selects/confirms, the same as a quick press
  of the Record button.
- **The Record button does double duty.** A quick tap selects/confirms,
  just like Select. Press and *hold* it, and it starts recording instead.
  You don't need to reach for a different control to navigate menus versus
  to record.
- **Holding PWR for a full 6 seconds forces the power off**, no matter what
  the screen is doing. This is a hardware failsafe, not something you
  should need in normal use.
- The screen is e-paper: black-and-white, and it **visibly flashes** on a
  full-screen refresh (opening a new screen, waking up). Smaller updates
  (moving a selection, a checkbox) redraw more quietly. This is normal.

## First-time setup

On first boot, Folloup walks you through a short introduction carousel.
Use the **rocker** to move through it (tilt to move, press in or tap the
**Record button** to confirm), and close it on the final slide. You can
replay this carousel any time from **Settings → Manual**.

To actually get set up, Folloup opens its own WiFi hotspot named
**`Followup-XXXXXX`** (the last few characters are unique to your device)
whenever it doesn't have saved WiFi credentials yet. Connect a phone or
laptop to that hotspot — it's open, no password needed — and your device
should automatically open a setup page (or visit `http://192.168.4.1`
yourself). From there you can:

- **Connect Folloup to your WiFi** — scan for networks, pick yours, enter
  the password.
- **Add your Gemini API key** — this is what powers transcription and
  summaries. Without it, recordings still save to the SD card, they just
  won't be transcribed.
- **Set your timezone and the current time/date** — needed for accurate
  timestamps on your recordings and for the "Today" grouping on Notes/Todos.

You can come back to WiFi and time settings later from the device itself
(see [Settings](#settings) below — they're the Network and Time pages in
the Settings hub) — the setup portal is only needed the first time, or if
you want to change your Gemini key.

## Recording a thought

This is the core interaction:

1. **Press and hold the Record button.** You'll hear a start cue.
   Recording actually begins fractionally before the cue finishes, so
   your first word isn't cut off — and Folloup even keeps a rolling
   one-second buffer while the button is held, so a beat of audio right
   before you "properly" started holding it is still caught.
2. **Speak.** Recordings are capped at **10 seconds** — Folloup is built
   for quick captures, not long dictation. If you keep holding past 10
   seconds, it stops automatically.
3. **Release the Record button.** You'll hear a stop cue, and Folloup
   immediately plays your recording back to you.
4. **Choose a tag**, from a menu with four options:
   - **Note** — a general thought.
   - **Task** — something to do. (This is what shows up on the **Todos**
     screen — the tag itself is called "Task," even though the screen
     that lists them is called "Todos.")
   - **Idea** — something to mull over later. Ideas are also what the
     [Vibe Check](#vibe-check) screen surfaces for triage.
   - **Discard** — throws the recording away. Nothing is saved to the SD
     card if you pick this — that's exactly why the review-then-tag order
     exists: you get to hear it back before deciding.
5. If you tagged it as something other than Discard, and Folloup has WiFi
   and a Gemini key configured, it transcribes the recording in the
   background — you'll see a brief "Transcribing recording..." message,
   then either a transcript being saved or a note that transcription
   failed.

**If you're offline when you record**, the clip still saves to the SD
card — it just gets marked as pending. The moment Folloup reconnects to
WiFi, it automatically retries every pending recording. A small badge on
the status bar shows how many are waiting.

## The Home screen

Home is Folloup's dashboard. Near the top you'll see:

- A **task tracker** — "`done`/`total` completed" with a progress bar, or
  "No tasks yet" if you haven't tagged anything as a Task.
- Today's date and a rotating welcome message.

Below that is the main menu, in this order:

- **Follow up** — anything you've flagged for later, regardless of what
  it's tagged as (badge shows how many).
- **Summarize** — Gemini-generated summaries of your Notes or Todos.
- **Vibe check** — a quick way to triage your Ideas one at a time.
- **Notes** (badge shows how many) — your Note and Idea recordings.
- **Todos** (badge shows how many) — your Task recordings.

Move between them by tilting the rocker, open one with the Record button
or Select (pressing the rocker in).

## Notes, Todos, and Follow-up

These three screens share the same layout: recordings are grouped by the
day they were made. Select a day to open it, and you'll see the individual
recordings inside — each showing a snippet of its transcript (or "Audio
only" if it hasn't been transcribed yet), its time, and its length. Hold
the rocker's down-tilt to back out of an opened day and return to the
list of days.

What shows up where:

- **Notes** — everything tagged Note or Idea.
- **Todos** — everything tagged Task.
- **Follow-up** — anything you've flagged as a follow-up, whatever its tag,
  shown with its actual tag labeled on the row.

Selecting an item opens an actions menu. The exact options depend on the
screen:

| Action | Notes | Todos | Follow-up |
|---|:---:|:---:|:---:|
| Play recording *(only if audio still exists)* | ✓ | ✓ | ✓ |
| View details | ✓ | ✓ | ✓ |
| Follow up / Remove follow-up | ✓ | ✓ | — |
| Turn to task | ✓ | — | — |
| Complete / Mark incomplete | — | ✓ | — |
| Complete follow-up | — | — | ✓ |
| Archive now *(only once completed)* | — | ✓ | — |
| Delete | ✓ | ✓ | ✓ |

A couple of notes on these:

- **Turn to task** (on Notes) is how you promote a Note or Idea into a
  Todo after the fact, without re-recording it.
- **Complete follow-up** (on the Follow-up screen) is one-way — it marks
  the item done and clears the follow-up flag, rather than toggling back
  and forth.
- **Delete removes the recording for good** — there's no separate
  "archive vs. delete" distinction here; that's a different feature (see
  next).

### Archiving completed Todos

At the top of the Todos screen is a **Current / Archived** switch. Select
it and tilt the rocker to flip between the two views.

A completed Task doesn't disappear right away — it stays visible (checked
off) in Current for a while, so you can see what you've gotten done.
After it's been completed for a configurable number of days (7 by
default — see [Settings](#settings)), Folloup automatically **archives**
it: the audio recording is deleted to free up SD card space, but the
transcript text is kept, and the item moves to the Archived view where you
can still read it. You can also archive a completed item immediately with
**Archive now** from its actions menu, rather than waiting.

Un-completing an archived item (via "Mark incomplete") brings it back to
Current — but the deleted audio does not come back.

## Viewing a recording's details

"View details" opens the full transcript for a recording, scrollable if
it's long — select the scroll area and tilt the rocker to move through it
in 10% steps. At the bottom:

- **Back** returns you to wherever you came from.
- The second button is either **Play** (if a transcript exists) or
  **Transcribe** (if it doesn't yet). If the transcript exists but the
  audio file itself is gone (for example, after archiving, or if you
  removed files over USB), this button disappears entirely rather than
  offering to play nothing.

## Vibe Check

Vibe Check is a one-at-a-time triage flow for your **Ideas** — specifically
the ones you haven't already flagged as a follow-up. A progress readout at
the top shows how many are left ("`remaining`/`initial` ideas").

For the idea currently on screen, you can:

- **Refresh** — skip to a different random idea, leaving this one as-is.
- **Check** — pin this idea as a follow-up (it'll then show up on the
  Follow-up screen, and drop out of the Vibe Check queue).
- **Close** — **permanently delete** this idea. This isn't a "skip" —
  treat it like the Delete action elsewhere.
- **Transcribe** (star icon, only for audio-only ideas) — transcribe it
  before deciding.

When you've gone through everything, or haven't recorded any Ideas yet,
you'll see "No ideas captured yet."

## Summarize

Summarize gives you a Gemini-written recap of either your **Notes** or your
**Todos** — switch between the two with the segment control at the top,
the same style of switch used on the Todos screen. Press **Get summary**
to have Gemini generate a fresh one for whichever is selected; the result
appears in the scrollable area below (same 10%-step scrolling as the
Details page). If Gemini isn't connected yet, you'll see a prompt to set
it up instead of a summary.

## Settings

Settings is a hub, like a phone's settings app: four headings, each
opening its own page, plus one action below them:

- **Network** — WiFi and Access Point.
- **Time** — timezone and clock.
- **Storage** — SD card status and management.
- **Todos** — todo archiving.
- **Manual** — replays the first-time onboarding carousel. This one isn't
  a heading — it's a direct action, since there's nothing to configure on
  it.

Select a heading to open its page. Every one of those pages has its own
**Back** button at the bottom that returns you to this Settings hub. The
footer's **Home** icon, on any of them, is different — it jumps straight
back to the Home screen, skipping the hub.

### Network

From the top:

1. **WiFi** — toggle WiFi on or off.
2. **Access Point** — toggle Folloup's own setup hotspot on or off.
3. A list of nearby networks — each row indicates whether it's open or
   password-protected, its signal strength, and whether it's the one
   you're currently connected to. Select a network, enter its password if
   it needs one (there's a show/hide toggle for what you've typed), and
   press **Connect**. If you're already connected to the highlighted
   network, that same button reads **Disconnect** instead. Use **Scan**
   to refresh the list.

### Time

Set your timezone from a picker, or enter the date and time by hand in the
fields provided (hour, minute, AM/PM, month, day, year). Press **Sync &
Save** when you're done — it both saves what you entered and re-syncs the
clock.

### Storage

1. Free space and usage percent on the SD card (only shown when a card is
   inserted and mounted).
2. **Enable OTG** — mounts the SD card as a USB drive on a computer you've
   connected via USB-C, so you can pull files off directly. While this is
   active, the card is unavailable to Folloup itself, and you'll need to
   explicitly **Disable OTG mode** to get it back (there's no accidental
   way out of this while the cable's connected — that's intentional, so
   Folloup doesn't touch the card while your computer has it mounted).
3. **Format SD** — erases the SD card completely. Folloup asks you to
   confirm ("Formatting the SD card will erase everything on the card") —
   there's no undo once you confirm.

### Todos

**Archive todos after** — how many days a completed Todo stays in the
active list before auto-archiving (see [Archiving completed
Todos](#archiving-completed-todos)). Choices are 7, 14, 30, 60, or 90
days, or **Never** to turn off automatic archiving entirely (you can
still archive individual items manually). Default is 7 days.

## Locking, sleep, and battery

**Locking**: a short press of PWR locks the screen. Locking also puts the
display straight to sleep — it doesn't wait out the usual inactivity
timer — so a locked device you drop in a pocket or bag goes dark right
away rather than staying lit for a few more minutes. The frozen screen
you'll see while locked is a quick summary of your pending Todos (see
below), not a clock.

While locked, **jostling the device does not wake it** — only a real
button press does. This is deliberate: motion alone (walking, being carried
in a bag) is ignored so a locked device doesn't keep flashing itself awake.

**Unlocking**: press PWR again. Unlike locking, it lands you straight back
on the Home screen in one step rather than needing a second press.

**The lock screen itself** shows, at a glance:
- The current weekday and date.
- A **"TO-DO"** heading — or **"TO-DO - N PENDING"** if you have pending
  Todos — followed by up to five of them (anything flagged as a
  follow-up is shown first). If there are more than five, you'll see a
  "+N more pending" line instead of a longer list. If you're all caught
  up, it just says so.

**Auto-sleep** (when you're not locked, but the device has just been left
alone): after about 3 minutes of no motion, the screen goes to sleep —
it freezes on whatever was last shown rather than blanking, so it still
reads as "resting," not "off" or broken. After about 30 minutes of
continued inactivity, the device goes into a deeper low-power sleep and
disconnects WiFi; press the Record button or PWR to wake it back up.

## Shutting down and charging

**Shutting down**: hold PWR for about a second. A confirmation appears —
"Your device will power off. Do you want to continue?" — with **Cancel**
and **Shut down** options. Confirming powers the device off. If it's
plugged into USB power at the time, it'll actually stay on until you
unplug the cable — shutting down and unplugging together is what fully
powers it off on external power.

As a last resort, **holding PWR continuously for 6 seconds cuts power
immediately**, regardless of what the screen is doing — useful if the
device is ever unresponsive.

**Charging**: plug in via USB-C. The status bar shows your battery
percentage and a charging indicator while power is connected.

## Tips and things that surprise people

- **Recordings are short on purpose** — 10 seconds max. Think "quick
  capture," not dictation.
- **Discard really discards** — nothing touches the SD card until you
  pick a tag other than Discard.
- **"Task" is what "Todos" means** — if you're looking for the tag that
  puts something on your Todos list, it's called Task in the tagging
  menu.
- **Archiving isn't deleting** — an archived Todo's transcript stays
  readable in the Archived view; only its audio goes away, and only after
  it's already been marked complete for a while (or you archive it
  yourself).
- **Vibe Check's Close is permanent** — it deletes the idea, it doesn't
  just dismiss it from the list.
- **A locked, sleeping screen isn't broken** — e-paper stays on whatever
  it last drew when it sleeps; that's expected, not a bug.

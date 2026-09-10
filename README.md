# Followup Product Introduction

Followup is a place to capture your thoughts — whether it's an idea, a to-do, or just a note. Record what's on your mind at that light-bulb moment, before it slips away, and Followup helps you organize it afterward. With Gemini, your recordings are transcribed automatically, and you can ask for a written summary of your Notes or Todos any time. Everything is stored on your SD card.

It runs on the [Waveshare ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97), so your thoughts live on a quiet, always-on screen you can place anywhere — a constant, low-interruption reminder instead of one more notification buried in your phone.

For the full walkthrough of every screen and control, see the [user manual](docs/user-manual.md).

## One-Sentence Positioning

**Followup is a voice-first thought-capture companion on always-on ePaper: record ideas, to-dos, and notes in the moment, let Gemini transcribe them and summarize what matters on demand, and keep your pending todos in view even when the screen is locked or off.**

## What It Is Suitable For

- Capturing a sudden idea by voice at the light-bulb moment, before it's forgotten
- Jotting quick to-dos and notes hands-free while you're in the middle of something else
- Keeping a device on a desk, fridge, or wall that shows your pending todos at a glance, even locked or powered off
- Revisiting past ideas later to decide what's still worth pursuing
- Anyone who wants their thoughts organized without living inside another app on their phone

## Key Features

### 1. Capture at the Light-Bulb Moment

Press record and speak. Every capture starts as a voice recording, tagged as an **Idea**, a **To-do**, or a **Note**, so you can get the thought down the instant it arrives without stopping to type.

### 2. Gemini Transcription, Plus On-Demand Summaries

Once a recording is saved, Gemini automatically transcribes it in the background, turning your voice into readable text. From the Summarize screen, you can also ask Gemini for a fresh written recap of everything currently in your Notes or your Todos, generated on demand rather than after every recording.

A Gemini API key from [Google AI Studio](https://aistudio.google.com/) is required. You can get started on the free tier, subject to Gemini's free-tier limits, or use a paid account without those limits.

### 3. Everything Stored on Your SD Card

Recordings, transcripts, and summaries are stored locally on the device's SD card. Your thoughts stay with you, on your own storage.

### 4. Vibe-Check Your Ideas

Ideas don't all age well. Review each one and decide whether it's still a vibe worth keeping — or something to trash so you can move on with a clear head.

### 5. Follow Up on Tasks and Notes

Mark a task or note as a follow-up to keep it on your radar. Followup helps you stay on track and focused on what actually needs doing next.

### 6. Your Pending Todos, Visible at a Glance

Lock the screen — or shut the device down — and the ePaper freezes on a summary of your pending todos instead of going blank or dark. Because e-paper holds its image with no power, that summary stays legible and in front of you even while the device is locked, asleep, or fully off.

### 7. Browse Your Follow-Ups as Stickies

From the Home screen, open your flagged follow-ups as a stack of sticky notes you browse one at a time — a quick way to page back through what you've marked as worth revisiting, whatever it's tagged as.

## Typical Applications

| Application | Description |
| --- | --- |
| Idea | Capture a spark by voice and revisit it later with a vibe check |
| To-do | Record a task hands-free and follow up until it's done |
| Note | Keep a quick thought or reminder, transcribed automatically |
| Follow-up | Flag the items that matter so they stay top of mind |
| Lock screen | See your pending todos at a glance whenever the device is locked or shut down |
| Stickies | Browse your flagged follow-ups one at a time from the Home screen |
| Summaries | Ask Gemini for a fresh written recap of your Notes or Todos, on demand |

## Brief Specifications

Followup runs on the [Waveshare ESP32-S3-ePaper-3.97](https://docs.waveshare.com/ESP32-S3-ePaper-3.97).

| Item | Information |
| --- | --- |
| Product Name | Followup (on ESP32-S3-ePaper-3.97) |
| Product Type | Voice-capture notes app on an ePaper terminal |
| MCU | ESP32-S3R8, dual-core Xtensa LX7 up to 240MHz |
| Memory | 8MB PSRAM, 16MB flash |
| Screen | 3.97-inch black-and-white ePaper, 800 x 480, SSD1677 controller |
| Interaction | Buttons only — this board has no touchscreen (see [Controls](#controls)) |
| Connectivity | 2.4GHz Wi-Fi (802.11 b/g/n), Bluetooth 5 (LE) |
| Audio | ES8311 codec, onboard microphone, NS4150B amplifier, speaker header |
| Sensors | QMI8658 6-axis IMU, PCF85063 real-time clock |
| Power | AXP2101 PMIC, 3.7V lithium battery (MX1.25 connector), USB-C charging |
| Storage | microSD card (recordings, transcripts, summaries) |
| AI | Gemini (cloud) transcription and summarization, over Wi-Fi |

The board also carries an SHTC3 temperature/humidity sensor on the shared I2C bus. Followup does not currently read it.

## Controls

Followup is driven entirely by three physical controls: a rocker, the Record button, and the PWR button. There is no touchscreen.

| Control | Action |
| --- | --- |
| Rocker, tilt up / down | Move the selection; hold a tilt to keep moving |
| Rocker, hold down-tilt | Back out of a list, scroll view, or switch you're currently inside |
| Rocker, press in ("Select") | Select / confirm |
| Record button, tap | Select / confirm — same as Select |
| Record button, press and hold | Record — starts on the hold, stops when you let go |
| PWR, tap | Lock the screen, or unlock it |
| PWR, hold ~1s | Open the shutdown confirmation |
| PWR, hold 6s | Hardware power-off, straight from the PMIC, no matter what's on screen |

Recording is exclusive to the Record button, so no other control can start or stop a capture by accident. The 6-second PWR hold bypasses the firmware entirely and always cuts power.

See the [user manual](docs/user-manual.md) for how these controls apply on every screen.

## Product Value Summary

The value of Followup is a quiet, always-visible place to catch your thoughts and keep the important ones in front of you. Instead of losing an idea to a forgotten note app or burying a task in a notification stream, you speak it in the moment, let Gemini turn it into clean text and a summary, and keep everything private on your SD card.

Ideas get a vibe check so you only carry forward what still matters. Tasks and notes become follow-ups so you stay on track. Your pending todos stay visible at a glance on the lock screen — even locked, asleep, or shut down — and the follow-ups you've flagged are always a button away as stickies: together, a steady, low-interruption view of what's next.

## License and Attribution

Followup is licensed under the [GNU GPLv3](LICENSE). This repository is a fork of [ALXV's folloup-sticky](https://github.com/alxv2016/folloup-sticky), whose `main` branch targets SeeedStudio's reTerminal Sticky and which also has a `folloup-waveshare` branch porting it to the Waveshare ESP32-S3-ePaper-3.97. This fork builds on that Waveshare work and makes it the primary target going forward. Thank you to ALXV for the original design and implementation this project builds on.

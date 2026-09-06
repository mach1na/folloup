#include "details_page_coordinator.h"

#include "project_assets.h"
#include "shared_page_interactions.h"
#include "timeline_format.h"

namespace {

using recording_archive_service::RecordingEntry;
using page_navigation::NavigationItemRole;

constexpr int kScrollStepPercent = 10;
constexpr const char* kNoTranscriptMessage = "No transcript available.";

}  // namespace

DetailsPageCoordinator::DetailsPageCoordinator()
{
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kDetailsPageScrollContainer));
}

void DetailsPageCoordinator::QueueShow(const std::string& recording_id,
                                       DetailsPageSource source_page)
{
    pending_recording_id_ = recording_id;
    pending_source_page_ = source_page;
}

void DetailsPageCoordinator::Show(const std::vector<RecordingEntry>& recordings)
{
    scroll_container_active_ = false;
    ResetScrollPosition();
    title_text_ = "Details";
    recording_header_ = {};
    transcript_text_.clear();
    has_transcript_ = false;

    if (!pending_recording_id_.empty()) {
        recording_id_ = pending_recording_id_;
        source_page_ = pending_source_page_;
        pending_recording_id_.clear();
        pending_source_page_ = DetailsPageSource::kUnknown;
    }

    RefreshFromArchive(recordings);
    // Fresh page load: home focus on the scroll container. RefreshFromArchive() has already rebuilt
    // the navigation model to match whether this recording has a transcript.
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kDetailsPageScrollContainer));
}

void DetailsPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& recordings)
{
    const RecordingEntry* entry = FindEntry(recordings);
    if (entry == nullptr) {
        title_text_ = "Details";
        recording_header_ = {};
        transcript_text_.clear();
        has_transcript_ = false;
    } else {
        ApplyEntry(*entry);
    }
    UpdateNavigationModel();
}

void DetailsPageCoordinator::UpdateNavigationModel()
{
    // The primary action button is present for a recording (Play once a transcript
    // exists, Transcribe otherwise) except when a transcript exists but the audio
    // itself is gone -- there, "Play" would have nothing to do, so the control slot
    // (and its focus target) is dropped entirely, matching show_transcribe_button in
    // BuildState().
    const bool want_primary = !(has_transcript_ && !has_audio_file_);
    const bool have_primary =
        navigation_model_.IndexOfRole(NavigationItemRole::kDetailsPageTranscribeButton) >= 0;
    if (want_primary == have_primary) {
        return;
    }
    navigation_model_ = page_navigation::BuildDetailsPageNavigationModel(want_primary);
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kDetailsPageScrollContainer));
}

bool DetailsPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    if (scroll_container_active_) {
        const shared_page_interactions::ScrollStepResult step =
            shared_page_interactions::StepScrollPercent(scroll_position_percent_, delta,
                                                         kScrollStepPercent);
        if (!step.changed) {
            return false;
        }
        scroll_position_percent_ = step.value;
        return true;
    }
    return focus_.Move(delta);
}

bool DetailsPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool DetailsPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

bool DetailsPageCoordinator::EnterScrollContainer()
{
    if (scroll_container_active_) {
        return false;
    }
    scroll_container_active_ = true;
    return true;
}

bool DetailsPageCoordinator::ExitScrollContainer()
{
    if (!scroll_container_active_) {
        return false;
    }
    scroll_container_active_ = false;
    return true;
}

epaper_ui::DetailsPageState DetailsPageCoordinator::BuildState() const
{
    epaper_ui::DetailsPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.title_text = title_text_;
    state.recording_header = recording_header_;
    state.scroll_container.content_text = has_transcript_ ? transcript_text_ : std::string();
    state.scroll_container.empty_state_message =
        has_transcript_ ? std::string() : kNoTranscriptMessage;
    state.scroll_container.focused =
        IsRoleFocused(NavigationItemRole::kDetailsPageScrollContainer) || scroll_container_active_;
    state.scroll_container.active = scroll_container_active_;
    state.scroll_container.scroll_position_percent = scroll_position_percent_;
    state.back_button.label_text = "Back";
    state.back_button.selected = IsRoleFocused(NavigationItemRole::kDetailsPageBackButton);
    // The primary action button sits beside Back: it plays the recording once a
    // transcript exists, and otherwise transcribes it. (The reused button/role is
    // still named "transcribe" in the layout; only the label and intent vary.)
    // Hidden when a transcript exists but the audio itself is gone (e.g. removed via
    // USB-OTG SD access) -- "Play" would otherwise show and silently do nothing.
    state.show_transcribe_button = !(has_transcript_ && !has_audio_file_);
    state.transcribe_button.label_text = has_transcript_ ? "Play" : "Transcribe";
    state.transcribe_button.selected =
        IsRoleFocused(NavigationItemRole::kDetailsPageTranscribeButton);
    return state;
}

const RecordingEntry* DetailsPageCoordinator::FindEntry(
    const std::vector<RecordingEntry>& recordings) const
{
    if (recording_id_.empty()) {
        return nullptr;
    }
    for (const RecordingEntry& entry : recordings) {
        if (entry.recording_id == recording_id_) {
            return &entry;
        }
    }
    return nullptr;
}

void DetailsPageCoordinator::ApplyEntry(const RecordingEntry& entry)
{
    const std::string transcript = timeline_format::TrimTranscript(entry.transcript_text);
    has_transcript_ = entry.metadata.has_transcript && !transcript.empty();
    has_audio_file_ = entry.has_audio_file;
    transcript_text_ = transcript;
    // "Details" (this page's own fallback title) when there's no date at all, rather than
    // timeline_format::FormatDateLabel's own "Today" fallback for that case.
    title_text_ = entry.metadata.created_local_date.empty()
                      ? "Details"
                      : timeline_format::FormatDateLabel(entry.metadata.created_local_date);

    recording_header_ = {};
    recording_header_.icon_asset = project_assets::GetIcon(
        has_transcript_ ? EmbeddedIconId::kTranscribe : EmbeddedIconId::kAudio);
    recording_header_.tag_icon_asset =
        entry.metadata.follow_up ? project_assets::GetIcon(EmbeddedIconId::kPin) : nullptr;
    recording_header_.time_text = timeline_format::FormatTimeLabel(
        entry.metadata.time_valid, entry.metadata.created_unix_seconds);
    recording_header_.minute_seconds_text =
        timeline_format::FormatDurationLabel(entry.metadata.duration_ms);
    recording_header_.tag_text = timeline_format::TagText(entry.metadata.tag);
    recording_header_.selected = false;
}

#include "vibe_check_page_coordinator.h"

#include <algorithm>

#include "esp_random.h"
#include "generated_epaper_icons.h"
#include "timeline_format.h"

namespace {

using page_navigation::NavigationItemRole;
using recording_archive_service::RecordingEntry;
using recording_archive_service::RecordingTag;

constexpr const char* kMessageText =
    "Some ideas are worth a follow-up, others aren't. Discard it, or turn it into a note or to-do.";
constexpr const char* kEmptyStateMessage = "No ideas captured yet.";
constexpr const char* kAudioOnlyMessage = "Audio only note...";

}  // namespace

VibeCheckPageCoordinator::VibeCheckPageCoordinator()
{
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kVibeCheckPageCard));
    RebuildCardState();
}

void VibeCheckPageCoordinator::PrepareForShow()
{
    focus_.Configure(navigation_model_.item_count,
                     navigation_model_.IndexOfRole(NavigationItemRole::kVibeCheckPageCard));
    card_active_ = false;
    action_focus_.Configure(epaper_ui::kVibeCardActionCount, 0);
    session_initialized_ = false;
    initial_idea_count_ = 0;
}

void VibeCheckPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& entries)
{
    LoadIdeas(entries, !session_initialized_);
}

bool VibeCheckPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    // While the action row is live, UP/DOWN steps through the card's actions. Because this
    // device has no dedicated back key, stepping past either edge leaves the card and hands
    // the movement back to page navigation (card <-> footer) so focus can never get trapped.
    if (card_active_ && HasIdeas()) {
        const int next = action_focus_.index() + delta;
        if (next < 0 || next >= ActionCount()) {
            card_active_ = false;
            return focus_.Move(delta);
        }
        return action_focus_.SetIndex(next);
    }
    return focus_.Move(delta);
}

bool VibeCheckPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool VibeCheckPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

page_navigation::NavigationItemRole VibeCheckPageCoordinator::FocusedRole() const
{
    const page_navigation::NavigationItemDescriptor* item =
        navigation_model_.ItemAt(focus_.index());
    return item != nullptr ? item->role : NavigationItemRole::kUnknown;
}

bool VibeCheckPageCoordinator::HasTranscribeAction() const
{
    const recording_archive_service::RecordingEntry* entry = FindCurrentIdea();
    return entry != nullptr && !entry->metadata.has_transcript;
}

int VibeCheckPageCoordinator::ActionCount() const
{
    return HasTranscribeAction() ? epaper_ui::kVibeCardActionCount + 1
                                 : epaper_ui::kVibeCardActionCount;
}

epaper_ui::VibeCardActionSelection VibeCheckPageCoordinator::ActionSelectionAt(int index) const
{
    // Audio-only ideas lead with Transcribe; every idea then has Refresh / Close / Check.
    int cursor = index;
    if (HasTranscribeAction()) {
        if (cursor == 0) {
            return epaper_ui::VibeCardActionSelection::kTranscribe;
        }
        --cursor;
    }
    switch (cursor) {
        case 0:
            return epaper_ui::VibeCardActionSelection::kRefresh;
        case 1:
            return epaper_ui::VibeCardActionSelection::kClose;
        case 2:
            return epaper_ui::VibeCardActionSelection::kCheck;
        default:
            return epaper_ui::VibeCardActionSelection::kNone;
    }
}

int VibeCheckPageCoordinator::ActionIndexFor(epaper_ui::VibeCardActionSelection selection) const
{
    const int count = ActionCount();
    for (int index = 0; index < count; ++index) {
        if (ActionSelectionAt(index) == selection) {
            return index;
        }
    }
    return -1;
}

bool VibeCheckPageCoordinator::EnterCard()
{
    if (card_active_ || !HasIdeas()) {
        return false;
    }
    card_active_ = true;
    action_focus_.Configure(ActionCount(), 0);
    return true;
}

bool VibeCheckPageCoordinator::EnterCardAtSelection(epaper_ui::VibeCardActionSelection selection)
{
    if (!HasIdeas()) {
        return false;
    }
    card_active_ = true;
    const int index = ActionIndexFor(selection);
    action_focus_.Configure(ActionCount(), index >= 0 ? index : 0);
    return true;
}

bool VibeCheckPageCoordinator::ExitCard()
{
    if (!card_active_) {
        return false;
    }
    card_active_ = false;
    return true;
}

bool VibeCheckPageCoordinator::HasIdeas() const
{
    return FindCurrentIdea() != nullptr;
}

epaper_ui::VibeCardActionSelection VibeCheckPageCoordinator::selected_action() const
{
    return card_active_ && HasIdeas() ? ActionSelectionAt(action_focus_.index())
                                      : epaper_ui::VibeCardActionSelection::kNone;
}

bool VibeCheckPageCoordinator::RandomizeIdea()
{
    if (ideas_.empty()) {
        return false;
    }
    const std::string previous = current_recording_id_;
    SelectRandomIdea(true);
    RebuildCardState();
    return !current_recording_id_.empty() && current_recording_id_ != previous;
}

bool VibeCheckPageCoordinator::RemoveIdea(const std::string& recording_id)
{
    if (recording_id.empty()) {
        return false;
    }
    const auto it = std::remove_if(ideas_.begin(), ideas_.end(),
                                   [&recording_id](const RecordingEntry& entry) {
                                       return entry.recording_id == recording_id;
                                   });
    if (it == ideas_.end()) {
        // Already gone -- typically because the archive mutation's synchronous notify
        // re-synced this page before we got here. Nothing to do, and specifically do not
        // touch the selection: it is now pointing at a different, still-valid idea.
        return false;
    }
    ideas_.erase(it, ideas_.end());
    // Only re-select when the entry we dropped was the one on screen.
    if (current_recording_id_ == recording_id) {
        current_recording_id_.clear();
        SelectRandomIdea(false);
    }
    ResetSessionIfEmpty();
    RebuildCardState();
    if (!HasIdeas()) {
        card_active_ = false;
    }
    return true;
}

epaper_ui::VibeCheckPageState VibeCheckPageCoordinator::BuildState() const
{
    epaper_ui::VibeCheckPageState state = {};
    state.navigation_focus_index = focus_.index();
    state.card = card_state_;
    state.card.focused = IsRoleFocused(NavigationItemRole::kVibeCheckPageCard) || card_active_;
    state.card.footer.selected_action = selected_action();
    state.progress = progress_state_;
    state.message_text = kMessageText;
    return state;
}

void VibeCheckPageCoordinator::LoadIdeas(const std::vector<RecordingEntry>& entries,
                                         bool reset_session)
{
    const std::string previous = current_recording_id_;
    ideas_.clear();
    ideas_.reserve(entries.size());
    for (const RecordingEntry& entry : entries) {
        if (IsIdeaCandidate(entry)) {
            ideas_.push_back(entry);
        }
    }

    if (reset_session) {
        initial_idea_count_ = ideas_.size();
        session_initialized_ = true;
    } else if (ideas_.size() > initial_idea_count_) {
        initial_idea_count_ = ideas_.size();
    }

    current_recording_id_.clear();
    if (!previous.empty()) {
        for (const RecordingEntry& entry : ideas_) {
            if (entry.recording_id == previous) {
                current_recording_id_ = previous;
                break;
            }
        }
    }
    if (current_recording_id_.empty()) {
        SelectRandomIdea(false);
    }
    ResetSessionIfEmpty();
    RebuildCardState();
    if (!HasIdeas()) {
        card_active_ = false;
    }
}

void VibeCheckPageCoordinator::RebuildCardState()
{
    card_state_ = {};
    card_state_.empty_state_icon_asset = &epaper_icons::kIdea;
    card_state_.empty_state_message = kEmptyStateMessage;

    const RecordingEntry* entry = FindCurrentIdea();
    if (entry == nullptr) {
        card_state_.empty = true;
        progress_state_.label_text = "Your ideas";
        progress_state_.status_text = "0/0 ideas";
        progress_state_.progress_percent = 0;
        return;
    }

    card_state_.tag_text = timeline_format::FormatDateLabel(entry->metadata.created_local_date);
    // Audio-only ideas (no transcript yet) offer the right-edge Transcribe (Star) action.
    card_state_.show_transcribe_action = !entry->metadata.has_transcript;
    card_state_.header.icon_asset =
        entry->metadata.has_transcript ? &epaper_icons::kTranscribe : &epaper_icons::kAudio;
    card_state_.header.time_text = timeline_format::FormatTimeLabel(
        entry->metadata.time_valid, entry->metadata.created_unix_seconds);
    card_state_.header.minute_seconds_text =
        timeline_format::FormatDurationLabel(entry->metadata.duration_ms);
    card_state_.header.tag_text = timeline_format::TagText(entry->metadata.tag);
    const std::string transcript = timeline_format::TrimTranscript(entry->transcript_text);
    card_state_.body_text =
        entry->metadata.has_transcript && !transcript.empty() ? transcript : kAudioOnlyMessage;

    const size_t remaining = ideas_.size();
    progress_state_.label_text = "Your ideas";
    progress_state_.status_text =
        remaining == 0 ? "0/0 ideas"
                       : std::to_string(remaining) + "/" + std::to_string(initial_idea_count_) +
                             " ideas";
    progress_state_.progress_percent =
        initial_idea_count_ == 0 ? 0
                                 : static_cast<int>((remaining * 100U) / initial_idea_count_);
}

void VibeCheckPageCoordinator::ResetSessionIfEmpty()
{
    if (!ideas_.empty()) {
        return;
    }
    current_recording_id_.clear();
    initial_idea_count_ = 0;
    session_initialized_ = false;
}

void VibeCheckPageCoordinator::SelectRandomIdea(bool avoid_current)
{
    if (ideas_.empty()) {
        current_recording_id_.clear();
        return;
    }
    if (ideas_.size() == 1) {
        current_recording_id_ = ideas_.front().recording_id;
        return;
    }

    const std::string previous = current_recording_id_;
    size_t selected = static_cast<size_t>(esp_random()) % ideas_.size();
    if (avoid_current && !previous.empty() && ideas_[selected].recording_id == previous) {
        selected = (selected + 1) % ideas_.size();
    }
    current_recording_id_ = ideas_[selected].recording_id;
}

const recording_archive_service::RecordingEntry* VibeCheckPageCoordinator::FindCurrentIdea() const
{
    if (current_recording_id_.empty()) {
        return nullptr;
    }
    for (const RecordingEntry& entry : ideas_) {
        if (entry.recording_id == current_recording_id_) {
            return &entry;
        }
    }
    return nullptr;
}

bool VibeCheckPageCoordinator::IsIdeaCandidate(const RecordingEntry& entry)
{
    return entry.metadata.tag == RecordingTag::kIdea && !entry.metadata.follow_up &&
           !entry.metadata.follow_up_completed;
}

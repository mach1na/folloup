#include "todos_page_coordinator.h"

#include <algorithm>

#include "project_assets.h"
#include "timeline_format.h"

namespace {

using recording_archive_service::RecordingEntry;
using recording_archive_service::RecordingTag;

bool IsTodoTag(RecordingTag tag)
{
    // Todos hold Task recordings; Note + Idea live on the Notes page.
    return tag == RecordingTag::kTask;
}

int64_t EntryUnixSeconds(const RecordingEntry& entry)
{
    if (entry.metadata.created_unix_seconds > 0) {
        return entry.metadata.created_unix_seconds;
    }
    return entry.modified_unix_seconds;
}

const EmbeddedImageAsset* PinIcon()
{
    return project_assets::GetIcon(EmbeddedIconId::kPin);
}

}  // namespace

TodosPageCoordinator::TodosPageCoordinator()
    : focus_(page_navigation::NavigationItemSection::kTodosPageTimelineGroups,
             page_navigation::NavigationItemRole::kTodosPageTimelineGroup)
{
    focus_.Show({}, page_navigation::BuildTodosPageNavigationModel(0));
}

std::vector<TodosPageCoordinator::TimelineGroup> TodosPageCoordinator::BuildGroups(
    const std::vector<RecordingEntry>& recordings) const
{
    std::vector<TimelineGroup> groups;

    const bool wants_archived = view_mode_ == TodosPageViewMode::kArchived;
    std::vector<const RecordingEntry*> sorted;
    sorted.reserve(recordings.size());
    for (const RecordingEntry& entry : recordings) {
        if (IsTodoTag(entry.metadata.tag) && entry.metadata.archived == wants_archived) {
            sorted.push_back(&entry);
        }
    }
    std::sort(sorted.begin(), sorted.end(), [](const RecordingEntry* a, const RecordingEntry* b) {
        return EntryUnixSeconds(*a) > EntryUnixSeconds(*b);
    });

    for (const RecordingEntry* entry_ptr : sorted) {
        const RecordingEntry& entry = *entry_ptr;
        const std::string date_key =
            !entry.metadata.created_local_date.empty()
                ? timeline_format::DateKey(entry.metadata.created_local_date)
                : timeline_format::FormatDateLabel(entry.metadata.created_local_date);

        int group_index = -1;
        for (size_t index = 0; index < groups.size(); ++index) {
            if (groups[index].date_key == date_key) {
                group_index = static_cast<int>(index);
                break;
            }
        }
        if (group_index < 0) {
            groups.push_back(
                {date_key, timeline_format::FormatDateLabel(entry.metadata.created_local_date), {}});
            group_index = static_cast<int>(groups.size()) - 1;
        }

        const std::string transcript = timeline_format::TrimTranscript(entry.transcript_text);
        const bool has_transcription = entry.metadata.has_transcript && !transcript.empty();

        TimelineEntry timeline_entry = {};
        timeline_entry.recording_id = entry.recording_id;
        timeline_entry.recording_path = entry.recording_path;
        timeline_entry.has_audio_file = entry.has_audio_file;
        timeline_entry.follow_up = entry.metadata.follow_up;
        timeline_entry.follow_up_completed = entry.metadata.follow_up_completed;
        timeline_entry.completed = entry.metadata.completed;
        timeline_entry.archived = entry.metadata.archived;
        timeline_entry.item.header.icon_asset = project_assets::GetIcon(
            has_transcription ? EmbeddedIconId::kTranscribe : EmbeddedIconId::kAudio);
        timeline_entry.item.header.tag_icon_asset = entry.metadata.follow_up ? PinIcon() : nullptr;
        timeline_entry.item.header.time_text = timeline_format::FormatTimeLabel(entry.metadata.time_valid, entry.metadata.created_unix_seconds);
        timeline_entry.item.header.minute_seconds_text =
            timeline_format::FormatDurationLabel(entry.metadata.duration_ms);
        timeline_entry.item.header.tag_text = "Task";
        timeline_entry.item.body_text = has_transcription ? transcript : "Audio only todo.";
        timeline_entry.item.accessory.kind = epaper_ui::ListItemAccessoryKind::kCheckbox;
        timeline_entry.item.accessory.checked = entry.metadata.completed;
        groups[static_cast<size_t>(group_index)].entries.push_back(std::move(timeline_entry));
    }

    if (groups.empty()) {
        groups.push_back({"today", "Today", {}});
    }
    return groups;
}

void TodosPageCoordinator::RebuildGroupsForViewMode()
{
    std::vector<TimelineGroup> groups = BuildGroups(recordings_);
    const int group_count = static_cast<int>(groups.size());
    // Position 0 is always the segment control (it's added first in
    // BuildTodosPageNavigationModel, ahead of the variable-length group list), so this both
    // resets the page's roving focus on a fresh Show() and keeps focus parked on the segment
    // control across a live view-mode switch, where it's the only place focus_ can be right now.
    focus_.Show(std::move(groups), page_navigation::BuildTodosPageNavigationModel(group_count));
}

void TodosPageCoordinator::Show(const std::vector<RecordingEntry>& recordings)
{
    recordings_ = recordings;
    // view_mode_ is deliberately not reset here -- like Summarize's segment selection, the last
    // viewed mode persists across leaving and re-entering the screen.
    segment_control_active_ = false;
    RebuildGroupsForViewMode();
}

void TodosPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& recordings)
{
    recordings_ = recordings;
    std::vector<TimelineGroup> groups = BuildGroups(recordings_);
    const int group_count = static_cast<int>(groups.size());
    focus_.RefreshPreservingSelection(std::move(groups),
                                      page_navigation::BuildTodosPageNavigationModel(group_count));
    // Otherwise focus_ stays at the position RefreshPreservingSelection() just configured it to
    // (0), which -- unlike the old first-timeline-chip default -- is now always the segment
    // control, so a background archive-changed event never silently moves focus off of it.
}

bool TodosPageCoordinator::EnterSegmentControl()
{
    if (segment_control_active_) {
        return false;
    }
    segment_control_active_ = true;
    segment_focus_.Configure(epaper_ui::kSegmentControlDefaultSegmentCount,
                             view_mode_ == TodosPageViewMode::kActive ? 0 : 1);
    return true;
}

bool TodosPageCoordinator::ExitSegmentControl()
{
    if (!segment_control_active_) {
        return false;
    }
    segment_control_active_ = false;
    return true;
}

bool TodosPageCoordinator::MoveFocus(int delta)
{
    if (delta == 0) {
        return false;
    }
    // While a control is entered, UP/DOWN drives it (switch segment / move within the item
    // list) instead of the page's roving focus. Leaving is the app-wide DOWN double-click
    // gesture, handled by the interactions layer.
    if (segment_control_active_) {
        if (!segment_focus_.Move(delta)) {
            return false;
        }
        view_mode_ = segment_focus_.index() == 0 ? TodosPageViewMode::kActive
                                                  : TodosPageViewMode::kArchived;
        RebuildGroupsForViewMode();
        return true;
    }
    return focus_.MoveFocus(delta);
}

bool TodosPageCoordinator::SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                                                 bool follow_up_completed)
{
    TimelineEntry* entry = focus_.MutableEntry(recording_id);
    if (entry == nullptr) {
        return false;
    }
    entry->follow_up = follow_up;
    entry->follow_up_completed = follow_up_completed;
    entry->item.header.tag_icon_asset = follow_up ? PinIcon() : nullptr;
    return true;
}

bool TodosPageCoordinator::SetEntryChecked(const std::string& recording_id, bool checked)
{
    TimelineEntry* entry = focus_.MutableEntry(recording_id);
    if (entry == nullptr) {
        return false;
    }
    entry->completed = checked;
    entry->item.accessory.checked = checked;
    return true;
}

epaper_ui::TodosPageState TodosPageCoordinator::BuildState() const
{
    const bool is_archived_view = view_mode_ == TodosPageViewMode::kArchived;

    epaper_ui::TodosPageState state = {};
    state.title_text = "Todos";
    state.navigation_focus_index = focus_.focus().index();

    state.segment_control.labels = {"Current", "Archived", ""};
    state.segment_control.segment_count = epaper_ui::kSegmentControlDefaultSegmentCount;
    state.segment_control.selected_index = is_archived_view ? 1 : 0;
    state.segment_control.focused =
        IsRoleFocused(page_navigation::NavigationItemRole::kTodosPageSegmentControl) ||
        segment_control_active_;
    state.segment_control.active = segment_control_active_;

    epaper_ui::TimelineListState timeline = {};
    timeline.item_label_plural = "Todos";
    timeline.empty_state_text = is_archived_view ? "No archived todos yet" : "No todos available";
    timeline.empty_state_icon_asset = project_assets::GetIcon(EmbeddedIconId::kTaskStart);
    timeline.visible_group_index = focus_.VisibleGroupIndex();
    timeline.focused_group_index = focus_.FocusedGroupIndex();
    timeline.active_group_index = focus_.ItemListActive() ? focus_.ActiveGroupIndex() : -1;
    timeline.selected_item_index = focus_.ItemListActive() ? focus_.ItemFocusIndex() : -1;
    timeline.groups.reserve(focus_.groups().size());
    for (const TimelineGroup& group : focus_.groups()) {
        epaper_ui::TimelineGroupState group_state = {};
        group_state.label_text = group.label_text;
        group_state.items.reserve(group.entries.size());
        for (const TimelineEntry& entry : group.entries) {
            group_state.items.push_back(entry.item);
        }
        timeline.groups.push_back(std::move(group_state));
    }
    state.timeline = std::move(timeline);
    return state;
}

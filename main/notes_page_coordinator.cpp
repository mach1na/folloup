#include "notes_page_coordinator.h"

#include <algorithm>

#include "project_assets.h"
#include "timeline_format.h"

namespace {

using recording_archive_service::RecordingEntry;
using recording_archive_service::RecordingTag;

bool IsNotesTag(RecordingTag tag)
{
    // Notes groups Note + Idea; Task lives on the Todos page.
    return tag == RecordingTag::kNote || tag == RecordingTag::kIdea;
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

NotesPageCoordinator::NotesPageCoordinator()
    : focus_(page_navigation::NavigationItemSection::kNotesPageTimelineGroups,
             page_navigation::NavigationItemRole::kNotesPageTimelineGroup)
{
    focus_.Show({}, page_navigation::BuildNotesPageNavigationModel(0));
}

std::vector<NotesPageCoordinator::TimelineGroup> NotesPageCoordinator::BuildGroups(
    const std::vector<RecordingEntry>& recordings) const
{
    std::vector<TimelineGroup> groups;

    std::vector<const RecordingEntry*> sorted;
    sorted.reserve(recordings.size());
    for (const RecordingEntry& entry : recordings) {
        if (IsNotesTag(entry.metadata.tag)) {
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
        timeline_entry.item.header.icon_asset = project_assets::GetIcon(
            has_transcription ? EmbeddedIconId::kTranscribe : EmbeddedIconId::kAudio);
        timeline_entry.item.header.tag_icon_asset = entry.metadata.follow_up ? PinIcon() : nullptr;
        timeline_entry.item.header.time_text = timeline_format::FormatTimeLabel(entry.metadata.time_valid, entry.metadata.created_unix_seconds);
        timeline_entry.item.header.minute_seconds_text =
            timeline_format::FormatDurationLabel(entry.metadata.duration_ms);
        timeline_entry.item.header.tag_text = timeline_format::TagText(entry.metadata.tag);
        timeline_entry.item.body_text = has_transcription ? transcript : "Audio only note.";
        timeline_entry.item.accessory.kind = epaper_ui::ListItemAccessoryKind::kNone;
        groups[static_cast<size_t>(group_index)].entries.push_back(std::move(timeline_entry));
    }

    if (groups.empty()) {
        groups.push_back({"today", "Today", {}});
    }
    return groups;
}

void NotesPageCoordinator::Show(const std::vector<RecordingEntry>& recordings)
{
    std::vector<TimelineGroup> groups = BuildGroups(recordings);
    const int group_count = static_cast<int>(groups.size());
    focus_.Show(std::move(groups), page_navigation::BuildNotesPageNavigationModel(group_count));
}

void NotesPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& recordings)
{
    std::vector<TimelineGroup> groups = BuildGroups(recordings);
    const int group_count = static_cast<int>(groups.size());
    focus_.RefreshPreservingSelection(std::move(groups),
                                      page_navigation::BuildNotesPageNavigationModel(group_count));
}

bool NotesPageCoordinator::SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
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

epaper_ui::NotesPageState NotesPageCoordinator::BuildState() const
{
    epaper_ui::NotesPageState state = {};
    state.title_text = "Notes";
    state.navigation_focus_index = focus_.focus().index();

    epaper_ui::TimelineListState timeline = {};
    timeline.item_label_plural = "Notes";
    timeline.empty_state_text = "No notes available";
    timeline.empty_state_icon_asset = project_assets::GetIcon(EmbeddedIconId::kIdea);
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

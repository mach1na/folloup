#include "topic_entries_page_coordinator.h"

#include <algorithm>

#include "project_assets.h"
#include "timeline_format.h"

namespace {

using recording_archive_service::RecordingEntry;

int64_t EntryUnixSeconds(const RecordingEntry& entry)
{
    if (entry.metadata.created_unix_seconds > 0) {
        return entry.metadata.created_unix_seconds;
    }
    return entry.modified_unix_seconds;
}

}  // namespace

TopicEntriesPageCoordinator::TopicEntriesPageCoordinator()
    : focus_(page_navigation::NavigationItemSection::kTopicEntriesPageTimelineGroups,
             page_navigation::NavigationItemRole::kTopicEntriesTimelineGroup)
{
    focus_.Show({}, page_navigation::BuildTopicEntriesPageNavigationModel(0));
}

void TopicEntriesPageCoordinator::QueueShow(const std::string& topic_id,
                                            const std::string& topic_name)
{
    pending_topic_id_ = topic_id;
    pending_topic_name_ = topic_name;
}

std::vector<TopicEntriesPageCoordinator::TimelineGroup> TopicEntriesPageCoordinator::BuildGroups(
    const std::vector<RecordingEntry>& recordings) const
{
    std::vector<TimelineGroup> groups;
    if (topic_id_.empty()) {
        groups.push_back({"today", "Today", {}});
        return groups;
    }

    std::vector<const RecordingEntry*> sorted;
    sorted.reserve(recordings.size());
    for (const RecordingEntry& entry : recordings) {
        const std::vector<std::string>& topic_ids = entry.metadata.topic_ids;
        if (std::find(topic_ids.begin(), topic_ids.end(), topic_id_) != topic_ids.end()) {
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
        timeline_entry.item.header.icon_asset = project_assets::GetIcon(
            has_transcription ? EmbeddedIconId::kTranscribe : EmbeddedIconId::kAudio);
        timeline_entry.item.header.tag_icon_asset =
            entry.metadata.follow_up ? project_assets::GetIcon(EmbeddedIconId::kPin) : nullptr;
        timeline_entry.item.header.time_text = timeline_format::FormatTimeLabel(
            entry.metadata.time_valid, entry.metadata.created_unix_seconds);
        timeline_entry.item.header.minute_seconds_text =
            timeline_format::FormatDurationLabel(entry.metadata.duration_ms);
        // Unlike Notes (already filtered to Note+Idea) or Todos (already filtered to Task), this
        // view mixes all three tags, so the per-row tag text is what tells them apart.
        timeline_entry.item.header.tag_text = timeline_format::TagText(entry.metadata.tag);
        timeline_entry.item.body_text = has_transcription ? transcript : "Audio only.";
        timeline_entry.item.accessory.kind = epaper_ui::ListItemAccessoryKind::kNone;
        groups[static_cast<size_t>(group_index)].entries.push_back(std::move(timeline_entry));
    }

    if (groups.empty()) {
        groups.push_back({"today", "Today", {}});
    }
    return groups;
}

void TopicEntriesPageCoordinator::Show(const std::vector<RecordingEntry>& recordings)
{
    if (!pending_topic_id_.empty() || !pending_topic_name_.empty()) {
        topic_id_ = pending_topic_id_;
        topic_name_ = pending_topic_name_.empty() ? "Topic" : pending_topic_name_;
        pending_topic_id_.clear();
        pending_topic_name_.clear();
    }
    std::vector<TimelineGroup> groups = BuildGroups(recordings);
    const int group_count = static_cast<int>(groups.size());
    focus_.Show(std::move(groups), page_navigation::BuildTopicEntriesPageNavigationModel(group_count));
}

void TopicEntriesPageCoordinator::RefreshFromArchive(const std::vector<RecordingEntry>& recordings)
{
    std::vector<TimelineGroup> groups = BuildGroups(recordings);
    const int group_count = static_cast<int>(groups.size());
    focus_.RefreshPreservingSelection(
        std::move(groups), page_navigation::BuildTopicEntriesPageNavigationModel(group_count));
}

epaper_ui::TopicEntriesPageState TopicEntriesPageCoordinator::BuildState() const
{
    epaper_ui::TopicEntriesPageState state = {};
    state.title_text = topic_name_;
    state.navigation_focus_index = focus_.focus().index();

    epaper_ui::TimelineListState timeline = {};
    timeline.item_label_plural = "entries";
    timeline.empty_state_text = "No entries tagged with this topic yet";
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

    state.back_button.label_text = "Back";
    state.back_button.selected =
        IsRoleFocused(page_navigation::NavigationItemRole::kTopicEntriesBackButton);
    return state;
}

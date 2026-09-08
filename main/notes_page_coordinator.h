#ifndef NOTES_PAGE_COORDINATOR_H_
#define NOTES_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/notes_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"
#include "timeline_group_focus.h"

// Owns the Notes timeline's data + two-level focus: the top level roves date-group chips (and the
// footer), and entering a group activates a second focus level over that group's item rows. The
// focus/navigation state machine itself lives in the shared TimelineGroupFocus (composed below,
// also used by Todos/Follow-up) -- this class owns only what's genuinely Notes-specific: the
// Note+Idea tag filter and per-entry rendering in BuildGroups, and BuildState's labels.
class NotesPageCoordinator {
public:
    struct TimelineEntry {
        epaper_ui::ListItemState item = {};
        std::string recording_id = {};
        std::string recording_path = {};
        bool has_audio_file = false;
        bool follow_up = false;
        bool follow_up_completed = false;
    };
    using TimelineGroup = TimelineGroupFocus<TimelineEntry>::Group;

    NotesPageCoordinator();

    void Show(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void RefreshFromArchive(
        const std::vector<recording_archive_service::RecordingEntry>& recordings);

    bool MoveFocus(int delta) { return focus_.MoveFocus(delta); }
    bool SetFocusIndex(int index) { return focus_.SetFocusIndex(index); }
    bool EnterFocusedGroup() { return focus_.EnterFocusedGroup(); }
    bool ExitItemList() { return focus_.ExitItemList(); }
    // Touch helpers: focus a group's chip (collapsing any entered list), or enter a group's item
    // list directly at a given item row.
    bool FocusGroupChip(int group_index) { return focus_.FocusGroupChip(group_index); }
    bool EnterGroupItem(int group_index, int item_index)
    {
        return focus_.EnterGroupItem(group_index, item_index);
    }
    bool FocusRecording(const std::string& recording_id, bool activate_item_list)
    {
        return focus_.FocusRecording(recording_id, activate_item_list);
    }
    bool SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                               bool follow_up_completed);

    epaper_ui::NotesPageState BuildState() const;

    int TimelineGroupCount() const { return focus_.GroupCount(); }
    int FocusedTimelineGroupIndex() const { return focus_.FocusedGroupIndex(); }
    const TimelineEntry* SelectedEntry() const { return focus_.SelectedEntry(); }
    bool HasOnlyEmptyGroup() const { return focus_.HasOnlyEmptyGroup(); }
    bool item_list_active() const { return focus_.ItemListActive(); }
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const
    {
        return focus_.IsRoleFocused(role);
    }

    const page_navigation::NavigationModel& navigation_model() const
    {
        return focus_.navigation_model();
    }
    const page_navigation::RovingFocus& focus() const { return focus_.focus(); }

private:
    std::vector<TimelineGroup> BuildGroups(
        const std::vector<recording_archive_service::RecordingEntry>& recordings) const;

    TimelineGroupFocus<TimelineEntry> focus_;
};

#endif  // NOTES_PAGE_COORDINATOR_H_

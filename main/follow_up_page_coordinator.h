#ifndef FOLLOW_UP_PAGE_COORDINATOR_H_
#define FOLLOW_UP_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/follow_up_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"
#include "timeline_group_focus.h"

// Owns the Follow-up timeline: a cross-tag view of every recording flagged follow_up, on the same
// shared two-level focus model as Notes/Todos (composed below). Rows carry a checkbox (a
// tap-to-complete affordance); completing a follow-up clears the flag so it leaves the list.
class FollowUpPageCoordinator {
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

    FollowUpPageCoordinator();

    void Show(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void RefreshFromArchive(
        const std::vector<recording_archive_service::RecordingEntry>& recordings);

    bool MoveFocus(int delta) { return focus_.MoveFocus(delta); }
    bool SetFocusIndex(int index) { return focus_.SetFocusIndex(index); }
    bool EnterFocusedGroup() { return focus_.EnterFocusedGroup(); }
    bool ExitItemList() { return focus_.ExitItemList(); }
    bool FocusGroupChip(int group_index) { return focus_.FocusGroupChip(group_index); }
    bool EnterGroupItem(int group_index, int item_index)
    {
        return focus_.EnterGroupItem(group_index, item_index);
    }
    bool FocusRecording(const std::string& recording_id, bool activate_item_list)
    {
        return focus_.FocusRecording(recording_id, activate_item_list);
    }
    bool SetEntryChecked(const std::string& recording_id, bool checked);

    epaper_ui::FollowUpPageState BuildState() const;

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

#endif  // FOLLOW_UP_PAGE_COORDINATOR_H_

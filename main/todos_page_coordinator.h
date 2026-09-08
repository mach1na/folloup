#ifndef TODOS_PAGE_COORDINATOR_H_
#define TODOS_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/todos_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"
#include "timeline_group_focus.h"

// Owns the Todos timeline's data + focus (same model as Notes): the top level roves the segment
// control, date chips, and the footer; entering a group activates a second focus level over that
// day's todo rows. Todo rows carry a checkbox accessory reflecting completion. The date-chip/item
// focus state machine lives in the shared TimelineGroupFocus (composed below, also used by Notes/
// Follow-up); this class owns what's genuinely Todos-specific: the Task tag filter + completed/
// archived fields in BuildGroups, and the Current/Archived segment control, which is a real,
// permanent Todos-only feature layered on top rather than forced into the shared focus machine.
// Which subset of todos the page currently shows: active (default) or archived. Archived todos
// have had their audio deleted by recording_archive_service's age-based sweep (or a manual
// "Archive now") and are read via the same ScreenId::kTodos surface, switched by the segment
// control at the top of the page (same widget/pattern as the Summarize page's Notes/Todos switch).
enum class TodosPageViewMode : uint8_t { kActive, kArchived };

class TodosPageCoordinator {
public:
    struct TimelineEntry {
        epaper_ui::ListItemState item = {};
        std::string recording_id = {};
        std::string recording_path = {};
        bool has_audio_file = false;
        bool follow_up = false;
        bool follow_up_completed = false;
        bool completed = false;
        bool archived = false;
    };
    using TimelineGroup = TimelineGroupFocus<TimelineEntry>::Group;

    TodosPageCoordinator();

    void Show(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void RefreshFromArchive(
        const std::vector<recording_archive_service::RecordingEntry>& recordings);
    TodosPageViewMode view_mode() const { return view_mode_; }

    // Enter/exit the segment control (same enter-move-exit convention as Summarize's segment
    // control and this page's own item list): while active, MoveFocus drives the segment
    // selection -- which rebuilds the visible groups live -- instead of the page's roving focus.
    bool EnterSegmentControl();
    bool ExitSegmentControl();
    bool segment_control_active() const { return segment_control_active_; }

    bool MoveFocus(int delta);
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
    bool SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                               bool follow_up_completed);
    bool SetEntryChecked(const std::string& recording_id, bool checked);

    epaper_ui::TodosPageState BuildState() const;

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
    // Shared reset used by Show() (a fresh page entry) and by MoveFocus() for a live segment
    // switch. RefreshFromArchive() does NOT use this -- it has its own selection-preserving
    // restore logic (via focus_.RefreshPreservingSelection) for a reactive data refresh instead of
    // a full reset.
    void RebuildGroupsForViewMode();

    TimelineGroupFocus<TimelineEntry> focus_;
    page_navigation::RovingFocus segment_focus_{epaper_ui::kSegmentControlDefaultSegmentCount, 0};
    std::vector<recording_archive_service::RecordingEntry> recordings_ = {};
    TodosPageViewMode view_mode_ = TodosPageViewMode::kActive;
    bool segment_control_active_ = false;
};

#endif  // TODOS_PAGE_COORDINATOR_H_

#ifndef TODOS_PAGE_COORDINATOR_H_
#define TODOS_PAGE_COORDINATOR_H_

#include <string>
#include <vector>

#include "epaper_ui/todos_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "recording_archive_service.h"

// Owns the Todos timeline's data + focus (same model as Notes): the top level roves the segment
// control, date chips, and the footer; entering a group activates a second focus level over that
// day's todo rows. Todo rows carry a checkbox accessory reflecting completion.
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
    struct TimelineGroup {
        std::string date_key = {};
        std::string label_text = {};
        std::vector<TimelineEntry> entries = {};
    };

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
    bool SetFocusIndex(int index);
    bool EnterFocusedGroup();
    bool ExitItemList();
    bool FocusGroupChip(int group_index);
    bool EnterGroupItem(int group_index, int item_index);
    bool FocusRecording(const std::string& recording_id, bool activate_item_list);
    bool SetEntryFollowUpState(const std::string& recording_id, bool follow_up,
                               bool follow_up_completed);
    bool SetEntryChecked(const std::string& recording_id, bool checked);

    epaper_ui::TodosPageState BuildState() const;

    int TimelineGroupCount() const { return static_cast<int>(timeline_groups_.size()); }
    int FocusedTimelineGroupIndex() const;
    const TimelineEntry* SelectedEntry() const;
    bool HasOnlyEmptyGroup() const;
    bool item_list_active() const { return item_list_active_; }
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    void BuildGroups(const std::vector<recording_archive_service::RecordingEntry>& recordings);
    void UpdateSelectedRecordingId();
    const TimelineEntry* FindEntry(const std::string& recording_id, int* group_index,
                                   int* entry_index) const;
    // Shared reset used by Show() (a fresh page entry) and by MoveFocus() for a live segment
    // switch. RefreshFromArchive() does NOT use this -- it has its own selection-preserving
    // restore logic for a reactive data refresh instead of a full reset.
    void RebuildGroupsForViewMode();

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildTodosPageNavigationModel(0);
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    page_navigation::RovingFocus item_focus_{0, 0};
    page_navigation::RovingFocus segment_focus_{epaper_ui::kSegmentControlDefaultSegmentCount, 0};
    std::vector<recording_archive_service::RecordingEntry> recordings_ = {};
    std::vector<TimelineGroup> timeline_groups_ = {};
    TodosPageViewMode view_mode_ = TodosPageViewMode::kActive;
    bool segment_control_active_ = false;
    bool item_list_active_ = false;
    int active_group_index_ = -1;
    int visible_group_index_ = -1;
    std::string selected_recording_id_ = {};
};

#endif  // TODOS_PAGE_COORDINATOR_H_

#ifndef TIMELINE_GROUP_FOCUS_H_
#define TIMELINE_GROUP_FOCUS_H_

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"

// Shared focus/navigation state machine for every two-level "date-chip groups -> item rows"
// timeline page (Notes/Todos/Follow-up): the top level roves date-group chips (and the footer);
// entering a group activates a second focus level over that group's item rows. `Entry` just needs
// a `recording_id` member -- everything else about what an entry looks like (filtering, per-entry
// rendering fields, mutators) stays page-specific and lives in each page's own coordinator, which
// composes this class as a member rather than inheriting from it.
//
// Extracted from the three page coordinators' logic, which was confirmed byte-identical (modulo
// type/role names) by a full diff before this was written -- see docs/todo.md's "Page trio
// duplication" item for context. A page's own "another active control" state (Todos' Current/
// Archived segment control) is deliberately NOT here: it's a real, permanent, page-specific
// feature that composes alongside this class rather than being forced into it. A page's `MoveFocus`
// should check its own active-control state first and only delegate to this class's `MoveFocus`
// otherwise (see TodosPageCoordinator for the pattern).
template <typename Entry>
class TimelineGroupFocus {
public:
    struct Group {
        std::string date_key = {};
        std::string label_text = {};
        std::vector<Entry> entries = {};
    };

    // group_section/group_role identify which NavigationModel item represents a date-chip group
    // for this page (e.g. kTodosPageTimelineGroups/kTodosPageTimelineGroup). Runtime values, not
    // template parameters, since NavigationItemRole/Section are just enums with no per-page type.
    TimelineGroupFocus(page_navigation::NavigationItemSection group_section,
                       page_navigation::NavigationItemRole group_role)
        : group_section_(group_section), group_role_(group_role)
    {
    }

    // Full reset: page entry, or a live view-mode switch (e.g. Todos' segment control). The caller
    // builds both `groups` and `navigation_model` externally (its own BuildGroups()/
    // BuildXPageNavigationModel()) -- this class only cares about which navigation_model items
    // match (group_section, group_role); it doesn't care what else the model contains (e.g. Todos'
    // leading segment-control item).
    void Show(std::vector<Group> groups, page_navigation::NavigationModel navigation_model)
    {
        item_list_active_ = false;
        active_group_index_ = -1;
        selected_recording_id_.clear();
        groups_ = std::move(groups);
        navigation_model_ = std::move(navigation_model);
        focus_.Configure(navigation_model_.item_count, 0);
        item_focus_.Configure(0, 0);
        visible_group_index_ = GroupCount() > 0 ? 0 : -1;
    }

    // Selection-preserving reactive refresh (e.g. an archive-changed event): tries to restore the
    // previously active item list, or else the previously focused date group.
    void RefreshPreservingSelection(std::vector<Group> groups,
                                    page_navigation::NavigationModel navigation_model)
    {
        const bool was_active = item_list_active_;
        const std::string selected_id = selected_recording_id_;
        const int focused_group = FocusedGroupIndex();
        std::string focused_date_key;
        if (focused_group >= 0 && focused_group < GroupCount()) {
            focused_date_key = groups_[static_cast<size_t>(focused_group)].date_key;
        }

        item_list_active_ = false;
        active_group_index_ = -1;
        groups_ = std::move(groups);
        navigation_model_ = std::move(navigation_model);
        focus_.Configure(navigation_model_.item_count, 0);
        item_focus_.Configure(0, 0);
        visible_group_index_ = GroupCount() > 0 ? 0 : -1;

        if (was_active && !selected_id.empty() && FocusRecording(selected_id, true)) {
            return;
        }
        if (!focused_date_key.empty()) {
            for (size_t index = 0; index < groups_.size(); ++index) {
                if (groups_[index].date_key == focused_date_key) {
                    SetFocusIndex(static_cast<int>(index));
                    break;
                }
            }
        }
    }

    bool MoveFocus(int delta)
    {
        if (delta == 0) {
            return false;
        }
        if (item_list_active_) {
            if (!item_focus_.Move(delta)) {
                return false;
            }
            visible_group_index_ = active_group_index_;
            UpdateSelectedRecordingId();
            return true;
        }
        if (!focus_.Move(delta)) {
            return false;
        }
        const int group = FocusedGroupIndex();
        if (group >= 0) {
            visible_group_index_ = group;
        }
        return true;
    }

    bool SetFocusIndex(int index)
    {
        if (!focus_.SetIndex(index)) {
            return false;
        }
        const int group = FocusedGroupIndex();
        if (group >= 0) {
            visible_group_index_ = group;
        }
        return true;
    }

    bool EnterFocusedGroup()
    {
        if (item_list_active_) {
            return false;
        }
        const int group = FocusedGroupIndex();
        if (group < 0 || groups_[static_cast<size_t>(group)].entries.empty()) {
            return false;
        }
        item_list_active_ = true;
        active_group_index_ = group;
        item_focus_.Configure(
            static_cast<int>(groups_[static_cast<size_t>(group)].entries.size()), 0);
        visible_group_index_ = group;
        UpdateSelectedRecordingId();
        return true;
    }

    bool ExitItemList()
    {
        if (!item_list_active_) {
            return false;
        }
        item_list_active_ = false;
        active_group_index_ = -1;
        item_focus_.Configure(0, 0);
        selected_recording_id_.clear();
        return true;
    }

    bool FocusGroupChip(int group_index)
    {
        if (group_index < 0 || group_index >= GroupCount()) {
            return false;
        }
        ExitItemList();
        return SetFocusIndex(group_index);
    }

    bool EnterGroupItem(int group_index, int item_index)
    {
        if (group_index < 0 || group_index >= GroupCount()) {
            return false;
        }
        const std::vector<Entry>& entries = groups_[static_cast<size_t>(group_index)].entries;
        if (entries.empty()) {
            return false;
        }
        SetFocusIndex(group_index);
        item_list_active_ = true;
        active_group_index_ = group_index;
        const int clamped = std::clamp(item_index, 0, static_cast<int>(entries.size()) - 1);
        item_focus_.Configure(static_cast<int>(entries.size()), clamped);
        visible_group_index_ = group_index;
        UpdateSelectedRecordingId();
        return true;
    }

    bool FocusRecording(const std::string& recording_id, bool activate_item_list)
    {
        int group_index = -1;
        int entry_index = -1;
        if (FindEntry(recording_id, &group_index, &entry_index) == nullptr) {
            return false;
        }
        SetFocusIndex(group_index);
        visible_group_index_ = group_index;
        selected_recording_id_ = recording_id;
        if (activate_item_list) {
            item_list_active_ = true;
            active_group_index_ = group_index;
            item_focus_.Configure(
                static_cast<int>(groups_[static_cast<size_t>(group_index)].entries.size()),
                entry_index);
        }
        return true;
    }

    const Entry* SelectedEntry() const
    {
        if (!item_list_active_ || active_group_index_ < 0 || active_group_index_ >= GroupCount()) {
            return nullptr;
        }
        const std::vector<Entry>& entries =
            groups_[static_cast<size_t>(active_group_index_)].entries;
        const int index = item_focus_.index();
        if (index < 0 || index >= static_cast<int>(entries.size())) {
            return nullptr;
        }
        return &entries[static_cast<size_t>(index)];
    }

    // Find an entry by id for in-place field mutation (e.g. an optimistic UI update before a
    // persist call). Replaces the near-identical "loop every group/entry to find and mutate by id"
    // helper each page previously hand-rolled in its own SetEntryFollowUpState/SetEntryChecked.
    Entry* MutableEntry(const std::string& recording_id)
    {
        if (recording_id.empty()) {
            return nullptr;
        }
        for (Group& group : groups_) {
            for (Entry& entry : group.entries) {
                if (entry.recording_id == recording_id) {
                    return &entry;
                }
            }
        }
        return nullptr;
    }

    bool HasOnlyEmptyGroup() const { return groups_.size() == 1 && groups_.front().entries.empty(); }

    bool IsRoleFocused(page_navigation::NavigationItemRole role) const
    {
        return navigation_model_.IsRoleSelected(focus_.index(), role);
    }

    int GroupCount() const { return static_cast<int>(groups_.size()); }

    int FocusedGroupIndex() const
    {
        const page_navigation::NavigationItemDescriptor* item =
            navigation_model_.ItemAt(focus_.index());
        if (item == nullptr || item->section != group_section_ || item->role != group_role_) {
            return -1;
        }
        return item->item_index >= 0 && item->item_index < GroupCount() ? item->item_index : -1;
    }

    int VisibleGroupIndex() const { return visible_group_index_; }
    bool ItemListActive() const { return item_list_active_; }
    // The group whose item list is currently entered (only meaningful while ItemListActive()).
    // Deliberately the raw stored value, not derived from FocusedGroupIndex() -- entering an item
    // list never moves the group-chip-level focus_, so the two stay in sync in practice, but
    // callers that need "which group is the active item list in" should use this directly rather
    // than relying on that invariant.
    int ActiveGroupIndex() const { return active_group_index_; }
    int ItemFocusIndex() const { return item_focus_.index(); }

    const std::vector<Group>& groups() const { return groups_; }
    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    void UpdateSelectedRecordingId()
    {
        const Entry* entry = SelectedEntry();
        selected_recording_id_ = entry != nullptr ? entry->recording_id : std::string();
    }

    const Entry* FindEntry(const std::string& recording_id, int* group_index,
                           int* entry_index) const
    {
        if (recording_id.empty()) {
            return nullptr;
        }
        for (size_t g = 0; g < groups_.size(); ++g) {
            const std::vector<Entry>& entries = groups_[g].entries;
            for (size_t e = 0; e < entries.size(); ++e) {
                if (entries[e].recording_id == recording_id) {
                    if (group_index != nullptr) {
                        *group_index = static_cast<int>(g);
                    }
                    if (entry_index != nullptr) {
                        *entry_index = static_cast<int>(e);
                    }
                    return &entries[e];
                }
            }
        }
        return nullptr;
    }

    page_navigation::NavigationItemSection group_section_;
    page_navigation::NavigationItemRole group_role_;
    page_navigation::NavigationModel navigation_model_ = {};
    page_navigation::RovingFocus focus_{0, 0};
    page_navigation::RovingFocus item_focus_{0, 0};
    std::vector<Group> groups_ = {};
    bool item_list_active_ = false;
    int active_group_index_ = -1;
    int visible_group_index_ = -1;
    std::string selected_recording_id_ = {};
};

#endif  // TIMELINE_GROUP_FOCUS_H_

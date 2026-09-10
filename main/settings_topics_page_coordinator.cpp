#include "settings_topics_page_coordinator.h"

namespace {

using page_navigation::NavigationItemRole;

}  // namespace

SettingsTopicsPageCoordinator::SettingsTopicsPageCoordinator() = default;

void SettingsTopicsPageCoordinator::RebuildNavigationModel(int topic_count)
{
    navigation_model_ = page_navigation::BuildSettingsTopicsPageNavigationModel(topic_count);
}

void SettingsTopicsPageCoordinator::Show(int topic_count)
{
    RebuildNavigationModel(topic_count);
    focus_.Configure(navigation_model_.item_count, 0);
}

void SettingsTopicsPageCoordinator::Sync(int topic_count)
{
    const int previous_index = focus_.index();
    RebuildNavigationModel(topic_count);
    focus_.Configure(navigation_model_.item_count, previous_index);
}

bool SettingsTopicsPageCoordinator::MoveFocus(int delta)
{
    return focus_.Move(delta);
}

bool SettingsTopicsPageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool SettingsTopicsPageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

int SettingsTopicsPageCoordinator::FocusedTopicIndex() const
{
    const page_navigation::NavigationItemDescriptor* item = navigation_model_.ItemAt(focus_.index());
    if (item == nullptr || item->role != NavigationItemRole::kSettingsTopicsTopicRow) {
        return -1;
    }
    return item->item_index;
}

epaper_ui::SettingsTopicsPageState SettingsTopicsPageCoordinator::BuildState(
    const topic_service::Snapshot& snapshot) const
{
    epaper_ui::SettingsTopicsPageState state = {};
    state.navigation_focus_index = focus_.index();

    const int focused_index = FocusedTopicIndex();
    state.topics_list.items.reserve(snapshot.topics.size());
    for (const topic_service::Topic& topic : snapshot.topics) {
        state.topics_list.items.push_back({.label_text = topic.name});
    }
    state.topics_list.focused = focused_index >= 0;
    state.topics_list.active = focused_index >= 0;
    state.topics_list.selected_item_index =
        focused_index >= 0 ? focused_index : epaper_ui::kSelectListNoSelection;

    state.new_topic_button = {
        .label_text = "New topic",
        .selected = IsRoleFocused(NavigationItemRole::kSettingsTopicsNewTopicButton),
    };
    state.back = {
        .label_text = "Back",
        .selected = IsRoleFocused(NavigationItemRole::kSettingsTopicsBackButton),
    };
    return state;
}

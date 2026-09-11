#include "topics_browse_page_coordinator.h"

namespace {

using page_navigation::NavigationItemRole;

}  // namespace

TopicsBrowsePageCoordinator::TopicsBrowsePageCoordinator() = default;

void TopicsBrowsePageCoordinator::RebuildNavigationModel(int topic_count)
{
    navigation_model_ = page_navigation::BuildTopicsBrowsePageNavigationModel(topic_count);
}

void TopicsBrowsePageCoordinator::Show(int topic_count)
{
    RebuildNavigationModel(topic_count);
    focus_.Configure(navigation_model_.item_count, 0);
}

void TopicsBrowsePageCoordinator::Sync(int topic_count)
{
    const int previous_index = focus_.index();
    RebuildNavigationModel(topic_count);
    focus_.Configure(navigation_model_.item_count, previous_index);
}

bool TopicsBrowsePageCoordinator::MoveFocus(int delta)
{
    return focus_.Move(delta);
}

bool TopicsBrowsePageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool TopicsBrowsePageCoordinator::IsRoleFocused(NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

int TopicsBrowsePageCoordinator::FocusedTopicIndex() const
{
    const page_navigation::NavigationItemDescriptor* item = navigation_model_.ItemAt(focus_.index());
    if (item == nullptr || item->role != NavigationItemRole::kTopicsBrowseTopicRow) {
        return -1;
    }
    return item->item_index;
}

epaper_ui::TopicsBrowsePageState TopicsBrowsePageCoordinator::BuildState(
    const topic_service::Snapshot& snapshot) const
{
    epaper_ui::TopicsBrowsePageState state = {};
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
    return state;
}

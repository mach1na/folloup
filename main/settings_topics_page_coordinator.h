#ifndef SETTINGS_TOPICS_PAGE_COORDINATOR_H_
#define SETTINGS_TOPICS_PAGE_COORDINATOR_H_

#include "epaper_ui/settings_topics_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "topic_service.h"

// Each topic is its own top-level roving-focus item (like Notes' per-group items), not a
// separate "entered" sub-list -- the list is flat (unlike the day-grouped timelines), so there is
// no second focus level to model.
class SettingsTopicsPageCoordinator {
public:
    SettingsTopicsPageCoordinator();

    // (Re)builds the navigation model for the current topic count and resets focus to the top.
    // Call on page entry.
    void Show(int topic_count);
    // Rebuilds the navigation model for a changed topic count while the page stays open (e.g.
    // after a create/delete), preserving focus position where possible.
    void Sync(int topic_count);
    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;
    // Index into the topic list currently backing this page, or -1 when focus isn't on a row.
    int FocusedTopicIndex() const;

    epaper_ui::SettingsTopicsPageState BuildState(const topic_service::Snapshot& snapshot) const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    void RebuildNavigationModel(int topic_count);

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildSettingsTopicsPageNavigationModel(0);
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
};

#endif  // SETTINGS_TOPICS_PAGE_COORDINATOR_H_

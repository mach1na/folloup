#ifndef TOPICS_BROWSE_PAGE_COORDINATOR_H_
#define TOPICS_BROWSE_PAGE_COORDINATOR_H_

#include "epaper_ui/topics_browse_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "topic_service.h"

// Flat, read-only list of topics -- picking one opens a filtered day-grouped timeline
// (TopicEntriesPageCoordinator). Each topic is its own top-level roving-focus item (like Notes'
// per-group items), matching Settings > Topics' list shape but without the manage actions
// (New/Rename/Delete) -- browsing and managing are deliberately separate screens.
class TopicsBrowsePageCoordinator {
public:
    TopicsBrowsePageCoordinator();

    void Show(int topic_count);
    void Sync(int topic_count);
    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;
    // Index into the topic list currently backing this page, or -1 when focus isn't on a row.
    int FocusedTopicIndex() const;

    epaper_ui::TopicsBrowsePageState BuildState(const topic_service::Snapshot& snapshot) const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    void RebuildNavigationModel(int topic_count);

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildTopicsBrowsePageNavigationModel(0);
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
};

#endif  // TOPICS_BROWSE_PAGE_COORDINATOR_H_

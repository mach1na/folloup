#ifndef TOPIC_SUMMARY_PAGE_COORDINATOR_H_
#define TOPIC_SUMMARY_PAGE_COORDINATOR_H_

#include <string>

#include "epaper_ui/topic_summary_page.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/roving_focus.h"
#include "summary_service.h"

// Owns one topic's summary screen: focus roves scroll/back/get-summary + footer; entering the
// scroll container captures UP/DOWN. Reached only from Topic Entries' Summarize button, so Back
// always returns there -- same single-source reasoning as TopicEntriesPageCoordinator's own Back.
class TopicSummaryPageCoordinator {
public:
    TopicSummaryPageCoordinator();

    // Stash the target topic before the page is shown.
    void QueueShow(const std::string& topic_id, const std::string& topic_name);
    // Applies any pending topic id/name and resets focus/scroll for a fresh page entry.
    void Show();

    bool MoveFocus(int delta);
    bool SetFocusIndex(int index);
    bool IsRoleFocused(page_navigation::NavigationItemRole role) const;

    bool EnterScrollContainer();
    bool ExitScrollContainer();
    bool scroll_container_active() const { return scroll_container_active_; }

    const std::string& topic_id() const { return topic_id_; }
    const std::string& topic_name() const { return topic_name_; }

    epaper_ui::TopicSummaryPageState BuildState(
        bool gemini_ready, const summary_service::CacheEntrySnapshot& cache) const;

    const page_navigation::NavigationModel& navigation_model() const { return navigation_model_; }
    const page_navigation::RovingFocus& focus() const { return focus_; }

private:
    std::string BuildEmptyStateMessage(bool gemini_ready,
                                       const summary_service::CacheEntrySnapshot& cache) const;

    page_navigation::NavigationModel navigation_model_ =
        page_navigation::BuildTopicSummaryPageNavigationModel();
    page_navigation::RovingFocus focus_{navigation_model_.item_count, 0};
    bool scroll_container_active_ = false;
    int scroll_position_percent_ = 0;
    std::string pending_topic_id_ = {};
    std::string pending_topic_name_ = {};
    std::string topic_id_ = {};
    std::string topic_name_ = "Topic";
};

#endif  // TOPIC_SUMMARY_PAGE_COORDINATOR_H_

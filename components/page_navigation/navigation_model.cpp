#include "page_navigation/navigation_model.h"

namespace page_navigation {
namespace {

void AddItem(NavigationModel& model,
             NavigationItemSection section,
             NavigationItemRole role,
             int item_index)
{
    model.items.push_back({
        .section = section,
        .role = role,
        .item_index = item_index,
    });
    model.item_count = static_cast<int>(model.items.size());
}

// Single source of truth for which footer roles are reachable by roving focus on a page, mirroring
// main/app_shell.cpp's FooterLayoutForScreen visibility rule: the Home/Dashboard screen shows
// every other footer icon (no need for a "go Home" icon when already there); every other screen
// shows only Home (Mic stays reachable too, but it was never part of roving focus/NavigationModel
// to begin with -- see footer_runtime). Omitting a role here, rather than merely hiding it in the
// footer's visual LayoutState, keeps UP/DOWN roving focus from landing on an icon the user can't
// see: IndexOfRole/FooterSelectedIndexForFocus/FocusFooterItem all already tolerate an absent role.
void AddFooterItems(NavigationModel& model, bool is_home_screen)
{
    if (is_home_screen) {
        // WiFi and Time no longer have footer icons of their own (reached from the Settings hub
        // instead) -- kFooterWifi/kFooterTime are deliberately not added here, matching
        // FooterLayoutForScreen's show_wifi/show_time now being permanently false. Leaving them
        // in would have been exactly the bug this function's own comment above warns about:
        // roving focus landing on an icon the user can't see.
        AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterSettings, 1);
        AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterSticky, 4);
    } else {
        AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterHome, 0);
    }
}

}  // namespace

const NavigationItemDescriptor* NavigationModel::ItemAt(int index) const
{
    if (index < 0 || index >= item_count) {
        return nullptr;
    }
    return &items[static_cast<size_t>(index)];
}

int NavigationModel::IndexOfRole(NavigationItemRole role) const
{
    for (int index = 0; index < item_count; ++index) {
        if (items[static_cast<size_t>(index)].role == role) {
            return index;
        }
    }
    return -1;
}

bool NavigationModel::IsRoleSelected(int selected_index, NavigationItemRole role) const
{
    const NavigationItemDescriptor* item = ItemAt(selected_index);
    return item != nullptr && item->role == role;
}

NavigationModel BuildSettingsPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kSettings;

    // Settings is a hub: 4 headings that navigate to a dedicated sub-page, plus Manual (replay
    // onboarding) as a direct action rather than a heading -- it's a one-shot action, not
    // something to configure.
    AddItem(model, NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsMenuNetwork, 0);
    AddItem(model, NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsMenuTime, 1);
    AddItem(model, NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsMenuStorage, 2);
    AddItem(model, NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsMenuTodos, 3);
    AddItem(model, NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsMenuTopics, 4);
    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsManualOnboardingButton,
            5);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildWifiPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kWifi;

    // WiFi is reached from the Settings hub's "Network" heading now, not its own footer icon --
    // the WiFi-enable/Access-Point toggles moved here from the old flat Settings page, and a Back
    // button (below) returns to the hub.
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageWifiToggle,
            0);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageEnableApToggle,
            1);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageNetworkList,
            2);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPagePasswordInput,
            3);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPagePasswordVisibilityButton,
            4);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageScanButton,
            5);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageConnectButton,
            6);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageBackButton,
            7);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildTimePageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kTime;

    AddItem(model, NavigationItemSection::kTimePageControls,
            NavigationItemRole::kTimePageTimezone, 0);
    AddItem(model, NavigationItemSection::kTimePageControls, NavigationItemRole::kTimePageHour, 1);
    AddItem(model, NavigationItemSection::kTimePageControls,
            NavigationItemRole::kTimePageMinute, 2);
    AddItem(model, NavigationItemSection::kTimePageControls,
            NavigationItemRole::kTimePageMeridiem, 3);
    AddItem(model, NavigationItemSection::kTimePageControls, NavigationItemRole::kTimePageMonth, 4);
    AddItem(model, NavigationItemSection::kTimePageControls, NavigationItemRole::kTimePageDay, 5);
    AddItem(model, NavigationItemSection::kTimePageControls, NavigationItemRole::kTimePageYear, 6);
    AddItem(model, NavigationItemSection::kTimePageControls, NavigationItemRole::kTimePageSave, 7);
    // Reached from the Settings hub's "Time" heading now, not its own footer icon -- Back
    // returns to the hub.
    AddItem(model, NavigationItemSection::kTimePageControls,
            NavigationItemRole::kTimePageBackButton, 8);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildSettingsStoragePageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kSettingsStorage;

    AddItem(model, NavigationItemSection::kSettingsStoragePageControls,
            NavigationItemRole::kSettingsStorageEnableOtgButton, 0);
    AddItem(model, NavigationItemSection::kSettingsStoragePageControls,
            NavigationItemRole::kSettingsStorageFormatSdButton, 1);
    AddItem(model, NavigationItemSection::kSettingsStoragePageControls,
            NavigationItemRole::kSettingsStorageBackButton, 2);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildSettingsTodosPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kSettingsTodos;

    AddItem(model, NavigationItemSection::kSettingsTodosPageControls,
            NavigationItemRole::kSettingsTodosArchiveAfterInput, 0);
    AddItem(model, NavigationItemSection::kSettingsTodosPageControls,
            NavigationItemRole::kSettingsTodosBackButton, 1);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildSettingsTopicsPageNavigationModel(int topic_count)
{
    NavigationModel model = {};
    model.scope = NavigationScope::kSettingsTopics;

    const int count = topic_count > 0 ? topic_count : 0;
    for (int index = 0; index < count; ++index) {
        AddItem(model, NavigationItemSection::kSettingsTopicsPageControls,
                NavigationItemRole::kSettingsTopicsTopicRow, index);
    }
    AddItem(model, NavigationItemSection::kSettingsTopicsPageControls,
            NavigationItemRole::kSettingsTopicsNewTopicButton, 0);
    AddItem(model, NavigationItemSection::kSettingsTopicsPageControls,
            NavigationItemRole::kSettingsTopicsBackButton, 0);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildDashboardPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kDashboard;

    // Must match epaper_ui::kDashboardMenuItemCount (dashboard_page.h).
    for (int index = 0; index < 4; ++index) {
        AddItem(model, NavigationItemSection::kDashboardPageMenu,
                NavigationItemRole::kDashboardMenuItem, index);
    }
    AddFooterItems(model, /*is_home_screen=*/true);
    return model;
}

NavigationModel BuildTopicsBrowsePageNavigationModel(int topic_count)
{
    NavigationModel model = {};
    model.scope = NavigationScope::kTopicsBrowse;

    const int count = topic_count > 0 ? topic_count : 0;
    for (int index = 0; index < count; ++index) {
        AddItem(model, NavigationItemSection::kTopicsBrowsePageControls,
                NavigationItemRole::kTopicsBrowseTopicRow, index);
    }
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildTopicEntriesPageNavigationModel(int timeline_group_count,
                                                     bool with_summarize)
{
    NavigationModel model = {};
    model.scope = NavigationScope::kTopicEntries;

    const int group_count = timeline_group_count > 0 ? timeline_group_count : 0;
    for (int index = 0; index < group_count; ++index) {
        AddItem(model, NavigationItemSection::kTopicEntriesPageTimelineGroups,
                NavigationItemRole::kTopicEntriesTimelineGroup, index);
    }
    // Reached only from the Topics browse screen (picking a topic), so Back always returns there
    // -- same reasoning as Details' page-owned Back button, no source-tracking enum needed for a
    // single possible source.
    AddItem(model, NavigationItemSection::kTopicEntriesPageControls,
            NavigationItemRole::kTopicEntriesBackButton, 0);
    // Summarize opens the topic's summary screen; only offered once there's at least one entry
    // to summarize (empty topics have nothing to send to Gemini).
    if (with_summarize) {
        AddItem(model, NavigationItemSection::kTopicEntriesPageControls,
                NavigationItemRole::kTopicEntriesSummarizeButton, 1);
    }
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildTopicSummaryPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kTopicSummary;

    AddItem(model, NavigationItemSection::kTopicSummaryPageControls,
            NavigationItemRole::kTopicSummaryPageScrollContainer, 0);
    AddItem(model, NavigationItemSection::kTopicSummaryPageControls,
            NavigationItemRole::kTopicSummaryPageBackButton, 1);
    AddItem(model, NavigationItemSection::kTopicSummaryPageControls,
            NavigationItemRole::kTopicSummaryPageGetSummaryButton, 2);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildNotesPageNavigationModel(int timeline_group_count)
{
    NavigationModel model = {};
    model.scope = NavigationScope::kNotes;

    const int group_count = timeline_group_count > 0 ? timeline_group_count : 0;
    for (int index = 0; index < group_count; ++index) {
        AddItem(model, NavigationItemSection::kNotesPageTimelineGroups,
                NavigationItemRole::kNotesPageTimelineGroup, index);
    }
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildTodosPageNavigationModel(int timeline_group_count)
{
    NavigationModel model = {};
    model.scope = NavigationScope::kTodos;

    // Segment control (Current/Archived) is always item 0 -- the first focus target on page
    // entry, matching Summarize's segment-control-first ordering.
    AddItem(model, NavigationItemSection::kTodosPageControls,
            NavigationItemRole::kTodosPageSegmentControl, 0);
    const int group_count = timeline_group_count > 0 ? timeline_group_count : 0;
    for (int index = 0; index < group_count; ++index) {
        AddItem(model, NavigationItemSection::kTodosPageTimelineGroups,
                NavigationItemRole::kTodosPageTimelineGroup, index);
    }
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildFollowUpPageNavigationModel(int timeline_group_count)
{
    NavigationModel model = {};
    model.scope = NavigationScope::kFollowUp;

    const int group_count = timeline_group_count > 0 ? timeline_group_count : 0;
    for (int index = 0; index < group_count; ++index) {
        AddItem(model, NavigationItemSection::kFollowUpPageTimelineGroups,
                NavigationItemRole::kFollowUpPageTimelineGroup, index);
    }
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildOnboardingPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kOnboarding;

    // The carousel's own control row replaces the footer: Close, Prev, Next (visual left -> right).
    AddItem(model, NavigationItemSection::kOnboardingPageControls,
            NavigationItemRole::kOnboardingPageClose, 0);
    AddItem(model, NavigationItemSection::kOnboardingPageControls,
            NavigationItemRole::kOnboardingPagePrev, 1);
    AddItem(model, NavigationItemSection::kOnboardingPageControls,
            NavigationItemRole::kOnboardingPageNext, 2);
    return model;
}

NavigationModel BuildDetailsPageNavigationModel(bool with_transcribe)
{
    NavigationModel model = {};
    model.scope = NavigationScope::kDetails;

    AddItem(model, NavigationItemSection::kDetailsPageControls,
            NavigationItemRole::kDetailsPageScrollContainer, 0);
    AddItem(model, NavigationItemSection::kDetailsPageControls,
            NavigationItemRole::kDetailsPageEditTopicsButton, 1);
    AddItem(model, NavigationItemSection::kDetailsPageControls,
            NavigationItemRole::kDetailsPageBackButton, 2);
    // Audio-only recordings (no transcript yet) gain a Transcribe button to the right of Back.
    if (with_transcribe) {
        AddItem(model, NavigationItemSection::kDetailsPageControls,
                NavigationItemRole::kDetailsPageTranscribeButton, 3);
    }
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

}  // namespace page_navigation

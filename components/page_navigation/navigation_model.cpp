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
        AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterSettings, 1);
        AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterWifi, 2);
        AddItem(model, NavigationItemSection::kFooter, NavigationItemRole::kFooterTime, 3);
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

    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsWifiToggle,
            0);
    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsEnableApToggle,
            1);
    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsEnableOtgButton,
            2);
    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsFormatSdButton,
            3);
    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsManualOnboardingButton,
            4);
    AddItem(model,
            NavigationItemSection::kSettingsPageMenu,
            NavigationItemRole::kSettingsArchiveAfterInput,
            5);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildWifiPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kWifi;

    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageNetworkList,
            0);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPagePasswordInput,
            1);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPagePasswordVisibilityButton,
            2);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageScanButton,
            3);
    AddItem(model,
            NavigationItemSection::kWifiPageControls,
            NavigationItemRole::kWifiPageConnectButton,
            4);
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
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildDashboardPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kDashboard;

    for (int index = 0; index < 5; ++index) {
        AddItem(model, NavigationItemSection::kDashboardPageMenu,
                NavigationItemRole::kDashboardMenuItem, index);
    }
    AddFooterItems(model, /*is_home_screen=*/true);
    return model;
}

NavigationModel BuildVibeCheckPageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kVibeCheck;

    AddItem(model, NavigationItemSection::kVibeCheckPageControls,
            NavigationItemRole::kVibeCheckPageCard, 0);
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

NavigationModel BuildSummarizePageNavigationModel()
{
    NavigationModel model = {};
    model.scope = NavigationScope::kSummarize;

    AddItem(model, NavigationItemSection::kSummarizePageControls,
            NavigationItemRole::kSummarizePageSegmentControl, 0);
    AddItem(model, NavigationItemSection::kSummarizePageControls,
            NavigationItemRole::kSummarizePageScrollContainer, 1);
    AddItem(model, NavigationItemSection::kSummarizePageControls,
            NavigationItemRole::kSummarizePageGetSummaryButton, 2);
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
            NavigationItemRole::kDetailsPageBackButton, 1);
    // Audio-only recordings (no transcript yet) gain a Transcribe button to the right of Back.
    if (with_transcribe) {
        AddItem(model, NavigationItemSection::kDetailsPageControls,
                NavigationItemRole::kDetailsPageTranscribeButton, 2);
    }
    AddFooterItems(model, /*is_home_screen=*/false);
    return model;
}

}  // namespace page_navigation

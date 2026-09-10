#include "settings_storage_page_coordinator.h"

#include <cstdio>
#include <string>

namespace {

std::string FormatStorageBytes(uint64_t bytes)
{
    constexpr uint64_t kKilobyte = 1024ULL;
    constexpr uint64_t kMegabyte = 1024ULL * kKilobyte;
    constexpr uint64_t kGigabyte = 1024ULL * kMegabyte;

    char buffer[32] = {};
    if (bytes >= kGigabyte) {
        std::snprintf(buffer, sizeof(buffer), "%.1f GB free",
                     static_cast<double>(bytes) / static_cast<double>(kGigabyte));
    } else if (bytes >= kMegabyte) {
        std::snprintf(buffer, sizeof(buffer), "%.1f MB free",
                     static_cast<double>(bytes) / static_cast<double>(kMegabyte));
    } else if (bytes >= kKilobyte) {
        std::snprintf(buffer, sizeof(buffer), "%.1f KB free",
                     static_cast<double>(bytes) / static_cast<double>(kKilobyte));
    } else {
        std::snprintf(buffer, sizeof(buffer), "%llu B free",
                     static_cast<unsigned long long>(bytes));
    }
    return std::string(buffer);
}

}  // namespace

SettingsStoragePageCoordinator::SettingsStoragePageCoordinator() = default;

void SettingsStoragePageCoordinator::Show()
{
    focus_.Configure(navigation_model_.item_count, 0);
}

bool SettingsStoragePageCoordinator::MoveFocus(int delta)
{
    return focus_.Move(delta);
}

bool SettingsStoragePageCoordinator::SetFocusIndex(int index)
{
    return focus_.SetIndex(index);
}

bool SettingsStoragePageCoordinator::IsRoleFocused(page_navigation::NavigationItemRole role) const
{
    return navigation_model_.IsRoleSelected(focus_.index(), role);
}

epaper_ui::SettingsStoragePageState SettingsStoragePageCoordinator::BuildState(
    const storage_service::Snapshot& storage_snapshot) const
{
    storage_service::StorageStats storage_stats = {};
    const bool allow_live_storage_stats =
        !storage_service::IsWriteBusy() &&
        storage_snapshot.mode != storage_service::Mode::kFormatting;
    const bool has_storage_stats =
        allow_live_storage_stats && storage_service::GetStorageStats(&storage_stats);

    epaper_ui::SettingsStoragePageState state = {};
    state.navigation_focus_index = focus_.index();

    state.storage_status.has_sd_card =
        storage_snapshot.inserted && storage_snapshot.mounted && has_storage_stats;
    if (state.storage_status.has_sd_card) {
        state.storage_status.free_space_text = FormatStorageBytes(storage_stats.free_bytes);
        state.storage_status.used_percent = storage_stats.used_percent;
    }

    std::string_view otg_label = "Enable OTG";
    if (storage_snapshot.mode == storage_service::Mode::kUsbMounted) {
        otg_label = "Disable OTG";
    } else if (storage_snapshot.mode == storage_service::Mode::kEnteringUsbMode) {
        otg_label = "Enabling OTG";
    } else if (storage_snapshot.mode == storage_service::Mode::kExitingUsbMode) {
        otg_label = "Disabling OTG";
    }
    state.enable_otg_button = {
        .label_text = otg_label,
        .selected = IsRoleFocused(
            page_navigation::NavigationItemRole::kSettingsStorageEnableOtgButton),
    };

    std::string_view format_label = "Format SD";
    if (storage_snapshot.mode == storage_service::Mode::kFormatting ||
        (storage_snapshot.operation == storage_service::Operation::kFormatSd &&
         storage_snapshot.phase == storage_service::OperationPhase::kStarted)) {
        format_label = "Formatting SD";
    }
    state.format_sd_button = {
        .label_text = format_label,
        .selected = IsRoleFocused(
            page_navigation::NavigationItemRole::kSettingsStorageFormatSdButton),
    };

    state.back = {
        .label_text = "Back",
        .selected =
            IsRoleFocused(page_navigation::NavigationItemRole::kSettingsStorageBackButton),
    };
    return state;
}

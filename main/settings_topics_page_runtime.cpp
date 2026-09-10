#include "settings_topics_page_runtime.h"

#include <mutex>
#include <string>
#include <vector>

#include "esp_log.h"
#include "esp_timer.h"
#include "gemini_service.h"
#include "overlay_runtime.h"
#include "page_navigation/navigation_model.h"
#include "page_navigation/page_focus_projection.h"
#include "recording_service.h"
#include "recording_session_service.h"
#include "settings_topics_page_coordinator.h"
#include "topic_service.h"
#include "ui_refresh_runtime.h"

namespace settings_topics_page_runtime {
namespace {

constexpr const char* kTag = "SettingsTopicsPageRuntime";

enum class ItemAction : uint8_t {
    kRename,
    kDelete,
    kClose,
};

enum class KeyboardMode : uint8_t {
    kNone,
    kNewTopic,
    kRenameTopic,
};

enum class CaptureState : uint8_t {
    kIdle,
    kRecording,
    kTranscribing,
};

constexpr uint32_t kTopicCaptureDurationMs = 4000;

std::mutex s_mutex;
SettingsTopicsPageCoordinator s_coordinator = {};
bool s_item_actions_pending = false;
std::string s_item_actions_topic_id;
std::string s_item_actions_topic_name;
std::vector<ItemAction> s_item_actions;
std::string s_pending_delete_topic_id;
KeyboardMode s_keyboard_mode = KeyboardMode::kNone;
std::string s_rename_topic_id;
CaptureState s_capture_state = CaptureState::kIdle;
esp_timer_handle_t s_capture_timer = nullptr;

int CurrentTopicCount()
{
    return static_cast<int>(topic_service::GetSnapshot().topics.size());
}

footer_runtime::FooterFocusItem FooterItemForSelectedIndex(int selected_index)
{
    switch (selected_index) {
        case 1:
            return footer_runtime::FooterFocusItem::kSettings;
        case 2:
            return footer_runtime::FooterFocusItem::kWifi;
        case 3:
            return footer_runtime::FooterFocusItem::kTime;
        case 0:
            return footer_runtime::FooterFocusItem::kHome;
        case 4:
            return footer_runtime::FooterFocusItem::kSticky;
        default:
            return footer_runtime::FooterFocusItem::kNone;
    }
}

page_navigation::NavigationItemRole FooterRoleForFooterItem(footer_runtime::FooterFocusItem item)
{
    switch (item) {
        case footer_runtime::FooterFocusItem::kSettings:
            return page_navigation::NavigationItemRole::kFooterSettings;
        case footer_runtime::FooterFocusItem::kWifi:
            return page_navigation::NavigationItemRole::kFooterWifi;
        case footer_runtime::FooterFocusItem::kHome:
            return page_navigation::NavigationItemRole::kFooterHome;
        case footer_runtime::FooterFocusItem::kTime:
            return page_navigation::NavigationItemRole::kFooterTime;
        case footer_runtime::FooterFocusItem::kSticky:
            return page_navigation::NavigationItemRole::kFooterSticky;
        case footer_runtime::FooterFocusItem::kNone:
        case footer_runtime::FooterFocusItem::kFolder:
        case footer_runtime::FooterFocusItem::kMic:
        default:
            return page_navigation::NavigationItemRole::kUnknown;
    }
}

footer_runtime::ProjectionState BuildFooterProjectionStateLocked()
{
    const page_navigation::PageFocusProjection projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        s_coordinator.focus().index(), -1, -1);
    footer_runtime::ProjectionState state = {};
    state.focused_item = FooterItemForSelectedIndex(projection.footer_selected_index);
    return state;
}

bool FooterProjectionChangedForFocusIndexes(int old_focus_index, int new_focus_index)
{
    const page_navigation::PageFocusProjection old_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        old_focus_index, -1, -1);
    const page_navigation::PageFocusProjection new_projection = page_navigation::ProjectPageFocus(
        s_coordinator.navigation_model(), page_navigation::NavigationItemSection::kNone,
        new_focus_index, -1, -1);
    return FooterItemForSelectedIndex(old_projection.footer_selected_index) !=
           FooterItemForSelectedIndex(new_projection.footer_selected_index);
}

epaper_ui::SettingsTopicsPageState BuildStateLocked()
{
    return s_coordinator.BuildState(topic_service::GetSnapshot());
}

void KeyboardStateChanged(const epaper_ui::KeyboardState& keyboard_state,
                          epaper_ui::KeyboardIntent intent, void*)
{
    if (intent == epaper_ui::KeyboardIntent::kSubmit) {
        const std::string& text = keyboard_state.input.value_text;
        if (s_keyboard_mode == KeyboardMode::kNewTopic) {
            (void)topic_service::Create(text);
        } else if (s_keyboard_mode == KeyboardMode::kRenameTopic) {
            (void)topic_service::Rename(s_rename_topic_id, text);
        }
    }
    if (intent == epaper_ui::KeyboardIntent::kSubmit ||
        intent == epaper_ui::KeyboardIntent::kDismiss) {
        s_keyboard_mode = KeyboardMode::kNone;
        s_rename_topic_id.clear();
        // topic_service::Create/Rename fire an event on success; app_shell's handler calls
        // SyncFromTopicService, which repaints this page since it's the one on screen. No
        // separate repaint needed here even on success.
        (void)overlay_runtime::DismissKeyboard();
    }
}

esp_err_t ShowRenameKeyboard(const std::string& topic_id, const std::string& current_name)
{
    epaper_ui::KeyboardState keyboard_state = {};
    keyboard_state.visible = true;
    keyboard_state.input.label_text = "Rename topic";
    keyboard_state.input.value_text = current_name;
    keyboard_state.input.focused = true;
    keyboard_state.input.active = true;
    keyboard_state.layout = epaper_ui::KeyboardLayoutKind::kLettersLower;
    keyboard_state.selected_key_index = 0;
    keyboard_state.shift_locked = false;
    s_keyboard_mode = KeyboardMode::kRenameTopic;
    s_rename_topic_id = topic_id;
    return overlay_runtime::ShowKeyboard(keyboard_state, &KeyboardStateChanged, nullptr);
}

esp_err_t ShowNewTopicKeyboard(const std::string& prefill_text)
{
    epaper_ui::KeyboardState keyboard_state = {};
    keyboard_state.visible = true;
    keyboard_state.input.label_text = "New topic";
    keyboard_state.input.placeholder_text = "Topic name";
    keyboard_state.input.value_text = prefill_text;
    keyboard_state.input.focused = true;
    keyboard_state.input.active = true;
    keyboard_state.layout = epaper_ui::KeyboardLayoutKind::kLettersLower;
    keyboard_state.selected_key_index = 0;
    keyboard_state.shift_locked = false;
    s_keyboard_mode = KeyboardMode::kNewTopic;
    s_rename_topic_id.clear();
    return overlay_runtime::ShowKeyboard(keyboard_state, &KeyboardStateChanged, nullptr);
}

std::string TrimWhitespace(const std::string& text)
{
    const size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) {
        return {};
    }
    const size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

void ShowToastText(const char* text)
{
    epaper_ui::ToastState toast = {};
    toast.visible = true;
    toast.body_text = text;
    (void)overlay_runtime::ShowToast(toast);
}

// Distills a spoken transcript ("this one's for the kitchen renovation project") into a short
// topic name via a second, cheap Gemini call -- falls back to the raw transcript if that call
// fails, since a slightly verbose name beats an empty field.
std::string ExtractTopicName(const std::string& transcript)
{
    const std::string prompt =
        "Extract a short topic or project name (2-4 words, title case, no punctuation) from "
        "this spoken phrase. Respond with only the name, nothing else.\n\n" +
        transcript;
    const gemini_service::TextResult result = gemini_service::GenerateText(prompt);
    if (result.success && !result.text.empty()) {
        return TrimWhitespace(result.text);
    }
    return TrimWhitespace(transcript);
}

// Runs on gemini_service's shared worker task (queued by CaptureTimerCallback below) --
// Transcribe/GenerateText are blocking HTTP calls and must never run on the esp_timer task.
void ProcessTopicCapture(recording_service::RecordedClipPtr clip)
{
    std::string name;
    if (clip && !clip->empty()) {
        const gemini_service::TranscriptionResult transcription =
            gemini_service::Transcribe(*clip);
        if (transcription.success && !transcription.transcript.empty()) {
            name = ExtractTopicName(transcription.transcript);
        }
    }
    clip.reset();
    recording_service::DiscardClip();

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_capture_state = CaptureState::kIdle;
    }
    (void)overlay_runtime::ClearToast();
    if (name.empty()) {
        ShowToastText("Didn't catch that -- try typing instead");
    }
    (void)ShowNewTopicKeyboard(name);
}

void CaptureTimerCallback(void*)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_capture_state != CaptureState::kRecording) {
            return;
        }
        s_capture_state = CaptureState::kTranscribing;
    }
    (void)recording_service::Finish();
    recording_service::RecordedClipPtr clip = recording_service::GetRecordedClip();
    (void)overlay_runtime::ClearToast();
    ShowToastText("Transcribing...");
    gemini_service::RunOnWorker([clip]() mutable { ProcessTopicCapture(std::move(clip)); });
}

esp_err_t EnsureCaptureTimer()
{
    if (s_capture_timer != nullptr) {
        return ESP_OK;
    }
    esp_timer_create_args_t timer_args = {};
    timer_args.callback = &CaptureTimerCallback;
    timer_args.dispatch_method = ESP_TIMER_TASK;
    timer_args.name = "topic_capture";
    timer_args.skip_unhandled_events = true;
    return esp_timer_create(&timer_args, &s_capture_timer);
}

// Shares recording_service with the app-wide press-and-hold note flow; refuse to collide with an
// in-progress take rather than fight it for the recorder (same guard Storage's OTG toggle uses).
void StartTopicCapture()
{
    const recording_session_service::Snapshot session = recording_session_service::GetSnapshot();
    if (session.phase != recording_session_service::Phase::kIdle &&
        session.phase != recording_session_service::Phase::kComplete &&
        session.phase != recording_session_service::Phase::kFailed) {
        ShowToastText("Finish the recording first");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (s_capture_state != CaptureState::kIdle) {
            return;
        }
        s_capture_state = CaptureState::kRecording;
    }

    const esp_err_t arm_err = recording_service::Arm();
    if (arm_err != ESP_OK) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_capture_state = CaptureState::kIdle;
        ESP_LOGW(kTag, "Topic voice capture arm failed: %s", esp_err_to_name(arm_err));
        return;
    }
    if (EnsureCaptureTimer() != ESP_OK) {
        (void)recording_service::Cancel();
        std::lock_guard<std::mutex> lock(s_mutex);
        s_capture_state = CaptureState::kIdle;
        ESP_LOGW(kTag, "Topic capture timer unavailable");
        return;
    }
    const esp_err_t start_err = recording_service::Start(recording_service::StartMode::kFresh);
    if (start_err != ESP_OK) {
        (void)recording_service::Cancel();
        std::lock_guard<std::mutex> lock(s_mutex);
        s_capture_state = CaptureState::kIdle;
        ESP_LOGW(kTag, "Topic voice capture start failed: %s", esp_err_to_name(start_err));
        return;
    }

    ShowToastText("Listening...");
    const esp_err_t timer_err = esp_timer_start_once(
        s_capture_timer, static_cast<uint64_t>(kTopicCaptureDurationMs) * 1000ULL);
    if (timer_err != ESP_OK) {
        ESP_LOGW(kTag, "Topic capture timer start failed: %s", esp_err_to_name(timer_err));
        (void)recording_service::Cancel();
        std::lock_guard<std::mutex> lock(s_mutex);
        s_capture_state = CaptureState::kIdle;
    }
}

}  // namespace

esp_err_t UpdateDisplayState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return display_service::SetSettingsTopicsPageState(BuildStateLocked());
}

esp_err_t UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode refresh_mode)
{
    return UpdateDisplayStateAndRequestRefresh(
        display_service::RefreshRequest{.refresh_mode = refresh_mode});
}

esp_err_t UpdateDisplayStateAndRequestRefresh(
    const display_service::RefreshRequest& refresh_request)
{
    return ui_refresh_runtime::Schedule(ui_refresh_runtime::SurfaceKey::kSettingsTopicsPage,
                                        &UpdateDisplayState, refresh_request);
}

page_actions::FocusMoveOutcome MoveFocus(int delta)
{
    page_actions::FocusMoveOutcome result = {};
    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        old_focus_index = s_coordinator.focus().index();
        result = settings_topics_page_interactions::HandleMoveFocus(s_coordinator, delta);
        if (!result.handled) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }

    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

settings_topics_page_interactions::ActivateResult ActivateFocusedItem()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return settings_topics_page_interactions::HandlePrimaryActivate(s_coordinator);
}

footer_runtime::ProjectionState BuildFooterProjectionState()
{
    std::lock_guard<std::mutex> lock(s_mutex);
    return BuildFooterProjectionStateLocked();
}

page_actions::FocusUpdateOutcome FocusFooterItem(footer_runtime::FooterFocusItem item)
{
    page_actions::FocusUpdateOutcome result = {};
    const page_navigation::NavigationItemRole role = FooterRoleForFooterItem(item);
    if (role == page_navigation::NavigationItemRole::kUnknown) {
        return result;
    }

    int old_focus_index = -1;
    int new_focus_index = -1;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int focus_index = s_coordinator.navigation_model().IndexOfRole(role);
        if (focus_index < 0) {
            return result;
        }
        old_focus_index = s_coordinator.focus().index();
        if (!s_coordinator.SetFocusIndex(focus_index)) {
            return result;
        }
        new_focus_index = s_coordinator.focus().index();
    }

    result.handled = true;
    result.apply_page_state = true;
    result.sync_footer_projection =
        FooterProjectionChangedForFocusIndexes(old_focus_index, new_focus_index);
    return result;
}

void ResetFocus()
{
    footer_runtime::ProjectionState projection = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Show(CurrentTopicCount());
        projection = BuildFooterProjectionStateLocked();
    }
    footer_runtime::SetProjectionState(projection);
}

esp_err_t SyncFromTopicService(bool request_refresh_if_active)
{
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_coordinator.Sync(CurrentTopicCount());
    }
    const bool active =
        display_service::GetCurrentScreen() == display_service::ScreenId::kSettingsTopics;
    if (request_refresh_if_active && active) {
        return UpdateDisplayStateAndRequestRefresh(display_service::RefreshMode::kPartial);
    }
    return UpdateDisplayState();
}

bool ShowItemActionsModal()
{
    epaper_ui::SelectModalState modal = {};
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        const int index = s_coordinator.FocusedTopicIndex();
        const topic_service::Snapshot snapshot = topic_service::GetSnapshot();
        if (index < 0 || index >= static_cast<int>(snapshot.topics.size())) {
            return false;
        }
        s_item_actions_topic_id = snapshot.topics[static_cast<size_t>(index)].id;
        s_item_actions_topic_name = snapshot.topics[static_cast<size_t>(index)].name;
        s_item_actions.clear();
        modal.title_text = s_item_actions_topic_name;
        modal.items.push_back({"Rename"});
        s_item_actions.push_back(ItemAction::kRename);
        modal.items.push_back({"Delete"});
        s_item_actions.push_back(ItemAction::kDelete);
        modal.items.push_back({"Close"});
        s_item_actions.push_back(ItemAction::kClose);
        modal.selected_index = 0;
        s_item_actions_pending = true;
    }
    const esp_err_t err = overlay_runtime::ShowSelectModal(modal);
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        std::lock_guard<std::mutex> lock(s_mutex);
        s_item_actions_pending = false;
        ESP_LOGW(kTag, "Show topic actions modal failed: %s", esp_err_to_name(err));
        return false;
    }
    return true;
}

bool HandleItemActionSelection(int selected_index)
{
    ItemAction action = ItemAction::kClose;
    std::string topic_id;
    std::string topic_name;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        if (!s_item_actions_pending) {
            return false;
        }
        s_item_actions_pending = false;
        topic_id = s_item_actions_topic_id;
        topic_name = s_item_actions_topic_name;
        if (selected_index < 0 || selected_index >= static_cast<int>(s_item_actions.size())) {
            return true;  // dismissed without a valid selection
        }
        action = s_item_actions[static_cast<size_t>(selected_index)];
    }
    if (topic_id.empty()) {
        return true;
    }

    switch (action) {
        case ItemAction::kRename:
            (void)ShowRenameKeyboard(topic_id, topic_name);
            break;
        case ItemAction::kDelete: {
            std::lock_guard<std::mutex> lock(s_mutex);
            s_pending_delete_topic_id = topic_id;
            const esp_err_t err = overlay_runtime::ShowTopicsModalConfirmDelete();
            if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
                ESP_LOGW(kTag, "Show topics delete-confirm modal failed: %s",
                         esp_err_to_name(err));
                s_pending_delete_topic_id.clear();
            }
            break;
        }
        case ItemAction::kClose:
        default:
            break;
    }
    return true;
}

void HandleNewTopicActivated()
{
    if (gemini_service::GetSnapshot().runtime.ready) {
        StartTopicCapture();
        return;
    }
    // No working Gemini call available (no Wi-Fi / not authenticated) -- creation stays possible
    // via the keyboard, just without the voice shortcut.
    ShowToastText("Connect to Wi-Fi to add topics by voice -- use the keyboard instead");
    (void)ShowNewTopicKeyboard({});
}

bool DeleteConfirmedTopic()
{
    std::string topic_id;
    {
        std::lock_guard<std::mutex> lock(s_mutex);
        topic_id.swap(s_pending_delete_topic_id);
    }
    if (topic_id.empty()) {
        return false;
    }
    return topic_service::Delete(topic_id);
}

}  // namespace settings_topics_page_runtime

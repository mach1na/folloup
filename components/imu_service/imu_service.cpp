#include "imu_service.h"

#include <new>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "qmi8658.h"
#include "waveshare_board.h"
#include "waveshare_board_config.h"

namespace imu_service {
namespace {

constexpr const char* kTag = "ImuService";
constexpr int kDebugSampleCount = 3;
constexpr TickType_t kDebugSampleDelay = pdMS_TO_TICKS(250);
constexpr float kMilliGToG = 1.0f / 1000.0f;

// Any-motion/no-motion thresholds, in the chip's ~31.25mg quantization steps (Qmi8658::MgToBytes).
// Any-motion: 2 steps (~63mg) per axis, OR'd across axes, 1-sample window -- reacts within one
// accelerometer period. No-motion: 1 step (~31mg) per axis, AND'd across axes (every axis must be
// quiet). The no-motion window register's timebase turned out not to follow the accelerometer's
// configured ODR the way its datasheet-style unit ("samples") suggests -- empirically, 42 measured
// ~290ms on-device, implying an effective rate around 125-150Hz regardless of ODR. kNoMotionWindowSamples
// is set to the register's max (255) to get as close as this field allows to the previous software
// classifier's 2s still window; both this and the thresholds are first-pass values pending on-device
// tuning against real motion/stillness.
constexpr float kAnyMotionThresholdMg = 63.0f;
constexpr float kNoMotionThresholdMg = 31.0f;
constexpr uint8_t kAnyMotionWindowSamples = 1;
constexpr uint8_t kNoMotionWindowSamples = 255;

i2c_master_bus_handle_t s_sensor_bus = nullptr;
Qmi8658* s_imu = nullptr;
bool s_initialized = false;

}  // namespace

esp_err_t Init()
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(kTag,
             "Initializing QMI8658: addr=0x%02X bus=%d scl=GPIO%d sda=GPIO%d int=GPIO%d",
             WAVESHARE_QMI8658_I2C_ADDR, static_cast<int>(WAVESHARE_SENSOR_I2C_PORT),
             static_cast<int>(WAVESHARE_SENSOR_I2C_SCL_PIN),
             static_cast<int>(WAVESHARE_SENSOR_I2C_SDA_PIN),
             static_cast<int>(WAVESHARE_IMU_INT_PIN));

    esp_err_t err = waveshare_board::EnsureSensorI2cBus(&s_sensor_bus);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Sensor I2C bus unavailable: %s", esp_err_to_name(err));
        return err;
    }

    // INT2 wired to the QMI8658's onboard any-motion/no-motion detector (see
    // EnableMotionDetection) -- this spins up the driver's own interrupt task. Left at the
    // default (normal-mode) accelerometer ODR: a low-power ODR was tried here and found to
    // silently break ReadTemperature() (all-1s register read), so that extra power trim is
    // left for a dedicated follow-up rather than risking it in this change.
    Qmi8658::Config config = {};
    config.interrupt2_pin = WAVESHARE_IMU_INT_PIN;
    s_imu = new (std::nothrow) Qmi8658(s_sensor_bus, WAVESHARE_QMI8658_I2C_ADDR, config);
    if (s_imu == nullptr) {
        return ESP_ERR_NO_MEM;
    }

    err = s_imu->Initialize(true);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "QMI8658 init failed: %s", esp_err_to_name(err));
        delete s_imu;
        s_imu = nullptr;
        return err;
    }

    s_initialized = true;
    ESP_LOGI(kTag, "IMU service initialized");
    return ESP_OK;
}

bool IsInitialized()
{
    return s_initialized;
}

esp_err_t EnableMotionDetection(MotionCallback on_motion, MotionCallback on_no_motion)
{
    if (!s_initialized || s_imu == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    s_imu->SetAnyMotionCallback(std::move(on_motion));
    s_imu->SetNoMotionCallback(std::move(on_no_motion));

    const uint8_t mode_ctrl = static_cast<uint8_t>(Qmi8658::MotionCtrl::kAnyMotionEnableX) |
                              static_cast<uint8_t>(Qmi8658::MotionCtrl::kAnyMotionEnableY) |
                              static_cast<uint8_t>(Qmi8658::MotionCtrl::kAnyMotionEnableZ) |
                              static_cast<uint8_t>(Qmi8658::MotionCtrl::kNoMotionEnableX) |
                              static_cast<uint8_t>(Qmi8658::MotionCtrl::kNoMotionEnableY) |
                              static_cast<uint8_t>(Qmi8658::MotionCtrl::kNoMotionEnableZ);
    esp_err_t err = s_imu->ConfigMotion(
        mode_ctrl, kAnyMotionThresholdMg, kAnyMotionThresholdMg, kAnyMotionThresholdMg,
        kAnyMotionWindowSamples, kNoMotionThresholdMg, kNoMotionThresholdMg, kNoMotionThresholdMg,
        kNoMotionWindowSamples,
        /*significant_motion_wait_window=*/1, /*significant_motion_confirm_window=*/1);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "QMI8658 motion config failed: %s", esp_err_to_name(err));
        return err;
    }

    if (!s_imu->EnableMotionDetect(Qmi8658::IntPin::kInt2)) {
        ESP_LOGW(kTag, "QMI8658 motion detect enable failed");
        return ESP_FAIL;
    }

    ESP_LOGI(kTag,
             "Hardware motion detection enabled: any_motion=%.1fmg/axis (OR, %uw) "
             "no_motion=%.1fmg/axis (AND, %uw)",
             static_cast<double>(kAnyMotionThresholdMg), kAnyMotionWindowSamples,
             static_cast<double>(kNoMotionThresholdMg), kNoMotionWindowSamples);
    return ESP_OK;
}

esp_err_t ReadSample(ImuSample* out_sample)
{
    if (out_sample == nullptr) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_initialized || s_imu == nullptr) {
        return ESP_ERR_INVALID_STATE;
    }

    float accel_mg[3] = {};
    esp_err_t err = s_imu->ReadAcceleration(accel_mg);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Accelerometer read failed: %s", esp_err_to_name(err));
        return err;
    }

    float gyro_dps[3] = {};
    err = s_imu->ReadGyroscope(gyro_dps);
    if (err != ESP_OK) {
        ESP_LOGW(kTag, "Gyroscope read failed: %s", esp_err_to_name(err));
        return err;
    }

    float temperature_c = 0.0f;
    if (s_imu->ReadTemperature(&temperature_c) != ESP_OK) {
        // Temperature is best-effort; a failure here should not drop the sample.
        temperature_c = 0.0f;
    }

    ImuSample sample = {};
    sample.temperature_c = temperature_c;
    sample.accel_x_g = accel_mg[0] * kMilliGToG;
    sample.accel_y_g = accel_mg[1] * kMilliGToG;
    sample.accel_z_g = accel_mg[2] * kMilliGToG;
    sample.gyro_x_dps = gyro_dps[0];
    sample.gyro_y_dps = gyro_dps[1];
    sample.gyro_z_dps = gyro_dps[2];

    *out_sample = sample;
    return ESP_OK;
}

void LogDebugStatus()
{
    if (!s_initialized) {
        ESP_LOGW(kTag, "IMU unavailable");
        return;
    }

    for (int i = 0; i < kDebugSampleCount; ++i) {
        vTaskDelay(kDebugSampleDelay);

        ImuSample sample = {};
        const esp_err_t err = ReadSample(&sample);
        if (err != ESP_OK) {
            ESP_LOGW(kTag, "Sample %d failed: %s", i + 1, esp_err_to_name(err));
            continue;
        }

        ESP_LOGI(kTag,
                 "Sample %d temp=%.2fC accel[g]={x=%.3f y=%.3f z=%.3f} gyro[dps]={x=%.3f y=%.3f z=%.3f}",
                 i + 1,
                 static_cast<double>(sample.temperature_c),
                 static_cast<double>(sample.accel_x_g),
                 static_cast<double>(sample.accel_y_g),
                 static_cast<double>(sample.accel_z_g),
                 static_cast<double>(sample.gyro_x_dps),
                 static_cast<double>(sample.gyro_y_dps),
                 static_cast<double>(sample.gyro_z_dps));
    }
}

}  // namespace imu_service

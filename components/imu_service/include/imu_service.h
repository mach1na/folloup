#ifndef IMU_SERVICE_H_
#define IMU_SERVICE_H_

#include <functional>

#include "esp_err.h"

namespace imu_service {

struct ImuSample {
    float temperature_c = 0.0f;
    float accel_x_g = 0.0f;
    float accel_y_g = 0.0f;
    float accel_z_g = 0.0f;
    float gyro_x_dps = 0.0f;
    float gyro_y_dps = 0.0f;
    float gyro_z_dps = 0.0f;
    unsigned all_ones_count = 0;
    unsigned read_error_count = 0;
};

using MotionCallback = std::function<void()>;

esp_err_t Init();
bool IsInitialized();
esp_err_t ReadSample(ImuSample* out_sample);
void LogDebugStatus();

// Enables the QMI8658's onboard any-motion/no-motion detection (INT2-driven, no host
// polling) and routes its edge-triggered events to the given callbacks -- on_motion fires
// once when motion starts, on_no_motion fires once after the configured quiet window
// elapses. Callbacks run on the driver's own interrupt task; keep them fast and non-blocking.
esp_err_t EnableMotionDetection(MotionCallback on_motion, MotionCallback on_no_motion);

}  // namespace imu_service

#endif  // IMU_SERVICE_H_

#include "esphome/core/log.h"
#include "tuya_binary_sensor.h"

namespace esphome {
namespace tuya_door_lock {

static const char *const TAG = "tuya_door_lock.binary_sensor";

void TuyaDoorLockBinarySensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const TuyaDoorLockDatapoint &datapoint) {
    ESP_LOGV(TAG, "MCU reported binary sensor %u is: %s", datapoint.id, ONOFF(datapoint.value_bool));
    this->publish_state(datapoint.value_bool);
  });
}

void TuyaDoorLockBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "TuyaDoorLock Binary Sensor:");
  ESP_LOGCONFIG(TAG, "  Binary Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace tuya_door_lock
}  // namespace esphome

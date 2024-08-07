#include "esphome/core/log.h"
#include "tuya_sensor.h"
#include <cinttypes>

namespace esphome {
namespace tuya_door_lock {

static const char *const TAG = "tuya_door_lock.sensor";

void TuyaDoorLockSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const TuyaDoorLockDatapoint &datapoint) {
    if (datapoint.type == TuyaDoorLockDatapointType::BOOLEAN) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %s", datapoint.id, ONOFF(datapoint.value_bool));
      this->publish_state(datapoint.value_bool);
    } else if (datapoint.type == TuyaDoorLockDatapointType::INTEGER) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %d", datapoint.id, datapoint.value_int);
      this->publish_state(datapoint.value_int);
    } else if (datapoint.type == TuyaDoorLockDatapointType::ENUM) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %u", datapoint.id, datapoint.value_enum);
      this->publish_state(datapoint.value_enum);
    } else if (datapoint.type == TuyaDoorLockDatapointType::BITMASK) {
      ESP_LOGV(TAG, "MCU reported sensor %u is: %" PRIx32, datapoint.id, datapoint.value_bitmask);
      this->publish_state(datapoint.value_bitmask);
    }
  });
}

void TuyaDoorLockSensor::dump_config() {
  LOG_SENSOR("", "TuyaDoorLock Sensor", this);
  ESP_LOGCONFIG(TAG, "  Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace tuya_door_lock
}  // namespace esphome

#include "tuya_text_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tuya_door_lock {

static const char *const TAG = "tuya_door_lock.text_sensor";

void TuyaDoorLockTextSensor::setup() {
  this->parent_->register_listener(this->sensor_id_, [this](const TuyaDoorLockDatapoint &datapoint) {
    switch (datapoint.type) {
      case TuyaDoorLockDatapointType::STRING:
        ESP_LOGD(TAG, "MCU reported text sensor %u is: %s", datapoint.id, datapoint.value_string.c_str());
        this->publish_state(datapoint.value_string);
        break;
      case TuyaDoorLockDatapointType::RAW: {
        std::string data = format_hex_pretty(datapoint.value_raw);
        ESP_LOGD(TAG, "MCU reported text sensor %u is: %s", datapoint.id, data.c_str());
        this->publish_state(data);
        break;
      }
      case TuyaDoorLockDatapointType::ENUM: {
        std::string data = to_string(datapoint.value_enum);
        ESP_LOGD(TAG, "MCU reported text sensor %u is: %s", datapoint.id, data.c_str());
        this->publish_state(data);
        break;
      }
      default:
        ESP_LOGW(TAG, "Unsupported data type for tuya_door_lock text sensor %u: %#02hhX", datapoint.id, (uint8_t) datapoint.type);
        break;
    }
  });
}

void TuyaDoorLockTextSensor::dump_config() {
  ESP_LOGCONFIG(TAG, "TuyaDoorLock Text Sensor:");
  ESP_LOGCONFIG(TAG, "  Text Sensor has datapoint ID %u", this->sensor_id_);
}

}  // namespace tuya_door_lock
}  // namespace esphome

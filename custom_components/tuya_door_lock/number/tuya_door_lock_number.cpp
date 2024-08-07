#include "esphome/core/log.h"
#include "tuya_number.h"

namespace esphome {
namespace tuya_door_lock {

static const char *const TAG = "tuya_door_lock.number";

void TuyaDoorLockNumber::setup() {
  this->parent_->register_listener(this->number_id_, [this](const TuyaDoorLockDatapoint &datapoint) {
    if (datapoint.type == TuyaDoorLockDatapointType::INTEGER) {
      ESP_LOGV(TAG, "MCU reported number %u is: %d", datapoint.id, datapoint.value_int);
      this->publish_state(datapoint.value_int / multiply_by_);
    } else if (datapoint.type == TuyaDoorLockDatapointType::ENUM) {
      ESP_LOGV(TAG, "MCU reported number %u is: %u", datapoint.id, datapoint.value_enum);
      this->publish_state(datapoint.value_enum);
    }
    this->type_ = datapoint.type;
  });
}

void TuyaDoorLockNumber::control(float value) {
  ESP_LOGV(TAG, "Setting number %u: %f", this->number_id_, value);
  if (this->type_ == TuyaDoorLockDatapointType::INTEGER) {
    int integer_value = lround(value * multiply_by_);
    this->parent_->set_integer_datapoint_value(this->number_id_, integer_value);
  } else if (this->type_ == TuyaDoorLockDatapointType::ENUM) {
    this->parent_->set_enum_datapoint_value(this->number_id_, value);
  }
  this->publish_state(value);
}

void TuyaDoorLockNumber::dump_config() {
  LOG_NUMBER("", "TuyaDoorLock Number", this);
  ESP_LOGCONFIG(TAG, "  Number has datapoint ID %u", this->number_id_);
}

}  // namespace tuya_door_lock
}  // namespace esphome

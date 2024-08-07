#include "esphome/core/log.h"

#include "automation.h"

static const char *const TAG = "tuya_door_lock.automation";

namespace esphome {
namespace tuya_door_lock {

void check_expected_datapoint(const TuyaDoorLockDatapoint &dp, TuyaDoorLockDatapointType expected) {
  if (dp.type != expected) {
    ESP_LOGW(TAG, "TuyaDoorLock sensor %u expected datapoint type %#02hhX but got %#02hhX", dp.id,
             static_cast<uint8_t>(expected), static_cast<uint8_t>(dp.type));
  }
}

TuyaDoorLockRawDatapointUpdateTrigger::TuyaDoorLockRawDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const TuyaDoorLockDatapoint &dp) {
    check_expected_datapoint(dp, TuyaDoorLockDatapointType::RAW);
    this->trigger(dp.value_raw);
  });
}

TuyaDoorLockBoolDatapointUpdateTrigger::TuyaDoorLockBoolDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const TuyaDoorLockDatapoint &dp) {
    check_expected_datapoint(dp, TuyaDoorLockDatapointType::BOOLEAN);
    this->trigger(dp.value_bool);
  });
}

TuyaDoorLockIntDatapointUpdateTrigger::TuyaDoorLockIntDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const TuyaDoorLockDatapoint &dp) {
    check_expected_datapoint(dp, TuyaDoorLockDatapointType::INTEGER);
    this->trigger(dp.value_int);
  });
}

TuyaDoorLockUIntDatapointUpdateTrigger::TuyaDoorLockUIntDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const TuyaDoorLockDatapoint &dp) {
    check_expected_datapoint(dp, TuyaDoorLockDatapointType::INTEGER);
    this->trigger(dp.value_uint);
  });
}

TuyaDoorLockStringDatapointUpdateTrigger::TuyaDoorLockStringDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const TuyaDoorLockDatapoint &dp) {
    check_expected_datapoint(dp, TuyaDoorLockDatapointType::STRING);
    this->trigger(dp.value_string);
  });
}

TuyaDoorLockEnumDatapointUpdateTrigger::TuyaDoorLockEnumDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const TuyaDoorLockDatapoint &dp) {
    check_expected_datapoint(dp, TuyaDoorLockDatapointType::ENUM);
    this->trigger(dp.value_enum);
  });
}

TuyaDoorLockBitmaskDatapointUpdateTrigger::TuyaDoorLockBitmaskDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id) {
  parent->register_listener(sensor_id, [this](const TuyaDoorLockDatapoint &dp) {
    check_expected_datapoint(dp, TuyaDoorLockDatapointType::BITMASK);
    this->trigger(dp.value_bitmask);
  });
}

}  // namespace tuya_door_lock
}  // namespace esphome

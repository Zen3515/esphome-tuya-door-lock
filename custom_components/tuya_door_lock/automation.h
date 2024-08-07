#pragma once

#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "tuya_door_lock.h"

#include <vector>

namespace esphome {
namespace tuya_door_lock {

class TuyaDoorLockDatapointUpdateTrigger : public Trigger<TuyaDoorLockDatapoint> {
 public:
  explicit TuyaDoorLockDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id) {
    parent->register_listener(sensor_id, [this](const TuyaDoorLockDatapoint &dp) { this->trigger(dp); });
  }
};

class TuyaDoorLockRawDatapointUpdateTrigger : public Trigger<std::vector<uint8_t>> {
 public:
  explicit TuyaDoorLockRawDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id);
};

class TuyaDoorLockBoolDatapointUpdateTrigger : public Trigger<bool> {
 public:
  explicit TuyaDoorLockBoolDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id);
};

class TuyaDoorLockIntDatapointUpdateTrigger : public Trigger<int> {
 public:
  explicit TuyaDoorLockIntDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id);
};

class TuyaDoorLockUIntDatapointUpdateTrigger : public Trigger<uint32_t> {
 public:
  explicit TuyaDoorLockUIntDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id);
};

class TuyaDoorLockStringDatapointUpdateTrigger : public Trigger<std::string> {
 public:
  explicit TuyaDoorLockStringDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id);
};

class TuyaDoorLockEnumDatapointUpdateTrigger : public Trigger<uint8_t> {
 public:
  explicit TuyaDoorLockEnumDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id);
};

class TuyaDoorLockBitmaskDatapointUpdateTrigger : public Trigger<uint32_t> {
 public:
  explicit TuyaDoorLockBitmaskDatapointUpdateTrigger(TuyaDoorLock *parent, uint8_t sensor_id);
};

}  // namespace tuya_door_lock
}  // namespace esphome

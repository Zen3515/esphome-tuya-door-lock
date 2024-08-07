#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya_door_lock/tuya_door_lock.h"
#include "esphome/components/text_sensor/text_sensor.h"

namespace esphome {
namespace tuya_door_lock {

class TuyaDoorLockTextSensor : public text_sensor::TextSensor, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_sensor_id(uint8_t sensor_id) { this->sensor_id_ = sensor_id; }

  void set_tuya_parent(TuyaDoorLock *parent) { this->parent_ = parent; }

 protected:
  TuyaDoorLock *parent_;
  uint8_t sensor_id_{0};
};

}  // namespace tuya_door_lock
}  // namespace esphome

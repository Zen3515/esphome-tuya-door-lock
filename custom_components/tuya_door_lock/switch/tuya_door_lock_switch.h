#pragma once

#include "esphome/core/component.h"
#include "esphome/components/tuya_door_lock/tuya_door_lock.h"
#include "esphome/components/switch/switch.h"

namespace esphome {
namespace tuya_door_lock {

class TuyaDoorLockSwitch : public switch_::Switch, public Component {
 public:
  void setup() override;
  void dump_config() override;
  void set_switch_id(uint8_t switch_id) { this->switch_id_ = switch_id; }

  void set_tuya_parent(TuyaDoorLock *parent) { this->parent_ = parent; }

 protected:
  void write_state(bool state) override;

  TuyaDoorLock *parent_;
  uint8_t switch_id_{0};
};

}  // namespace tuya_door_lock
}  // namespace esphome

#pragma once

#include <cinttypes>
#include <vector>

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/components/text/text.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"

#ifdef USE_TIME
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/time.h"
#endif

namespace esphome {
namespace tuya_door_lock {

enum class TuyaDoorLockDatapointType : uint8_t {
  RAW = 0x00,      // variable length
  BOOLEAN = 0x01,  // 1 byte (0/1)
  INTEGER = 0x02,  // 4 byte
  STRING = 0x03,   // variable length
  ENUM = 0x04,     // 1 byte
  BITMASK = 0x05,  // 1/2/4 bytes // Not supported
};

struct TuyaDoorLockDatapoint {
  uint8_t id;
  TuyaDoorLockDatapointType type;
  size_t len;
  union {
    bool value_bool;
    int value_int;
    uint32_t value_uint;
    uint8_t value_enum;
    uint32_t value_bitmask;
  };
  std::string value_string;
  std::vector<uint8_t> value_raw;
};

struct TuyaDoorLockDatapointListener {
  uint8_t datapoint_id;
  std::function<void(TuyaDoorLockDatapoint)> on_datapoint;
};

enum class TuyaDoorLockCommandType : uint8_t {
  // The one that I recieved

  PRODUCT_QUERY = 0x01,            // Exactly the same // for me is {"p":"bljvjx2nsv02dhao","v":"3.4.0"}
  WIFI_STATE = 0x02,               // FROM 0x03 The rest is matched
  DATAPOINT_REPORT = 0x05,         // From 0x07 and 0x22 // Happen everytime someone try to unlock
  DATAPOINT_RECORD_REPORT = 0x08,  // FROM 0x34 but nothing seems to match (the protocol is matched, but tuya imeplementation is diffrent) // Happen only when unlock successful
  // DATAPOINT_DELIVER = 0x05, // REPORT REAL-TIME STATUS
  // DATAPOINT_QUERY = 0x08, // REPORT STATUS OF RECORD TYPE
  LOCAL_TIME_QUERY = 0x06,  // FROM 0x1C The rest is matched
  GMT_TIME_QUERY = 0x10,    // FROM 0x0c The rest is matched # BUT esphome only implement local time so..
  REQUEST_TEMP_PASSWD_CLOUD_MULTIPLE = 0x13,
  VERIFY_DYNAMIC_PASSWORD = 0x12,  // 8 digits totp
  MODULE_SEND_COMMAND = 0x09,

  // Below is what they said on the table which seems totally wrong

  WIFI_RESET = 0x03,
  WIFI_SELECT = 0x04,
  WIFI_TEST = 0x07,
  REQUEST_WIFI_MODULE_FW_UPDATE = 0x0A,
  WIFI_RSSI = 0x0B,
  REQUEST_MCU_FW_UPDATE = 0x0c,
  START_UPDATE = 0x0D,
  TRANSMIT_UPDATE_PACKAGE = 0x0E,
  REQUEST_TEMP_PASSWD_CLOUD_SINGLE = 0x11,
  REQUEST_TEMP_PASSWD_CLOUD_SCHEDULE = 0x14,
  GET_DP_CACHE_COMMAND = 0x15,
  OFFLINE_DYNAMIC_PASSWORD = 0x16,
  REPORT_SERIAL_NUMBER_MCU = 0x17,
  POSITIONAL_NOTATION = 0x1C,
  AUTOMATIC_UPDATE = 0x21,
  NOTIFY_MODULE_RESET = 0x25,
  WIFI_TEST_2 = 0xF0,
};

enum class TuyaDoorLockInitState : uint8_t {
  INIT_LISTEN_ENABLE_PIN = 0x00,
  INIT_DONE,
};

struct TuyaDoorLockCommand {
  TuyaDoorLockCommandType cmd;
  std::vector<uint8_t> payload;
};

class TuyaDoorLock : public Component, public uart::UARTDevice {
 public:
  float get_setup_priority() const override { return setup_priority::LATE; }
  void setup() override;
  void loop() override;
  void dump_config() override;
  void register_listener(uint8_t datapoint_id, const std::function<void(TuyaDoorLockDatapoint)> &func);
  void set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void set_status_pin(InternalGPIOPin *status_pin) { this->status_pin_ = status_pin; }
  void set_en_binary_sensor(binary_sensor::BinarySensor *en_binary_sensor) { this->en_binary_sensor_ = en_binary_sensor; }
  // static void listen_enable_pin(TuyaDoorLock *arg);
  void set_totp_key(const std::string key) { this->totp_key_b32 = key; }
  void parse_totp_key();
  // void set_input_totp_text(text::Text *input_totp_text) { this->input_totp_text_ = input_totp_text; }
  void set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  void force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value);
  void force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value);
  void force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value);
  void force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value);
  void force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value);
  void force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length);
  TuyaDoorLockInitState get_init_state();
  // I add theses here to use from lambda
  std::string totp_key_b32 = "";
  uint8_t *totp_key_{nullptr};
  size_t totp_key_length_ = 0;
  // text::Text *input_totp_text_{nullptr};
#ifdef USE_TIME
  void set_time_id(time::RealTimeClock *time_id) { this->time_id_ = time_id; }
#endif
  void add_ignore_mcu_update_on_datapoints(uint8_t ignore_mcu_update_on_datapoints) {
    this->ignore_mcu_update_on_datapoints_.push_back(ignore_mcu_update_on_datapoints);
  }
  void add_on_initialized_callback(std::function<void()> callback) {
    this->initialized_callback_.add(std::move(callback));
  }

 protected:
  void handle_char_(uint8_t c);
  void handle_datapoints_(const uint8_t *buffer, size_t len);
  optional<TuyaDoorLockDatapoint> get_datapoint_(uint8_t datapoint_id);
  bool validate_message_();

  void handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len);
  void send_raw_command_(TuyaDoorLockCommand command);
  void process_command_queue_();
  void send_command_(const TuyaDoorLockCommand &command);
  void send_empty_command_(TuyaDoorLockCommandType command);
  void set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDoorLockDatapointType datapoint_type, uint32_t value,
                                    uint8_t length, bool forced);
  void set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced);
  void set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced);
  void send_datapoint_command_(uint8_t datapoint_id, TuyaDoorLockDatapointType datapoint_type, std::vector<uint8_t> data);
  void set_status_pin_();
  void send_wifi_status_();
  uint8_t get_wifi_status_code_();
  uint8_t get_wifi_rssi_();

#ifdef USE_TIME
  void send_local_time_();
  void send_gmt_time_();
  time::RealTimeClock *time_id_{nullptr};
  bool time_sync_callback_registered_{false};
#endif
  TuyaDoorLockInitState init_state_ = TuyaDoorLockInitState::INIT_LISTEN_ENABLE_PIN;
  bool init_failed_{false};
  int init_retries_{0};
  uint8_t protocol_version_ = -1;
  InternalGPIOPin *status_pin_{nullptr};
  binary_sensor::BinarySensor *en_binary_sensor_{nullptr};
  bool has_sent_wifi_status = false;
  int status_pin_reported_ = -1;
  int reset_pin_reported_ = -1;
  uint32_t last_command_timestamp_ = 0;
  uint32_t last_rx_char_timestamp_ = 0;
  std::string product_ = "";
  std::vector<TuyaDoorLockDatapointListener> listeners_;
  std::vector<TuyaDoorLockDatapoint> datapoints_;
  std::vector<uint8_t> rx_message_;
  std::vector<uint8_t> ignore_mcu_update_on_datapoints_{};
  std::vector<TuyaDoorLockCommand> command_queue_;
  optional<TuyaDoorLockCommandType> expected_response_{};
  uint8_t wifi_status_ = -1;
  CallbackManager<void()> initialized_callback_{};
};

}  // namespace tuya_door_lock
}  // namespace esphome

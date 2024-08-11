#include "tuya_door_lock.h"

#include "esphome/components/network/util.h"
#include "esphome/core/gpio.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/util.h"
#include "otp.hpp"

#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_CAPTIVE_PORTAL
#include "esphome/components/captive_portal/captive_portal.h"
#endif

namespace esphome {
namespace tuya_door_lock {

static const char *const TAG = "tuya_door_lock";
static const int COMMAND_DELAY = 10;
static const int RECEIVE_TIMEOUT = 300;
static const int MAX_RETRIES = 5;

void TuyaDoorLock::setup() {
  this->send_empty_command_(TuyaDoorLockCommandType::PRODUCT_QUERY);
  this->parse_totp_key();
  ESP_LOGD(TAG, "Finished setup");
}

void TuyaDoorLock::loop() {
  if ((this->init_state_ == TuyaDoorLockInitState::INIT_DONE) && (this->en_binary_sensor_ != nullptr)) {
    const bool is_enable = this->en_binary_sensor_->state;
    if (is_enable) {
      if (!this->has_sent_wifi_status) {
        ESP_LOGD(TAG, "Tuya module enabled, reporting cloud connection in 1.25s, 3s");
        this->set_timeout("handle_wake_1.25s", 1250, [this] {
          this->send_command_(
              TuyaDoorLockCommand{
                  .cmd = TuyaDoorLockCommandType::WIFI_STATE,
                  .payload = std::vector<uint8_t>{0x04}  // Connected with Tuya Cloud
              });
        });
        this->set_timeout("handle_wake_3s", 3000, [this] {
          this->send_command_(
              TuyaDoorLockCommand{
                  .cmd = TuyaDoorLockCommandType::WIFI_STATE,
                  .payload = std::vector<uint8_t>{0x04}  // Connected with Tuya Cloud
              });
        });
        this->has_sent_wifi_status = true;
      }
    } else {
      this->has_sent_wifi_status = false;
    }
  }
  while (this->available()) {
    uint8_t c;
    this->read_byte(&c);
    this->handle_char_(c);
  }
  process_command_queue_();
}

void TuyaDoorLock::dump_config() {
  ESP_LOGCONFIG(TAG, "TuyaDoorLock:");
  for (auto &info : this->datapoints_) {
    if (info.type == TuyaDoorLockDatapointType::RAW) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: raw (value: %s)", info.id, format_hex_pretty(info.value_raw).c_str());
    } else if (info.type == TuyaDoorLockDatapointType::BOOLEAN) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: switch (value: %s)", info.id, ONOFF(info.value_bool));
    } else if (info.type == TuyaDoorLockDatapointType::INTEGER) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: int value (value: %d)", info.id, info.value_int);
    } else if (info.type == TuyaDoorLockDatapointType::STRING) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: string value (value: %s)", info.id, info.value_string.c_str());
    } else if (info.type == TuyaDoorLockDatapointType::ENUM) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: enum (value: %d)", info.id, info.value_enum);
    } else if (info.type == TuyaDoorLockDatapointType::BITMASK) {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: bitmask (value: %" PRIx32 ")", info.id, info.value_bitmask);
    } else {
      ESP_LOGCONFIG(TAG, "  Datapoint %u: unknown", info.id);
    }
  }
  if ((this->status_pin_reported_ != -1) || (this->reset_pin_reported_ != -1)) {
    ESP_LOGCONFIG(TAG, "  GPIO Configuration: status: pin %d, reset: pin %d", this->status_pin_reported_,
                  this->reset_pin_reported_);
  }
  LOG_PIN("  Status Pin: ", this->status_pin_);
  LOG_BINARY_SENSOR("", "  EN Sensor: ", this->en_binary_sensor_);
  ESP_LOGCONFIG(TAG, "  Product: '%s'", this->product_.c_str());
  if (this->totp_key_length_ > 0) {
    ESP_LOGCONFIG(TAG, "  totp: enabled");
  } else {
    ESP_LOGCONFIG(TAG, "  totp: disabled");
    this->parse_totp_key();  // I leave it here in case someone else needs to know why the tot parsing failed
  }
}

bool TuyaDoorLock::validate_message_() {
  uint32_t at = this->rx_message_.size() - 1;
  auto *data = &this->rx_message_[0];
  uint8_t new_byte = data[at];

  // Byte 0: HEADER1 (always 0x55)
  if (at == 0)
    return new_byte == 0x55;
  // Byte 1: HEADER2 (always 0xAA)
  if (at == 1)
    return new_byte == 0xAA;

  // Byte 2: VERSION
  // no validation for the following fields:
  uint8_t version = data[2];
  if (at == 2)
    return true;
  // Byte 3: COMMAND
  uint8_t command = data[3];
  if (at == 3)
    return true;

  // Byte 4: LENGTH1
  // Byte 5: LENGTH2
  if (at <= 5) {
    // no validation for these fields
    return true;
  }

  uint16_t length = (uint16_t(data[4]) << 8) | (uint16_t(data[5]));

  // wait until all data is read
  if (at - 6 < length)
    return true;

  // Byte 6+LEN: CHECKSUM - sum of all bytes (including header) modulo 256
  uint8_t rx_checksum = new_byte;
  uint8_t calc_checksum = 0;
  for (uint32_t i = 0; i < 6 + length; i++)
    calc_checksum += data[i];

  if (rx_checksum != calc_checksum) {
    ESP_LOGW(TAG, "TuyaDoorLock Received invalid message checksum %02X!=%02X", rx_checksum, calc_checksum);
    return false;
  }

  // valid message
  const uint8_t *message_data = data + 6;
  ESP_LOGV(TAG, "Received TuyaDoorLock: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u", command, version,
           format_hex_pretty(message_data, length).c_str(), static_cast<uint8_t>(this->init_state_));
  this->handle_command_(command, version, message_data, length);

  // return false to reset rx buffer
  return false;
}

void TuyaDoorLock::handle_char_(uint8_t c) {
  this->rx_message_.push_back(c);
  if (!this->validate_message_()) {
    this->rx_message_.clear();
  } else {
    this->last_rx_char_timestamp_ = millis();
  }
}

void TuyaDoorLock::handle_command_(uint8_t command, uint8_t version, const uint8_t *buffer, size_t len) {
  TuyaDoorLockCommandType command_type = (TuyaDoorLockCommandType)command;

  if (this->expected_response_.has_value() && this->expected_response_ == command_type) {
    this->expected_response_.reset();
    this->command_queue_.erase(command_queue_.begin());
    this->init_retries_ = 0;
  }

  switch (command_type) {
    case TuyaDoorLockCommandType::PRODUCT_QUERY: {
      ESP_LOGD(TAG, "PRODUCT_QUERY (0x%02X)", command);
      // check it is a valid string made up of printable characters
      bool valid = true;
      for (size_t i = 0; i < len; i++) {
        if (!std::isprint(buffer[i])) {
          valid = false;
          break;
        }
      }
      if (valid) {
        this->product_ = std::string(reinterpret_cast<const char *>(buffer), len);
      } else {
        this->product_ = R"({"p":"INVALID"})";
      }
      if (this->init_state_ == TuyaDoorLockInitState::INIT_LISTEN_ENABLE_PIN) {
        this->init_state_ = TuyaDoorLockInitState::INIT_DONE;
      }
      break;
    }
    case TuyaDoorLockCommandType::WIFI_STATE:
      ESP_LOGD(TAG, "WIFI_STATE handled, expected: 55 AA 00 02 00 00 01");
      break;
    case TuyaDoorLockCommandType::WIFI_RESET:
      ESP_LOGE(TAG, "WIFI_RESET is not handled");
      break;
    case TuyaDoorLockCommandType::WIFI_SELECT:
      ESP_LOGE(TAG, "WIFI_SELECT is not handled");
      break;
    case TuyaDoorLockCommandType::DATAPOINT_REPORT:
      // This case happen every unlock attempt
      ESP_LOGD(TAG, "DATAPOINT_REPORT (0x%02X)", command);
      // We can just acknowledge before process, right? This is required for the request remote unlock
      this->send_command_(
          TuyaDoorLockCommand{
              .cmd = TuyaDoorLockCommandType::DATAPOINT_REPORT,
              .payload = std::vector<uint8_t>{0x00}  // Reporting succeeded
          });
      this->handle_datapoints_(buffer, len);
      break;
    case TuyaDoorLockCommandType::DATAPOINT_RECORD_REPORT:
      // This case happen every sucessful unlock
      ESP_LOGD(TAG, "DATAPOINT_RECORD_REPORT (0x%02X)", command);
      {
        // 02   00      01  01  01  04  00  01 02 00 04 00 00 00 03 0B 04 00 01 01
        // GMT  YY+2000 MM  DD  HH  MM  SS  .................DATA.................
        const uint8_t *report_data = buffer + 7;
        this->handle_datapoints_(report_data, len - 7);
        this->send_command_(
            TuyaDoorLockCommand{
                .cmd = TuyaDoorLockCommandType::DATAPOINT_RECORD_REPORT,
                .payload = std::vector<uint8_t>{0x00}  // Reporting succeeded
            });
      }
      break;
    case TuyaDoorLockCommandType::MODULE_SEND_COMMAND:
      break;
    case TuyaDoorLockCommandType::WIFI_TEST:
      ESP_LOGD(TAG, "WIFI_TEST (0x%02X)", command);
      this->send_command_(TuyaDoorLockCommand{.cmd = TuyaDoorLockCommandType::WIFI_TEST, .payload = std::vector<uint8_t>{0x00, 0x00}});
      break;
    case TuyaDoorLockCommandType::WIFI_RSSI:
      ESP_LOGD(TAG, "WIFI_RSSI (0x%02X)", command);
      this->send_command_(
          TuyaDoorLockCommand{.cmd = TuyaDoorLockCommandType::WIFI_RSSI, .payload = std::vector<uint8_t>{get_wifi_rssi_()}});
      break;
    case TuyaDoorLockCommandType::REQUEST_TEMP_PASSWD_CLOUD_MULTIPLE:
      ESP_LOGD(TAG, "REQUEST_TEMP_PASSWD_CLOUD_MULTIPLE (0x%02X)", command);
      // #ifdef USE_TIME
      //       if (this->time_id_ != nullptr && this->totp_key_length_ > 0 && this->totp_key_ != nullptr) {
      //         // Generate totp password
      //         ESPTime now = this->time_id_->now();
      //         if (now.is_valid()) {
      //           // Convert the current time to a timestamp (in seconds since the epoch)
      //           auto timestamp = (time_t)floor(now.timestamp / 30.0);  // Use the same 30-second time window
      //           const uint32_t generated_password = otp::totp_hash_token(this->totp_key_, this->totp_key_length_, timestamp, 6);
      //           ESP_LOGD(TAG, "Generated TOTP password: %u", generated_password);
      //         } else {
      //           ESP_LOGW(TAG, "Current time is invalid, cannot generate TOTP password.");
      //         }
      //       } else
      // #endif
      //       {
      //         ESP_LOGW(TAG, "REQUEST_TEMP_PASSWD_CLOUD_MULTIPLE is not handled because time is not configured and/or totp key was incorrect");
      //       }
      ESP_LOGD(TAG, "REQUEST_TEMP_PASSWD_CLOUD_MULTIPLE is not handled, I don't want this feature");
      break;
    case TuyaDoorLockCommandType::VERIFY_DYNAMIC_PASSWORD:
      ESP_LOGD(TAG, "VERIFY_DYNAMIC_PASSWORD (0x%02X)", command);
      ESP_LOGV(TAG, "Input password was: %.*s", 8, reinterpret_cast<const char *>(&buffer[6]));
#ifdef USE_TIME
      if (this->time_id_ != nullptr && this->totp_key_length_ > 0 && this->totp_key_ != nullptr) {
        // Generate totp password
        ESPTime now = this->time_id_->now();
        char generated_password_str[9];
        if (now.is_valid()) {
          auto timestamp = (time_t)floor(now.timestamp / 300.0);  // time windows of 5 min
          const uint32_t generated_password = otp::totp_hash_token(this->totp_key_, totp_key_length_, timestamp, 8);
          snprintf(generated_password_str, sizeof(generated_password_str), "%08u", generated_password);
          ESP_LOGVV(TAG, "Generated TOTP len=8 password: %u", generated_password);
        } else {
          ESP_LOGW(TAG, "Current time is invalid, cannot generate TOTP password.");
          break;
        }
        if (memcmp(generated_password_str, (char *)(buffer + 6), 8) == 0) {
          ESP_LOGD(TAG, "Password matched");
          this->send_command_(
              TuyaDoorLockCommand{
                  .cmd = TuyaDoorLockCommandType::VERIFY_DYNAMIC_PASSWORD,
                  .payload = std::vector<uint8_t>{0x00}});
        } else {
          ESP_LOGD(TAG, "Password not matched");
          this->send_command_(
              TuyaDoorLockCommand{
                  .cmd = TuyaDoorLockCommandType::VERIFY_DYNAMIC_PASSWORD,
                  .payload = std::vector<uint8_t>{0x01}});
        }
      } else
#endif
      {
        ESP_LOGW(TAG, "VERIFY_DYNAMIC_PASSWORD is not handled because time is not configured and/or totp key was incorrect");
      }
      break;
    case TuyaDoorLockCommandType::LOCAL_TIME_QUERY:
      ESP_LOGD(TAG, "LOCAL_TIME_QUERY (0x%02X)", command);
#ifdef USE_TIME
      if (this->time_id_ != nullptr) {
        this->send_local_time_();

        if (!this->time_sync_callback_registered_) {
          // tuya_door_lock mcu supports time, so we let them know when our time changed
          this->time_id_->add_on_time_sync_callback([this] { this->send_local_time_(); });
          this->time_sync_callback_registered_ = true;
        }
      } else
#endif
      {
        ESP_LOGW(TAG, "LOCAL_TIME_QUERY is not handled because time is not configured");
      }
      break;
    case TuyaDoorLockCommandType::GMT_TIME_QUERY:
      ESP_LOGD(TAG, "GMT_TIME_QUERY (0x%02X)", command);
#ifdef USE_TIME
      if (this->time_id_ != nullptr) {
        this->send_gmt_time_();

        if (!this->time_sync_callback_registered_) {
          // tuya_door_lock mcu supports time, so we let them know when our time changed
          this->time_id_->add_on_time_sync_callback([this] { this->send_gmt_time_(); });
          this->time_sync_callback_registered_ = true;
        }
      } else
#endif
      {
        ESP_LOGW(TAG, "GMT_TIME_QUERY is not handled because time is not configured");
      }
      break;
    default:
      ESP_LOGE(TAG, "Invalid command (0x%02X) received", command);
  }
}

void TuyaDoorLock::handle_datapoints_(const uint8_t *buffer, size_t len) {
  while (len >= 4) {
    TuyaDoorLockDatapoint datapoint{};
    datapoint.id = buffer[0];
    datapoint.type = (TuyaDoorLockDatapointType)buffer[1];
    datapoint.value_uint = 0;

    size_t data_size = (buffer[2] << 8) + buffer[3];
    const uint8_t *data = buffer + 4;
    size_t data_len = len - 4;
    if (data_size > data_len) {
      ESP_LOGW(TAG, "Datapoint %u is truncated and cannot be parsed (%zu > %zu)", datapoint.id, data_size, data_len);
      return;
    }

    datapoint.len = data_size;

    switch (datapoint.type) {
      case TuyaDoorLockDatapointType::RAW:
        datapoint.value_raw = std::vector<uint8_t>(data, data + data_size);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, format_hex_pretty(datapoint.value_raw).c_str());
        break;
      case TuyaDoorLockDatapointType::BOOLEAN:
        if (data_size != 1) {
          ESP_LOGW(TAG, "Datapoint %u has bad boolean len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_bool = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, ONOFF(datapoint.value_bool));
        break;
      case TuyaDoorLockDatapointType::INTEGER:
        if (data_size != 4) {
          ESP_LOGW(TAG, "Datapoint %u has bad integer len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_uint = encode_uint32(data[0], data[1], data[2], data[3]);
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_int);
        break;
      case TuyaDoorLockDatapointType::STRING:
        datapoint.value_string = std::string(reinterpret_cast<const char *>(data), data_size);
        ESP_LOGD(TAG, "Datapoint %u update to %s", datapoint.id, datapoint.value_string.c_str());
        break;
      case TuyaDoorLockDatapointType::ENUM:
        if (data_size != 1) {
          ESP_LOGW(TAG, "Datapoint %u has bad enum len %zu", datapoint.id, data_size);
          return;
        }
        datapoint.value_enum = data[0];
        ESP_LOGD(TAG, "Datapoint %u update to %d", datapoint.id, datapoint.value_enum);
        break;
      case TuyaDoorLockDatapointType::BITMASK:
        switch (data_size) {
          case 1:
            datapoint.value_bitmask = encode_uint32(0, 0, 0, data[0]);
            break;
          case 2:
            datapoint.value_bitmask = encode_uint32(0, 0, data[0], data[1]);
            break;
          case 4:
            datapoint.value_bitmask = encode_uint32(data[0], data[1], data[2], data[3]);
            break;
          default:
            ESP_LOGW(TAG, "Datapoint %u has bad bitmask len %zu", datapoint.id, data_size);
            return;
        }
        ESP_LOGD(TAG, "Datapoint %u update to %#08" PRIX32, datapoint.id, datapoint.value_bitmask);
        break;
      default:
        ESP_LOGW(TAG, "Datapoint %u has unknown type %#02hhX", datapoint.id, static_cast<uint8_t>(datapoint.type));
        return;
    }

    len -= data_size + 4;
    buffer = data + data_size;

    // drop update if datapoint is in ignore_mcu_datapoint_update list
    bool skip = false;
    for (auto i : this->ignore_mcu_update_on_datapoints_) {
      if (datapoint.id == i) {
        ESP_LOGV(TAG, "Datapoint %u found in ignore_mcu_update_on_datapoints list, dropping MCU update", datapoint.id);
        skip = true;
        break;
      }
    }
    if (skip)
      continue;

    // Update internal datapoints
    bool found = false;
    for (auto &other : this->datapoints_) {
      if (other.id == datapoint.id) {
        other = datapoint;
        found = true;
      }
    }
    if (!found) {
      this->datapoints_.push_back(datapoint);
    }

    // Run through listeners
    for (auto &listener : this->listeners_) {
      if (listener.datapoint_id == datapoint.id)
        listener.on_datapoint(datapoint);
    }
  }
}

void TuyaDoorLock::send_raw_command_(TuyaDoorLockCommand command) {
  uint8_t len_hi = (uint8_t)(command.payload.size() >> 8);
  uint8_t len_lo = (uint8_t)(command.payload.size() & 0xFF);
  uint8_t version = 0;

  this->last_command_timestamp_ = millis();
  switch (command.cmd) {
    case TuyaDoorLockCommandType::PRODUCT_QUERY:
      this->expected_response_ = TuyaDoorLockCommandType::PRODUCT_QUERY;
      break;
    case TuyaDoorLockCommandType::MODULE_SEND_COMMAND:
      break;
    case TuyaDoorLockCommandType::WIFI_STATE:
      break;
    case TuyaDoorLockCommandType::DATAPOINT_REPORT:
      break;
    case TuyaDoorLockCommandType::DATAPOINT_RECORD_REPORT:
      break;
    case TuyaDoorLockCommandType::LOCAL_TIME_QUERY:
      break;
    case TuyaDoorLockCommandType::GMT_TIME_QUERY:
      break;
    case TuyaDoorLockCommandType::VERIFY_DYNAMIC_PASSWORD:
      break;
    default:
      ESP_LOGE(TAG, "The command asked to be sent was not yet handled");
      break;
  }

  ESP_LOGV(TAG, "Sending TuyaDoorLock: CMD=0x%02X VERSION=%u DATA=[%s] INIT_STATE=%u", static_cast<uint8_t>(command.cmd),
           version, format_hex_pretty(command.payload).c_str(), static_cast<uint8_t>(this->init_state_));

  this->write_array({0x55, 0xAA, version, (uint8_t)command.cmd, len_hi, len_lo});
  if (!command.payload.empty())
    this->write_array(command.payload.data(), command.payload.size());

  uint8_t checksum = 0x55 + 0xAA + (uint8_t)command.cmd + len_hi + len_lo;
  for (auto &data : command.payload)
    checksum += data;
  this->write_byte(checksum);
}

void TuyaDoorLock::process_command_queue_() {
  uint32_t now = millis();
  uint32_t delay = now - this->last_command_timestamp_;

  if (now - this->last_rx_char_timestamp_ > RECEIVE_TIMEOUT) {
    this->rx_message_.clear();
  }

  if (this->expected_response_.has_value() && delay > RECEIVE_TIMEOUT) {
    this->expected_response_.reset();
    if (init_state_ != TuyaDoorLockInitState::INIT_DONE) {
      if (++this->init_retries_ >= MAX_RETRIES) {
        this->init_failed_ = true;
        ESP_LOGE(TAG, "Initialization failed at init_state %u", static_cast<uint8_t>(this->init_state_));
        this->command_queue_.erase(command_queue_.begin());
        this->init_retries_ = 0;
      }
    } else {
      this->command_queue_.erase(command_queue_.begin());
    }
  }

  // Left check of delay since last command in case there's ever a command sent by calling send_raw_command_ directly
  if (delay > COMMAND_DELAY && !this->command_queue_.empty() && this->rx_message_.empty() &&
      !this->expected_response_.has_value()) {
    this->send_raw_command_(command_queue_.front());
    if (!this->expected_response_.has_value())
      this->command_queue_.erase(command_queue_.begin());
  }
}

void TuyaDoorLock::send_command_(const TuyaDoorLockCommand &command) {
  command_queue_.push_back(command);
  process_command_queue_();
}

void TuyaDoorLock::send_empty_command_(TuyaDoorLockCommandType command) {
  send_command_(TuyaDoorLockCommand{.cmd = command, .payload = std::vector<uint8_t>{}});
}

void TuyaDoorLock::set_status_pin_() {
  bool is_network_ready = network::is_connected() && remote_is_connected();
  this->status_pin_->digital_write(is_network_ready);
}

uint8_t TuyaDoorLock::get_wifi_status_code_() {
  uint8_t status = 0x02;

  if (network::is_connected()) {
    status = 0x03;

    // Protocol version 3 also supports specifying when connected to "the cloud"
    if (this->protocol_version_ >= 0x03 && remote_is_connected()) {
      status = 0x04;
    }
  } else {
#ifdef USE_CAPTIVE_PORTAL
    if (captive_portal::global_captive_portal != nullptr && captive_portal::global_captive_portal->is_active()) {
      status = 0x01;
    }
#endif
  };

  return status;
}

uint8_t TuyaDoorLock::get_wifi_rssi_() {
#ifdef USE_WIFI
  if (wifi::global_wifi_component != nullptr)
    return wifi::global_wifi_component->wifi_rssi();
#endif

  return 0;
}

void TuyaDoorLock::send_wifi_status_() {
  uint8_t status = this->get_wifi_status_code_();

  if (status == this->wifi_status_) {
    return;
  }

  ESP_LOGD(TAG, "Sending WiFi Status");
  this->wifi_status_ = status;
  this->send_command_(TuyaDoorLockCommand{.cmd = TuyaDoorLockCommandType::WIFI_STATE, .payload = std::vector<uint8_t>{status}});
}

#ifdef USE_TIME
void TuyaDoorLock::send_local_time_() {
  std::vector<uint8_t> payload;
  ESPTime now = this->time_id_->now();
  if (now.is_valid()) {
    uint8_t year = now.year - 2000;
    uint8_t month = now.month;
    uint8_t day_of_month = now.day_of_month;
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    uint8_t second = now.second;
    // TuyaDoorLock days starts from Monday, esphome uses Sunday as day 1
    uint8_t day_of_week = now.day_of_week - 1;
    if (day_of_week == 0) {
      day_of_week = 7;
    }
    ESP_LOGD(TAG, "Sending local time");
    payload = std::vector<uint8_t>{0x01, year, month, day_of_month, hour, minute, second, day_of_week};
  } else {
    // By spec we need to notify MCU that the time was not obtained if this is a response to a query
    ESP_LOGW(TAG, "Sending missing local time");
    payload = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
  this->send_command_(TuyaDoorLockCommand{.cmd = TuyaDoorLockCommandType::LOCAL_TIME_QUERY, .payload = payload});
}
void TuyaDoorLock::send_gmt_time_() {
  std::vector<uint8_t> payload;
  ESPTime now = this->time_id_->now();
  if (now.is_valid()) {
    uint8_t year = now.year - 2000;
    uint8_t month = now.month;
    uint8_t day_of_month = now.day_of_month;
    uint8_t hour = now.hour;
    uint8_t minute = now.minute;
    uint8_t second = now.second;
    // TuyaDoorLock days starts from Monday, esphome uses Sunday as day 1
    uint8_t day_of_week = now.day_of_week - 1;
    if (day_of_week == 0) {
      day_of_week = 7;
    }
    ESP_LOGD(TAG, "Sending gmt time (TODO: shift to GMT)");
    payload = std::vector<uint8_t>{0x01, year, month, day_of_month, hour, minute, second, day_of_week};
  } else {
    // By spec we need to notify MCU that the time was not obtained if this is a response to a query
    ESP_LOGW(TAG, "Sending missing gmt time");
    payload = std::vector<uint8_t>{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  }
  this->send_command_(TuyaDoorLockCommand{.cmd = TuyaDoorLockCommandType::GMT_TIME_QUERY, .payload = payload});
}
#endif

void TuyaDoorLock::set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, false);
}

void TuyaDoorLock::set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDoorLockDatapointType::BOOLEAN, value, 1, false);
}

void TuyaDoorLock::set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDoorLockDatapointType::INTEGER, value, 4, false);
}

void TuyaDoorLock::set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, false);
}

void TuyaDoorLock::set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDoorLockDatapointType::ENUM, value, 1, false);
}

void TuyaDoorLock::set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDoorLockDatapointType::BITMASK, value, length, false);
}

void TuyaDoorLock::force_set_raw_datapoint_value(uint8_t datapoint_id, const std::vector<uint8_t> &value) {
  this->set_raw_datapoint_value_(datapoint_id, value, true);
}

void TuyaDoorLock::force_set_boolean_datapoint_value(uint8_t datapoint_id, bool value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDoorLockDatapointType::BOOLEAN, value, 1, true);
}

void TuyaDoorLock::force_set_integer_datapoint_value(uint8_t datapoint_id, uint32_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDoorLockDatapointType::INTEGER, value, 4, true);
}

void TuyaDoorLock::force_set_string_datapoint_value(uint8_t datapoint_id, const std::string &value) {
  this->set_string_datapoint_value_(datapoint_id, value, true);
}

void TuyaDoorLock::force_set_enum_datapoint_value(uint8_t datapoint_id, uint8_t value) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDoorLockDatapointType::ENUM, value, 1, true);
}

void TuyaDoorLock::force_set_bitmask_datapoint_value(uint8_t datapoint_id, uint32_t value, uint8_t length) {
  this->set_numeric_datapoint_value_(datapoint_id, TuyaDoorLockDatapointType::BITMASK, value, length, true);
}

optional<TuyaDoorLockDatapoint> TuyaDoorLock::get_datapoint_(uint8_t datapoint_id) {
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      return datapoint;
  }
  return {};
}

void TuyaDoorLock::set_numeric_datapoint_value_(uint8_t datapoint_id, TuyaDoorLockDatapointType datapoint_type, const uint32_t value,
                                                uint8_t length, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %" PRIu32, datapoint_id, value);
  optional<TuyaDoorLockDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  } else if (datapoint->type != datapoint_type) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_uint == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }

  std::vector<uint8_t> data;
  switch (length) {
    case 4:
      data.push_back(value >> 24);
      data.push_back(value >> 16);
    case 2:
      data.push_back(value >> 8);
    case 1:
      data.push_back(value >> 0);
      break;
    default:
      ESP_LOGE(TAG, "Unexpected datapoint length %u", length);
      return;
  }
  this->send_datapoint_command_(datapoint_id, datapoint_type, data);
}

void TuyaDoorLock::set_raw_datapoint_value_(uint8_t datapoint_id, const std::vector<uint8_t> &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, format_hex_pretty(value).c_str());
  optional<TuyaDoorLockDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  } else if (datapoint->type != TuyaDoorLockDatapointType::RAW) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_raw == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }
  this->send_datapoint_command_(datapoint_id, TuyaDoorLockDatapointType::RAW, value);
}

void TuyaDoorLock::set_string_datapoint_value_(uint8_t datapoint_id, const std::string &value, bool forced) {
  ESP_LOGD(TAG, "Setting datapoint %u to %s", datapoint_id, value.c_str());
  optional<TuyaDoorLockDatapoint> datapoint = this->get_datapoint_(datapoint_id);
  if (!datapoint.has_value()) {
    ESP_LOGW(TAG, "Setting unknown datapoint %u", datapoint_id);
  } else if (datapoint->type != TuyaDoorLockDatapointType::STRING) {
    ESP_LOGE(TAG, "Attempt to set datapoint %u with incorrect type", datapoint_id);
    return;
  } else if (!forced && datapoint->value_string == value) {
    ESP_LOGV(TAG, "Not sending unchanged value");
    return;
  }
  std::vector<uint8_t> data;
  for (char const &c : value) {
    data.push_back(c);
  }
  this->send_datapoint_command_(datapoint_id, TuyaDoorLockDatapointType::STRING, data);
}

void TuyaDoorLock::send_datapoint_command_(uint8_t datapoint_id, TuyaDoorLockDatapointType datapoint_type, std::vector<uint8_t> data) {
  std::vector<uint8_t> buffer;
  buffer.push_back(datapoint_id);
  buffer.push_back(static_cast<uint8_t>(datapoint_type));
  buffer.push_back(data.size() >> 8);
  buffer.push_back(data.size() >> 0);
  buffer.insert(buffer.end(), data.begin(), data.end());

  this->send_command_(TuyaDoorLockCommand{.cmd = TuyaDoorLockCommandType::MODULE_SEND_COMMAND, .payload = buffer});
}

void TuyaDoorLock::register_listener(uint8_t datapoint_id, const std::function<void(TuyaDoorLockDatapoint)> &func) {
  auto listener = TuyaDoorLockDatapointListener{
      .datapoint_id = datapoint_id,
      .on_datapoint = func,
  };
  this->listeners_.push_back(listener);

  // Run through existing datapoints
  for (auto &datapoint : this->datapoints_) {
    if (datapoint.id == datapoint_id)
      func(datapoint);
  }
}

TuyaDoorLockInitState TuyaDoorLock::get_init_state() { return this->init_state_; }

void TuyaDoorLock::parse_totp_key() {
  // Free any previously allocated memory
  if (this->totp_key_ != nullptr) {
    delete[] this->totp_key_;
    this->totp_key_ = nullptr;
    this->totp_key_length_ = 0;
  }

  // Convert std::string to const char*
  const char *key_cstr = this->totp_key_b32.c_str();
  int decoded_length = this->totp_key_b32.length();    // Set to the maximum length you expect
  uint8_t *decoded_key = new uint8_t[decoded_length];  // Allocate memory for decoded_key
  int actual_decoded_length = otp::base32_decode(key_cstr, decoded_key, decoded_length);

  if (actual_decoded_length > 0) {
    ESP_LOGD(TAG, "Sucessfully read totp_key_b32");
    // Store the decoded key
    this->totp_key_length_ = actual_decoded_length;
    this->totp_key_ = new uint8_t[this->totp_key_length_];
    std::memcpy(this->totp_key_, decoded_key, this->totp_key_length_);
  } else {
    ESP_LOGE(TAG, "Fail to decode the provided totp_key_b32");
  }

  // Clean up temporary decoded_key
  delete[] decoded_key;
}

}  // namespace tuya_door_lock
}  // namespace esphome

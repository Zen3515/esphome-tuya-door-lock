# ESPHome Tuya Door Lock

This is tuya that's renamed to tuya_door_lock, then adapted to be compatible with battery powered door lock.

Why?
The command between normal tuya protocol and this one overlapped too much.

## My datapoint are

| DP 	| DP_NAME              	| Transfer Type 	| Data Type 	| Properties                                                                                                                                                       	|
|----	|----------------------	|---------------	|-----------	|------------------------------------------------------------------------------------------------------------------------------------------------------------------	|
| 1  	| unlock_fingerprint   	| Report Only   	| Value     	| Value Range: 0-999, Pitch: 1, Scale: 0, Unit:                                                                                                                    	|
| 2  	| unlock_password      	| Report Only   	| Value     	| Value Range: 0-999, Pitch: 1, Scale: 0, Unit:                                                                                                                    	|
| 3  	| unlock_temporary     	| Report Only   	| Value     	| Value Range: 0-999, Pitch: 1, Scale: 0, Unit:                                                                                                                    	|
| 4  	| unlock_dynamic       	| Report Only   	| Value     	| Value Range: 0-999, Pitch: 1, Scale: 0, Unit:                                                                                                                    	|
| 5  	| unlock_card          	| Report Only   	| Value     	| Value Range: 0-999, Pitch: 1, Scale: 0, Unit:                                                                                                                    	|
| 8  	| alarm_lock           	| Report Only   	| Enum      	| Enum Value: wrong_finger, wrong_password, wrong_card, wrong_face, tongue_bad, too_hot, unclosed_time, tongue_not_out, pry, key_in, low_battery, power_off, shock 	|
| 9  	| unlock_request       	| Send Only     	| Value     	| Value Range: 0-90, Pitch: 1, Scale: 1, Unit:                                                                                                                     	|
| 10 	| reply_unlock_request 	| Report Only   	| Bool      	|                                                                                                                                                                  	|
| 11 	| battery_state        	| Report Only   	| Enum      	| Enum Value: high, medium, low, poweroff                                                                                                                          	|
| 15 	| unlock_app           	| Report Only   	| Value     	| Value Range: 0-999, Pitch: 1, Scale: 0, Unit:                                                                                                                    	|
| 16 	| hijack               	| Report Only   	| Bool      	|                                                                                                                                                                  	|
| 19 	| doorbell             	| Report Only   	| Bool      	|                                                                                                                                                                  	|

## Hidden features:
- Request remote unlock (9 + #) it set time left to DP9, you must answer with DP10. My implementation require that to answer the unlock, you also needs to send the totp of 30s and length 6 which was generated using the app too.
- When input password of length 8, it will check with dynamic password

## Configuration variables:

- **time_id** (*Optional*, :ref:`config-id`): Some Tuya devices support obtaining local time from ESPHome.
  Specify the ID of the :doc:`time/index` which will be used.

- **status_pin** (*Optional*, :ref:`Pin Schema <config-pin_schema>`): Some Tuya devices support WiFi status reporting ONLY through gpio pin.
  Specify the pin reported in the config dump or leave empty otherwise.
  More about this `here <https://developer.tuya.com/en/docs/iot/tuya-cloud-universal-serial-port-access-protocol?id=K9hhi0xxtn9cb#title-6-Query%20working%20mode>`__.

- **ignore_mcu_update_on_datapoints** (*Optional*, list): A list of datapoints to ignore MCU updates for.  Useful for certain broken/erratic hardware and debugging.

- **en_binary_sensor** (*Optional*, :ref:`Binary Sensor <config-binary_sensor>`): After you forced the device to be 24/7 online, you can use some binary_sensor watch the original power control.

- **totp_key_b32** (*Optional*): A Based32 (RFC 4648, RFC 3548) encoded string you can use site like [this](https://cryptii.com/pipes/base32) to convert some bytes into your secret keys. You can generate the qrcode for authenticator scan using site like [this](https://stefansundin.github.io/2fa-qr/). For temp password time should be 300 seconds, legth is 8. For request remote unlock, you need to use 30s with length of 6.

## Example configuration

```yaml
globals:
   - id: global_last_unlock_method
     type: std::string
     restore_value: yes
     max_restore_data_length: 19 # 18 + 1

time:
  - platform: homeassistant
    id: homeassistant_time

tuya_door_lock:
  id: tuyadeivce
  time_id: homeassistant_time
  totp_key_b32: "ABCDEFGHIJKLMNO" # please beware of the = padding at the end, you don't need it
  en_binary_sensor: tuya_mcu_power
  on_datapoint_update:
    - sensor_datapoint: 9 # Remote unlock time
      datapoint_type: int
      then:
        - lambda: |-
            ESP_LOGD("main check_remote_unlock", "on_datapoint_update (Request remote unlock) %u", x);
            if (x <= 0) { return; }
            auto tuya_instance = id(tuyadeivce);
            auto ha_time = id(homeassistant_time);
            auto input_password_instance = id(input_totp_text);
            auto input_password = input_password_instance->state;
            if (input_password.length() < 6) { return; }
            if (tuya_instance->totp_key_length_ > 0 && tuya_instance->totp_key_ != nullptr) {
              // Generate totp password
              ESPTime now = ha_time->now();
              char generated_password_str[7];
              if (now.is_valid()) {
                // Convert the current time to a timestamp (in seconds since the epoch)
                auto timestamp = (time_t)floor(now.timestamp / 30.0);  // Use the 30s time window
                const uint32_t generated_password = otp::totp_hash_token(tuya_instance->totp_key_, tuya_instance->totp_key_length_, timestamp, 6);
                snprintf(generated_password_str, sizeof(generated_password_str), "%06u", generated_password);
                ESP_LOGD("main check_remote_unlock", "Generated TOTP password: %u", generated_password);
              } else {
                ESP_LOGW("main check_remote_unlock", "Current time is invalid, cannot generate TOTP password.");
                return;
              }
              bool is_password_valid = memcmp(generated_password_str, input_password.c_str(), 6) == 0;
              if (is_password_valid) {
                ESP_LOGD("main check_remote_unlock", "Password matched");
              } else {
                ESP_LOGD("main check_remote_unlock", "Password not matched");
              }
              tuya_instance->force_set_boolean_datapoint_value(10, is_password_valid);
              auto call = input_password_instance->make_call();
              call.set_value(""); // reset the password field
              call.perform();
            }
    - sensor_datapoint: 1 # unlock_fingerprint
      datapoint_type: int
      then:
        - text_sensor.template.publish:
            id: last_unlock_method
            state: "Fingerprint"
    - sensor_datapoint: 2 # unlock_password
      datapoint_type: int
      then:
        - text_sensor.template.publish:
            id: last_unlock_method
            state: "Password"
    - sensor_datapoint: 3 # unlock_temporary
      datapoint_type: int
      then:
        - text_sensor.template.publish:
            id: last_unlock_method
            state: "Temporary Password"
    - sensor_datapoint: 4 # unlock_dynamic
      datapoint_type: int
      then:
        - text_sensor.template.publish:
            id: last_unlock_method
            state: "Dynamic Password"
    - sensor_datapoint: 5 # unlock_card
      datapoint_type: int
      then:
        - text_sensor.template.publish:
            id: last_unlock_method
            state: "Card"
    - sensor_datapoint: 15 # unlock_app
      datapoint_type: int
      then:
        - text_sensor.template.publish:
            id: last_unlock_method
            state: "App"

text:
  - platform: template
    id: input_totp_text
    name: TOTP input # for request remote unlock
    mode: password
    optimistic: true
    initial_value: ""

text_sensor:
  - platform: template
    name: Last unlock method
    id: last_unlock_method
    icon: mdi:book-check
    lambda: "return id(global_last_unlock_method);"
    on_value:
      - globals.set:
          id: global_last_unlock_method
          value: !lambda |-
            return x;

binary_sensor:
  - platform: gpio
    name: Tuya mcu power
    id: tuya_mcu_power
    internal: False
    pin:
      number: GPIO1
      inverted: False
      mode:
        input: True
        pulldown: True
    on_release: 
      then:
        - script.execute:
            id: reset_last_unlock_method

script:
  - id: reset_last_unlock_method
    mode: restart
    then:
      - delay: 5s
      - text_sensor.template.publish:
          id: last_unlock_method
          state: "Locked"

```
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

- **en_pin** (*Optional*, :ref:`Pin Schema <config-pin_schema>`): After you forced the device to be 24/7 online, you can use some pin to watch the original power control.

- **totp_key_b32** (*Optional*): A Based32 (RFC 4648, RFC 3548) encoded string you can use site like [this](https://cryptii.com/pipes/base32) to convert some bytes into your secret keys. You can generate the qrcode for authenticator scan using site like [this](https://stefansundin.github.io/2fa-qr/). For temp password time should be 300 seconds, legth is 8. For request remote unlock, you need to use 30s with length of 6.

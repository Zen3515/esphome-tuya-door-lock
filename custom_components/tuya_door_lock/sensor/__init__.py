from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_SENSOR_DATAPOINT
from .. import tuya_ns, CONF_TUYA_ID, TuyaDoorLock

DEPENDENCIES = ["tuya_door_lock"]
CODEOWNERS = ["@jesserockz"]

TuyaDoorLockSensor = tuya_ns.class_("TuyaDoorLockSensor", sensor.Sensor, cg.Component)

CONFIG_SCHEMA = (
    sensor.sensor_schema(TuyaDoorLockSensor)
    .extend(
        {
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(TuyaDoorLock),
            cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)

    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))

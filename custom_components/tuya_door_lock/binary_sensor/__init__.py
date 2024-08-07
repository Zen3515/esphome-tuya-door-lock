from esphome.components import binary_sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_SENSOR_DATAPOINT

from .. import tuya_ns, CONF_TUYA_ID, TuyaDoorLock

DEPENDENCIES = ["tuya_door_lock"]
CODEOWNERS = ["@jesserockz"]

TuyaDoorLockBinarySensor = tuya_ns.class_(
    "TuyaDoorLockBinarySensor", binary_sensor.BinarySensor, cg.Component
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(TuyaDoorLockBinarySensor)
    .extend(
        {
            cv.GenerateID(CONF_TUYA_ID): cv.use_id(TuyaDoorLock),
            cv.Required(CONF_SENSOR_DATAPOINT): cv.uint8_t,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)

    paren = await cg.get_variable(config[CONF_TUYA_ID])
    cg.add(var.set_tuya_parent(paren))

    cg.add(var.set_sensor_id(config[CONF_SENSOR_DATAPOINT]))

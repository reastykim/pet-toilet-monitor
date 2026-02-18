local log = require "log"
log.info("=== LITTERBOX v10 LOADING ===")

local ZigbeeDriver = require "st.zigbee"
local capabilities = require "st.capabilities"
local defaults = require "st.zigbee.defaults"
local zcl_clusters = require "st.zigbee.zcl.clusters"
local data_types = require "st.zigbee.data_types"
local TemperatureMeasurement = zcl_clusters.TemperatureMeasurement
local OnOff = zcl_clusters.OnOff

-- CO₂ Concentration Measurement cluster ID (0x040D)
local CO2_CLUSTER_ID = 0x040D
local CO2_MEASURED_VALUE_ATTR = 0x0000

log.info("=== LITTERBOX v10 modules loaded ===")

-- Temperature handler
local function temperature_attr_handler(driver, device, value, zb_rx)
  log.info(string.format("Temperature: raw=%d, %.2f C", value.value, value.value / 100.0))
  device:emit_event(capabilities.temperatureMeasurement.temperature({
    value = value.value / 100.0,
    unit = "C"
  }))
end

-- CO₂/NH₃ handler (ZCL float → ppm)
local function co2_attr_handler(driver, device, value, zb_rx)
  local zcl_float = value.value  -- ZCL single precision float (fraction of 1)
  local ppm = zcl_float * 1000000.0
  log.info(string.format("CO2/NH3: raw=%.8f, %.0f ppm", zcl_float, ppm))
  device:emit_event(capabilities.carbonDioxideMeasurement.carbonDioxide({
    value = math.floor(ppm),
    unit = "ppm"
  }))
end

-- Lifecycle: device added
local function device_added(driver, device)
  log.info("=== LITTERBOX v10 device_added ===")
  device:emit_event(capabilities.temperatureMeasurement.temperature({ value = 0, unit = "C" }))
  device:emit_event(capabilities.carbonDioxideMeasurement.carbonDioxide({ value = 0, unit = "ppm" }))
  device:emit_event(capabilities.switch.switch.off())
end

-- Lifecycle: device init
local function device_init(driver, device)
  log.info("=== LITTERBOX v10 device_init ===")
end

-- Lifecycle: doConfigure
local function do_configure(driver, device)
  log.info("=== LITTERBOX v10 do_configure ===")
  device:configure()
  -- Configure temperature reporting (min 1s, max 300s, delta 1.00°C)
  device:send(TemperatureMeasurement.attributes.MeasuredValue:configure_reporting(device, 1, 300, 100))
  log.info("=== LITTERBOX v10 configure_reporting sent ===")
end

local driver_template = {
  supported_capabilities = {
    capabilities.temperatureMeasurement,
    capabilities.carbonDioxideMeasurement,
    capabilities.switch,
  },
  zigbee_handlers = {
    attr = {
      [TemperatureMeasurement.ID] = {
        [TemperatureMeasurement.attributes.MeasuredValue.ID] = temperature_attr_handler,
      },
      [CO2_CLUSTER_ID] = {
        [CO2_MEASURED_VALUE_ATTR] = co2_attr_handler,
      },
    }
  },
  lifecycle_handlers = {
    added = device_added,
    init = device_init,
    doConfigure = do_configure,
  },
  health_check = false,
}

defaults.register_for_default_handlers(driver_template, driver_template.supported_capabilities)

local litterbox_driver = ZigbeeDriver("litterbox", driver_template)

log.info("=== LITTERBOX v10 driver created, calling run ===")
litterbox_driver:run()

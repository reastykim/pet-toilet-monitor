local log = require "log"
log.info("=== LITTERBOX v11 LOADING ===")

local ZigbeeDriver = require "st.zigbee"
local capabilities = require "st.capabilities"
local defaults = require "st.zigbee.defaults"
local zcl_clusters = require "st.zigbee.zcl.clusters"
local data_types = require "st.zigbee.data_types"
local OnOff = zcl_clusters.OnOff

-- CO₂ Concentration Measurement cluster ID (0x040D)
local CO2_CLUSTER_ID = 0x040D
local CO2_MEASURED_VALUE_ATTR = 0x0000

log.info("=== LITTERBOX v11 modules loaded ===")

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
  log.info("=== LITTERBOX v11 device_added ===")
  device:emit_event(capabilities.carbonDioxideMeasurement.carbonDioxide({ value = 0, unit = "ppm" }))
  device:emit_event(capabilities.switch.switch.off())
end

-- Lifecycle: device init
local function device_init(driver, device)
  log.info("=== LITTERBOX v11 device_init ===")
end

-- Lifecycle: doConfigure
local function do_configure(driver, device)
  log.info("=== LITTERBOX v11 do_configure ===")
  device:configure()
  log.info("=== LITTERBOX v11 configure done ===")
end

local driver_template = {
  supported_capabilities = {
    capabilities.carbonDioxideMeasurement,
    capabilities.switch,
  },
  zigbee_handlers = {
    attr = {
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

log.info("=== LITTERBOX v11 driver created, calling run ===")
litterbox_driver:run()

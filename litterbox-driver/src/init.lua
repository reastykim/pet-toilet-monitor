local log = require "log"
log.info("=== LITTERBOX v12 LOADING ===")

local ZigbeeDriver = require "st.zigbee"
local capabilities = require "st.capabilities"
local defaults = require "st.zigbee.defaults"
local zcl_clusters = require "st.zigbee.zcl.clusters"
local data_types = require "st.zigbee.data_types"
local OnOff = zcl_clusters.OnOff

-- Custom NH₃ Concentration Measurement cluster (0xFC00, Manufacturer-Specific)
-- Attribute 0x0000: uint16, ppm directly (no float fraction conversion needed)
local NH3_CLUSTER_ID = 0xFC00
local NH3_MEASURED_VALUE_ATTR = 0x0000

log.info("=== LITTERBOX v12 modules loaded ===")

-- NH₃ handler: uint16 ppm → tvocMeasurement capability
local function nh3_attr_handler(driver, device, value, zb_rx)
  local ppm = value.value  -- uint16, already in ppm
  log.info(string.format("NH3: %d ppm (cluster 0x%04X)", ppm, NH3_CLUSTER_ID))
  device:emit_event(capabilities.tvocMeasurement.tvocLevel({
    value = ppm,
    unit = "ppm"
  }))
end

-- Lifecycle: device added
local function device_added(driver, device)
  log.info("=== LITTERBOX v12 device_added ===")
  device:emit_event(capabilities.tvocMeasurement.tvocLevel({ value = 0, unit = "ppm" }))
  device:emit_event(capabilities.switch.switch.off())
end

-- Lifecycle: device init
local function device_init(driver, device)
  log.info("=== LITTERBOX v12 device_init ===")
end

-- Lifecycle: doConfigure
local function do_configure(driver, device)
  log.info("=== LITTERBOX v12 do_configure ===")
  device:configure()
  log.info("=== LITTERBOX v12 configure done ===")
end

local driver_template = {
  supported_capabilities = {
    capabilities.tvocMeasurement,
    capabilities.switch,
  },
  zigbee_handlers = {
    attr = {
      [NH3_CLUSTER_ID] = {
        [NH3_MEASURED_VALUE_ATTR] = nh3_attr_handler,
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

log.info("=== LITTERBOX v12 driver created, calling run ===")
litterbox_driver:run()

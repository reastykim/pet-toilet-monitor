local log = require "log"
log.info("=== LITTERBOX v14 LOADING ===")

local ZigbeeDriver = require "st.zigbee"
local capabilities = require "st.capabilities"
local defaults = require "st.zigbee.defaults"
local zcl_clusters = require "st.zigbee.zcl.clusters"
local data_types = require "st.zigbee.data_types"
local OnOff = zcl_clusters.OnOff

-- Custom NH₃ Concentration Measurement cluster (0xFC00, Manufacturer-Specific)
-- Attr 0x0000: uint16 ppm  — NH₃ concentration
-- Attr 0x0003: uint8       — event type (0=none, 1=urination, 2=defecation)
local NH3_CLUSTER_ID       = 0xFC00
local NH3_MEASURED_VALUE_ATTR = 0x0000
local NH3_EVENT_TYPE_ATTR  = 0x0003

log.info("=== LITTERBOX v14 modules loaded ===")

-- NH₃ handler: uint16 ppm → tvocMeasurement capability
local function nh3_attr_handler(driver, device, value, zb_rx)
  local ppm = value.value  -- uint16, already in ppm
  log.info(string.format("NH3: %d ppm (cluster 0x%04X)", ppm, NH3_CLUSTER_ID))
  device:emit_event(capabilities.tvocMeasurement.tvocLevel({
    value = ppm,
    unit = "ppm"
  }))
end

-- Event type handler: uint8 → carbonMonoxideDetector capability
-- 0 = no event  → carbonMonoxide.clear()  (맑음)
-- 1 = urination → carbonMonoxide.detected()
-- 2 = defecation → carbonMonoxide.detected()
local function event_type_attr_handler(driver, device, value, zb_rx)
  local event_type = value.value  -- uint8
  local event_names = { [0] = "NONE", [1] = "URINATION", [2] = "DEFECATION" }
  log.info(string.format("EventType: %d (%s)", event_type, event_names[event_type] or "UNKNOWN"))

  if event_type == 0 then
    device:emit_event(capabilities.carbonMonoxideDetector.carbonMonoxide.clear())
  else
    device:emit_event(capabilities.carbonMonoxideDetector.carbonMonoxide.detected())
    if event_type == 1 then
      log.info("EVENT: URINATION detected → CO detected")
    elseif event_type == 2 then
      log.info("EVENT: DEFECATION detected → CO detected")
    end
  end
end

-- Lifecycle: device added
local function device_added(driver, device)
  log.info("=== LITTERBOX v14 device_added ===")
  device:emit_event(capabilities.tvocMeasurement.tvocLevel({ value = 0, unit = "ppm" }))
  device:emit_event(capabilities.carbonMonoxideDetector.carbonMonoxide.clear())
  device:emit_event(capabilities.switch.switch.off())
end

-- Lifecycle: device init
local function device_init(driver, device)
  log.info("=== LITTERBOX v14 device_init ===")
end

-- Lifecycle: doConfigure
local function do_configure(driver, device)
  log.info("=== LITTERBOX v14 do_configure ===")
  device:configure()
  log.info("=== LITTERBOX v14 configure done ===")
end

local driver_template = {
  supported_capabilities = {
    capabilities.tvocMeasurement,
    capabilities.carbonMonoxideDetector,
    capabilities.switch,
  },
  zigbee_handlers = {
    attr = {
      [NH3_CLUSTER_ID] = {
        [NH3_MEASURED_VALUE_ATTR] = nh3_attr_handler,
        [NH3_EVENT_TYPE_ATTR]     = event_type_attr_handler,
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

log.info("=== LITTERBOX v14 driver created, calling run ===")
litterbox_driver:run()

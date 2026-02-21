local log = require "log"
log.info("=== LITTERBOX v18 LOADING ===")

local ZigbeeDriver = require "st.zigbee"
local capabilities = require "st.capabilities"
local defaults = require "st.zigbee.defaults"
local zcl_clusters = require "st.zigbee.zcl.clusters"
local data_types = require "st.zigbee.data_types"
local OnOff = zcl_clusters.OnOff

-- Custom NH₃ Concentration Measurement cluster (0xFC00, Manufacturer-Specific)
-- Attr 0x0000: uint16 ppm  — NH₃ concentration
-- Attr 0x0003: uint8       — event type (0=none, 1=urination, 2=defecation)
local NH3_CLUSTER_ID          = 0xFC00
local NH3_MEASURED_VALUE_ATTR = 0x0000
local NH3_EVENT_TYPE_ATTR     = 0x0003

-- Custom capabilities
local nh3Measurement = capabilities["streetsmile37673.nh3measurement"]
local litterEvent    = capabilities["streetsmile37673.litterevent"]

log.info(string.format("=== LITTERBOX v18 capabilities: nh3=%s, litter=%s ===",
  tostring(nh3Measurement), tostring(litterEvent)))

-- NH₃ handler: uint16 ppm → nh3Measurement custom capability
local function nh3_attr_handler(driver, device, value, zb_rx)
  local ppm = value.value  -- uint16, already in ppm
  log.info(string.format("NH3: %d ppm (cluster 0x%04X)", ppm, NH3_CLUSTER_ID))
  device:emit_event(nh3Measurement.ammoniaLevel({
    value = ppm,
    unit = "ppm"
  }))
end

-- Event type handler: uint8 → litterEvent custom capability
-- 0 = no event  → litterEvent("none")
-- 1 = urination → litterEvent("urination")
-- 2 = defecation → litterEvent("defecation")
local function event_type_attr_handler(driver, device, value, zb_rx)
  local event_type = value.value  -- uint8
  local event_names = { [0] = "none", [1] = "urination", [2] = "defecation" }
  local event_name = event_names[event_type] or "none"
  log.info(string.format("EventType: %d → litterEvent(%s)", event_type, event_name))
  device:emit_event(litterEvent.litterEvent({ value = event_name }))
end

-- Lifecycle: device added
local function device_added(driver, device)
  log.info("=== LITTERBOX v18 device_added ===")
  device:emit_event(nh3Measurement.ammoniaLevel({ value = 0, unit = "ppm" }))
  device:emit_event(litterEvent.litterEvent({ value = "none" }))
  device:emit_event(capabilities.switch.switch.off())
end

-- Lifecycle: device init
local function device_init(driver, device)
  log.info("=== LITTERBOX v18 device_init ===")
  -- Emit initial state so the app doesn't show "-" after driver switch
  local ok, err = pcall(function()
    device:emit_event(litterEvent.litterEvent({ value = "none" }))
  end)
  if not ok then
    log.error("device_init litterEvent emit failed: " .. tostring(err))
  end
end

-- Lifecycle: doConfigure
local function do_configure(driver, device)
  log.info("=== LITTERBOX v18 do_configure ===")
  device:configure()
  log.info("=== LITTERBOX v18 configure done ===")
end

local driver_template = {
  supported_capabilities = {
    nh3Measurement,
    litterEvent,
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
    added   = device_added,
    init    = device_init,
    doConfigure = do_configure,
  },
  health_check = false,
}

defaults.register_for_default_handlers(driver_template, driver_template.supported_capabilities)

local litterbox_driver = ZigbeeDriver("litterbox", driver_template)

log.info("=== LITTERBOX v18 driver created, calling run ===")
litterbox_driver:run()

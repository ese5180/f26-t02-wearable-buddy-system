cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(buddyguard)
if(CONFIG_BUDDYGUARD_WIFI)
  target_sources(app PRIVATE src/fleet/wifi_fleet.c)
endif()
if(CONFIG_BUDDYGUARD_FLEET)
  zephyr_include_directories(config)
  target_sources(app PRIVATE src/fleet/fleet.c)
endif()
target_sources(app PRIVATE
  src/main.c
  src/ble/presence.c
  src/console/commands.c
  src/update/health.c
  src/protocol/wire.c
  src/sensors/motion.c
  src/sensors/imu.c
  src/drivers/wio_sx1262.c
  src/radio/link.c
  src/radio/exchange.c
  src/radio/history.c
  src/roster/roster.c
)
target_include_directories(app PRIVATE include)
target_include_directories(app PRIVATE
  ${ZEPHYR_LORAMAC_NODE_MODULE_DIR}/src/radio
  ${ZEPHYR_LORAMAC_NODE_MODULE_DIR}/src/system
  ${ZEPHYR_LORAMAC_NODE_MODULE_DIR}/src/boards
)

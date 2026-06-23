zephyr_library()
zephyr_library_sources(${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_cmd_mo.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_OS_DETECTION
  ${CMAKE_CURRENT_SOURCE_DIR}/src/os_detection.c)
zephyr_library_sources_ifdef(CONFIG_ZMK_BLE_PROFILE_NAME
  ${CMAKE_CURRENT_SOURCE_DIR}/src/ble_profile_name.c)

if(CONFIG_ZMK_OS_DETECTION)
  zephyr_ld_options(-Wl,--wrap=usb_handle_os_desc)
endif()

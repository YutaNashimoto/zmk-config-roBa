if(CONFIG_ZMK_OS_DETECTION OR CONFIG_ZMK_BLE_PROFILE_NAME OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  zephyr_library()
  zephyr_library_sources_ifdef(CONFIG_ZMK_OS_DETECTION
    ${CMAKE_CURRENT_SOURCE_DIR}/src/os_detection.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_BLE_PROFILE_NAME
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ble_profile_name.c)
  # キーマップ/HIDを持つ中央機(central)のみでビルド。周辺機(roBa_L)では
  # zmk_keymap_layer_* / raise_zmk_keycode_state_changed が存在せず
  # 未定義参照になるため除外する。
  zephyr_library_sources_ifdef(CONFIG_ZMK_SPLIT_ROLE_CENTRAL
    ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_cmd_mo.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_SPLIT_ROLE_CENTRAL
    ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_nomod.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_SPLIT_ROLE_CENTRAL
    ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_ctrl_mo.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_SPLIT_ROLE_CENTRAL
    ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_to_base_keep_cpi.c)
  zephyr_library_sources_ifdef(CONFIG_ZMK_SPLIT_ROLE_CENTRAL
    ${CMAKE_CURRENT_SOURCE_DIR}/src/behavior_toggle_lshift.c)
endif()

if(CONFIG_ZMK_OS_DETECTION)
  zephyr_ld_options(-Wl,--wrap=usb_handle_os_desc)
endif()

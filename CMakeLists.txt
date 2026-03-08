zephyr_include_directories(include)

if ((NOT CONFIG_ZMK_SPLIT) OR CONFIG_ZMK_SPLIT_ROLE_CENTRAL)
  target_sources_ifdef(CONFIG_ZMK_OSKEY_STATE app PRIVATE behaviors/os_state.c)
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_OS_SELECTOR app PRIVATE behaviors/behavior_os_selector.c)
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_OS_KEY app PRIVATE behaviors/behavior_os_key.c)
  target_sources_ifdef(CONFIG_ZMK_BEHAVIOR_OS_LAYER_MOD app PRIVATE behaviors/behavior_os_layer_mod.c)
endif()

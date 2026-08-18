if(CONFIG_WISMOD_FW_COMPONENT_LIB)
  zephyr_library()

  zephyr_library_sources(
    src/fw_component.c
  )

  zephyr_include_directories(include)
  zephyr_library_include_directories(include)
endif()

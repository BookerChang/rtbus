if(CONFIG_RTBUS_LIB)
  zephyr_library()

  zephyr_library_sources(
    src/rtbus.c
  )
  if(CONFIG_RTBUS_ITERABLE_ENTRY)
    zephyr_library_sources(src/rtbus_iterable.c)
  else()
    zephyr_library_sources(src/rtbus_entries.c)
  endif()

  zephyr_library_sources_ifdef(CONFIG_RTBUS_NATIVE_SERVICE
    subsys/rtbus_runtime.c
    subsys/rtbus_bootstrap.c
    subsys/rtbus_event.c
    subsys/native_service/native_service.c
  )

  zephyr_include_directories(include)
  zephyr_library_include_directories(
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../system/include
  )
endif()

if(CONFIG_RTBUS_LIB)
  target_sources(app PRIVATE
    src/rtbus.c
  )
  if(CONFIG_RTBUS_ITERABLE_ENTRY)
    target_sources(app PRIVATE src/rtbus_iterable.c)
  else()
    target_sources(app PRIVATE src/rtbus_entries.c)
  endif()

  target_sources_ifdef(CONFIG_RTBUS_NATIVE_SERVICE app PRIVATE
    services/rtbus_runtime.c
    services/rtbus_diagnostics.c
    services/rtbus_event.c
    services/native_service/native_service.c
    services/native_service/native_board.c
    services/rtbus_image.c
  )

  target_include_directories(app PRIVATE
    include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../../system/include
  )
endif()

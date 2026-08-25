# SPDX-License-Identifier: MPL-2.0

cmake_minimum_required(VERSION 3.20.0)

# Devicetree is generated before Kconfig is evaluated. Read the runtime
# default here so the Nordic HV setting also selects the matching TCXO voltage.
set(nordic_hv_regout0_3v3_config "")
foreach(conf_file "${CMAKE_CURRENT_SOURCE_DIR}/prj.conf" ${EXTRA_CONF_FILE})
    if(EXISTS "${conf_file}")
        file(STRINGS "${conf_file}"
            nordic_hv_regout0_3v3_config_file
            REGEX "^CONFIG_NORDIC_HV_REGOUT0_3V3=y$"
        )
        if(nordic_hv_regout0_3v3_config_file)
            list(APPEND nordic_hv_regout0_3v3_config
                ${nordic_hv_regout0_3v3_config_file}
            )
        endif()
    endif()
endforeach()
if(nordic_hv_regout0_3v3_config AND BOARD MATCHES "^(rak4631|.*nrf52840.*)")
    list(APPEND EXTRA_DTC_OVERLAY_FILE
        "${CMAKE_CURRENT_SOURCE_DIR}/dts/nordic_hv_regout0_3v3.overlay"
    )
endif()

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(rtbus_runtime)

add_subdirectory(../modules/rtbus rtbus)

zephyr_linker_sources(SECTIONS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/sections.ld
)

target_sources(app PRIVATE
    src/main.c
    src/services/startup_service.c
)

target_sources_ifdef(CONFIG_NORDIC_HV_REGOUT0_3V3 app PRIVATE
    src/platform/nordic/nordic_hv_regout0_3v3.c
)

zephyr_include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../system/include
    include
)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/eil-cmake-utils/cmake)
include(eil_check_idf_features)
eil_check_idf_has_esp_drivers()

# List of common ESP-IDF components required by this component
set(REQUIRED_COMPONENTS freertos esp_idf_lib_helpers)

if(EIL_IDF_HAS_ESP_DRIVERS)
    list(APPEND REQUIRED_COMPONENTS esp_driver_gpio)
else()
    list(APPEND REQUIRED_COMPONENTS driver)
endif()

if(${IDF_TARGET} STREQUAL esp8266)
    set(REQUIRED_COMPONENTS esp8266 freertos esp_idf_lib_helpers)
endif()

idf_component_register(
    SRCS dht.c
    INCLUDE_DIRS .
    REQUIRES ${REQUIRED_COMPONENTS}
)

include(eil_ci)

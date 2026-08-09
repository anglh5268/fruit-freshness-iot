idf_component_register(
    SRCS
        "main.c"
        "sensor_bus.c"
        "vl53l0x.c"
        "as7341.c"
        "sensor_task.c"
        "display_task.c"
        "material_classifier.c"
        "freshness_classifier.c"
        "app_wifi.c"
        "cloud_protocol.c"
        "cloud_client.c"
        "cloud_task.c"
        "utf8_lcd.c"

    INCLUDE_DIRS
        "."

    PRIV_REQUIRES
        driver
        vl53l0x_api
        BSP
        esp_event
        esp_netif
        esp_wifi
        esp_http_client
        esp_timer
        json
        mbedtls
        nvs_flash
)

# Reconfigure when the ignored local credential file is created or removed.
# The compile definition also makes Ninja rebuild app_wifi.c after that change.
set_property(
    DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/app_wifi_credentials.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/cloud_gateway_config.h"
)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/app_wifi_credentials.h")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_WIFI_LOCAL_CREDENTIALS=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_WIFI_LOCAL_CREDENTIALS=0)
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/cloud_gateway_config.h")
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_CLOUD_LOCAL_CONFIG=1)
else()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE APP_CLOUD_LOCAL_CONFIG=0)
endif()

# RoboMind-S3 Main Component

file(READ "${CMAKE_CURRENT_LIST_DIR}/../../PPM/profiles/fatigue_lite.json" PPM_PROFILE_JSON)
configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/ppm_profile.h.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ppm_profile_generated.h"
    @ONLY)

idf_component_register(
    SRCS
        "main.cpp"
        "ppm_producer.cpp"
        "wire_codec.cpp"
        "wifi_manager.cpp"
        "chat_engine.cpp"
        "display_driver.cpp"
        "chat_ui.cpp"
        "audio_io.cpp"
        "camera_driver.cpp"
        "camera_budget.cpp"
        "resource_gate.cpp"
        "sd_card.cpp"
        "system_monitor.cpp"
    INCLUDE_DIRS
        "."
        "${CMAKE_CURRENT_BINARY_DIR}"
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_http_client
        esp_lcd
        lvgl
        json
        ppm
        esp32-camera
    PRIV_REQUIRES
        driver
        esp_timer
        fatfs
        sdmmc
)

target_include_directories(${COMPONENT_LIB} PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/../../PPM/src/core"
    "${CMAKE_CURRENT_LIST_DIR}/../../PPM/src/backend")

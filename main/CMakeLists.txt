# RoboMind-S3 Main Component
idf_component_register(
    SRCS
        "main.cpp"
        "wifi_manager.cpp"
        "chat_engine.cpp"
        "display_driver.cpp"
        "chat_ui.cpp"
        "audio_io.cpp"
        # "camera_driver.cpp"   # TODO v0.1.2: uncomment when OV5640 pins confirmed
        # "sd_card.cpp"         # TODO v0.1.2: uncomment when SD card pins confirmed
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        esp_wifi
        esp_netif
        esp_http_client
        lvgl
        json
    PRIV_REQUIRES
        driver
        esp_timer
)

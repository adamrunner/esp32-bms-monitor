# For ESP-IDF components
set(srcs
    "log_serializers.cpp"
    "log_manager.cpp"
    "serial_log_sink.cpp"
    "udp_log_sink.cpp"
    "tcp_log_sink.cpp"
    "http_log_sink.cpp"
    "mqtt_log_sink.cpp"
    "sd_card_log_sink.cpp"
)

idf_component_register(
    SRCS ${srcs}
    INCLUDE_DIRS "."
    REQUIRES
        esp_wifi
        esp_netif
        log
        lwip
        main
        json
        mqtt
        fatfs
        sdmmc
        spi_flash
        status_led
        device_id
    PRIV_REQUIRES
        nvs_flash
        esp_http_client
        spiffs
)

# Compile definitions
target_compile_definitions(${COMPONENT_TARGET} PRIVATE
    INCLUDE_SERIAL_SINK=1
    INCLUDE_UDP_SINK=1
    INCLUDE_TCP_SINK=1
    INCLUDE_MQTT_SINK=1
    INCLUDE_SDCARD_SINK=1
)

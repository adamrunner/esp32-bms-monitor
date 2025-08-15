# For ESP-IDF components
set(srcs
    "log_serializers.cpp"
    "log_manager.cpp"
    "serial_log_sink.cpp"
    "udp_log_sink.cpp"
    "tcp_log_sink.cpp"
    "http_log_sink.cpp"
)

# Optional: MQTT support (requires MQTT library)
if(CONFIG_INCLUDE_MQTT_SINK)
    list(APPEND srcs "mqtt_log_sink.cpp")
endif()

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
    PRIV_REQUIRES 
        nvs_flash
        esp_http_client
)

# Compile definitions
target_compile_definitions(${COMPONENT_TARGET} PRIVATE
    INCLUDE_SERIAL_SINK=1
    INCLUDE_UDP_SINK=1
    INCLUDE_TCP_SINK=1
)

if(CONFIG_INCLUDE_MQTT_SINK)
    target_compile_definitions(${COMPONENT_TARGET} PRIVATE INCLUDE_MQTT_SINK=1)
endif()

if(CONFIG_INCLUDE_HTTP_SINK)
    target_compile_definitions(${COMPONENT_TARGET} PRIVATE INCLUDE_HTTP_SINK=1)
endif()
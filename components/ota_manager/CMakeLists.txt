idf_component_register(
    SRCS "ota_manager.c" "ota_status_logger.cpp" "ota_mqtt_publisher.c" "ota_mqtt_commands.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_https_ota esp_http_client esp_wifi nvs_flash json logging mqtt app_update esp_app_format status_led
)

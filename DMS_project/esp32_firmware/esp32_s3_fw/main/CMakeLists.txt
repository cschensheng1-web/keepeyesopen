idf_component_register(
    SRCS "dms_mqtt_contract.c" "dms_decision.c" "main.c" "ear_mar.c" "perclos.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event mqtt driver freertos esp_timer
)

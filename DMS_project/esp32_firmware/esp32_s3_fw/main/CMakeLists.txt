idf_component_register(
    SRCS "main.c" "ear_mar.c" "perclos.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event mqtt driver freertos esp_timer
)

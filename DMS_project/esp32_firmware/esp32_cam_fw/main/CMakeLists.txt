idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_wifi esp_event mqtt driver freertos esp_timer esp_camera
)

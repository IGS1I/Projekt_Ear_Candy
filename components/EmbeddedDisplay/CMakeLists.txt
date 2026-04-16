idf_component_register(SRCS "Backlight.cpp" "UIStudio.cpp"
                        INCLUDE_DIRS "."
                        REQUIRES "esp_rom" "freertos" "esp_common" "esp_driver_gpio"
                        PRIV_REQUIRES "esp_driver_ledc")
idf_component_register(SRCS "mainApp.cpp"
                        INCLUDE_DIRS "."
                        REQUIRES "AudioProcessor" "SDIO" "EmbeddedDisplay" "esp_driver_spi")
idf_component_register(SRCS "vs1053b.cpp"
                        INCLUDE_DIRS "."
                        EMBED_FILES "audio_sample.mp3"
                        REQUIRES "esp_driver_spi" "esp_driver_gpio")
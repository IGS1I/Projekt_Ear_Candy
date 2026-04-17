#ifdef __cplusplus
extern "C" {
#endif

#include "UIStudio.h"
#include "vs1053b.h"
#include "test_mp3.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "main";

void app_main(void)
{
    ESP_ERROR_CHECK(display_init());
    ESP_ERROR_CHECK(audio_processor_init());
    if (!audio_is_chip_connected()) {
        ESP_LOGE(TAG, "VS1053 not responding on SCI path");
        while (true) {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
    ESP_ERROR_CHECK(audio_force_mp3_mode());
    ESP_ERROR_CHECK(vs1053_set_volume(0x00, 0x00)); /* force max volume */
    ESP_LOGI(TAG, "Music mode: display + audio ready (SD disabled)");
    ESP_ERROR_CHECK(display_fill_screen(0x0000)); /* black */
    ESP_ERROR_CHECK(display_draw_text(8, 24, "Now Playing"));

    ESP_ERROR_CHECK(display_show_song("VS1053 Test MP3", "Embedded Header"));
    ESP_LOGI(TAG, "Playing test_mp3.h sample once...");
    ESP_ERROR_CHECK(audio_play_memory(sampleMp3, sizeof(sampleMp3)));
    ESP_LOGI(TAG, "Playback finished. staying idle.");

    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

#ifdef __cplusplus
}
#endif

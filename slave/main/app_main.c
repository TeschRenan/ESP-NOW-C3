#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/temp_sensor.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#include "esp_utils.h"
#include "esp_storage.h"
#include "espnow.h"

//Set the same Wi-Fi channel of the ESP Master
#define WIFI_CHANNEL 11

static const char *TAG = "slave-esp-now";


static void wifi_init()
{
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_wifi_set_promiscuous(1);

    ESP_ERROR_CHECK(esp_wifi_set_channel(11, WIFI_SECOND_CHAN_NONE));

    esp_wifi_set_promiscuous(0);

    ESP_ERROR_CHECK( esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );

}

static void send_temperature_task(void *arg)
{
    ESP_LOGI(TAG, "Temperature read handle task is running");

    esp_err_t ret  = ESP_OK;
    uint32_t count = 0;
    float tsens_out;
    char* temperature[8] = {0};

    espnow_frame_head_t frame_head = {
        .retransmit_count = 5,
        .broadcast        = true,
    };


    temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
    temp_sensor_get_config(&temp_sensor);
    ESP_LOGI(TAG, "default dac %d, clk_div %d", temp_sensor.dac_offset, temp_sensor.clk_div);
    temp_sensor.dac_offset = TSENS_DAC_DEFAULT; // DEFAULT: range:-10℃ ~  80℃, error < 1℃.
    temp_sensor_set_config(temp_sensor);
    temp_sensor_start();
   


    while(1){

        temp_sensor_read_celsius(&tsens_out);

        sprintf(temperature,"%.2f",tsens_out);

        ret = espnow_send(ESPNOW_TYPE_DATA, ESPNOW_ADDR_BROADCAST, temperature, sizeof(temperature), &frame_head, (5000 / portTICK_RATE_MS));
        ESP_ERROR_CONTINUE(ret != ESP_OK, "<%s> espnow_send", esp_err_to_name(ret));

        ESP_LOGI(TAG, "espnow_send, count: %d, size: %d, data: %s", count++, sizeof(temperature), temperature);

        vTaskDelay(1000 / portTICK_RATE_MS);
    }

    ESP_LOGI(TAG, "Temperature handle task is exit");

    vTaskDelete(NULL);
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_storage_init();
    esp_event_loop_create_default();

    wifi_init();

    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_config.qsize.data      = 64;
    espnow_init(&espnow_config);

    xTaskCreate(send_temperature_task, "send_temperature_task", 4 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL);

}   

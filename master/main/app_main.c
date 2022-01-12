/* Get Start Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_system.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_netif.h"

#include "esp_utils.h"
#include "esp_storage.h"
#include "espnow.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"

#define EXAMPLE_ESP_WIFI_SSID      "default"
#define EXAMPLE_ESP_WIFI_PASS      "default"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define EXAMPLE_ESP_MQTT_BROKER     "mqtt://broker.hivemq.com"
#define EXAMPLE_ESP_MQTT_USER       "" //Clean beacause is a public Broker
#define EXAMPLE_ESP_MQTT_PASS       ""

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1
#define MQTT_CONNECT_BIT   BIT2

static int s_retry_num = 0;

static EventGroupHandle_t event_group;

static const char *TAG = "master-esp-now";

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t mqttEvents(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;

    switch (event->event_id)
    {
    case MQTT_EVENT_CONNECTED:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");

        xEventGroupSetBits(event_group, MQTT_CONNECT_BIT);

        break;
    }
    case MQTT_EVENT_DISCONNECTED:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");

        xEventGroupClearBits(event_group, MQTT_CONNECT_BIT);
    
        break;
    }
    case MQTT_EVENT_SUBSCRIBED:

        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
    break;
    case MQTT_EVENT_UNSUBSCRIBED:

        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
    break;
    case MQTT_EVENT_PUBLISHED:

        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
    break;
    case MQTT_EVENT_DATA:
    {
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        ESP_LOGI(TAG,"TOPIC=%.*s", event->topic_len, event->topic);
        ESP_LOGI(TAG,"DATA=%.*s", event->data_len, event->data);
    }
    break;
    case MQTT_EVENT_ERROR:

        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:

        ESP_LOGI(TAG, "MQTT other event id: %d", event->event_id);
        break;
    }
    return ESP_OK;
}


void wifi_init_sta(void)
{
	event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
	assert(ap_netif);
	esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
	assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));

    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK( esp_wifi_set_protocol(WIFI_IF_STA, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );

    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL)); 

	wifi_config_t sta_config = { 0 };
	strcpy((char *)sta_config.sta.ssid, EXAMPLE_ESP_WIFI_SSID);
	strcpy((char *)sta_config.sta.password, EXAMPLE_ESP_WIFI_PASS);

	ESP_ERROR_CHECK( esp_wifi_set_config(ESP_IF_WIFI_STA, &sta_config));

	ESP_ERROR_CHECK( esp_wifi_start());

	ESP_ERROR_CHECK( esp_wifi_connect());

    
    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT)
    {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);

        uint8_t sta_mac[6] = {0};
		uint8_t ap_mac[6] = {0};
		esp_wifi_get_mac(ESP_IF_WIFI_STA, sta_mac);
		esp_wifi_get_mac(ESP_IF_WIFI_AP, ap_mac);

		ESP_LOGI(TAG, "ap_mac:" MACSTR ,MAC2STR(ap_mac));

    }
    else if (bits & WIFI_FAIL_BIT)
    {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } 
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
    
}

static void send_mqtt_task(void *arg)
{
    esp_err_t ret  = ESP_OK;
    uint32_t count = 0;
    char *data     = ESP_MALLOC(ESPNOW_DATA_LEN);
    size_t size   = ESPNOW_DATA_LEN;
    uint8_t addr[ESPNOW_ADDR_LEN] = {0};
    wifi_pkt_rx_ctrl_t rx_ctrl    = {0};

    char topic[64] = {0};

    uint8_t baseMac[6];
    esp_read_mac(baseMac, ESP_MAC_WIFI_STA);
    char baseMacChr[18] = {0};

    sprintf(baseMacChr, "%02X%02X%02X%02X%02X%02X", baseMac[0], baseMac[1], baseMac[2], baseMac[3], baseMac[4], baseMac[5]);

    esp_mqtt_client_config_t mqtt_cfg;
      
    memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
    mqtt_cfg.event_handle = mqttEvents;
    mqtt_cfg.keepalive = 360;
    mqtt_cfg.buffer_size = 1024;
    mqtt_cfg.uri = EXAMPLE_ESP_MQTT_BROKER;
    mqtt_cfg.username = EXAMPLE_ESP_MQTT_USER;
    mqtt_cfg.password = EXAMPLE_ESP_MQTT_PASS;

    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);

    EventBits_t bits = xEventGroupWaitBits(event_group, MQTT_CONNECT_BIT, pdFALSE, pdFALSE, (2000 / portTICK_PERIOD_MS));

    ESP_LOGI(TAG, "MQTT send task is running");

    while(1){
        ret = espnow_recv(ESPNOW_TYPE_DATA, addr, data, &size, &rx_ctrl, portMAX_DELAY);
        ESP_ERROR_CONTINUE(ret != ESP_OK, "<%s>", esp_err_to_name(ret));

        ESP_LOGI(TAG, "espnow_recv, <%d> [" MACSTR "][%d][%d][%d]: %.*s", 
                count++, MAC2STR(addr), rx_ctrl.channel, rx_ctrl.rssi, size, size, data);

        bits = xEventGroupGetBits(event_group);

        if (bits & MQTT_CONNECT_BIT)
        {
        
            snprintf(topic, 64, "%s/value", baseMacChr);
            if(esp_mqtt_client_publish(client, topic, (const char *)data, 0, 0, 0) != -1){
                
                ESP_LOGW(TAG, "MQTT Publish Success, publish topic %s, publish value %s", topic, data);

            }

        }

    }

    ESP_LOGW(TAG, "MQTT send task is exit");

    ESP_FREE(data);
    vTaskDelete(NULL);
}

void app_main()
{
    esp_log_level_set("*", ESP_LOG_INFO);

    esp_storage_init();

    wifi_init_sta();

    espnow_config_t espnow_config = ESPNOW_INIT_CONFIG_DEFAULT();
    espnow_config.qsize.data      = 64;
    espnow_init(&espnow_config);

    uint8_t primary;
    wifi_second_chan_t  second;

    esp_wifi_get_channel(&primary, &second);

    ESP_LOGE(TAG,"Wi-Fi Channel Connected %d",primary);

    xTaskCreate(send_mqtt_task, "send_mqtt_task", 4 * 1024, NULL, tskIDLE_PRIORITY + 1, NULL);

}

# ESP-NOW-C3
Using the ESP-NOW Long Range P2P with the  ESP32-C3 and publish the data in the MQTT

This example is used for ESP-NOW data communication. The device read the internal temperature port and transparently broadcasts it to the node master. 

The master device that receives the data connect to Wi-Fi and send to topic of the MQTT, "MAC/value"

Note: This repository run in anyone ESP32 family, modify this example to running another device!

## Configuration

To run this example, at least two development boards are required to test the communication between the two devices

- Modify the configuration You can modify the `app_main.c` directly to configure

In master device configure!

```c
#define EXAMPLE_ESP_WIFI_SSID      "default"
#define EXAMPLE_ESP_WIFI_PASS      "default"
#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define EXAMPLE_ESP_MQTT_BROKER     "mqtt://broker.hivemq.com"
#define EXAMPLE_ESP_MQTT_USER       ""
#define EXAMPLE_ESP_MQTT_PASS       ""

```

In slave device configure!
Set the same Wi-Fi channel of the ESP Master

```c

#define WIFI_CHANNEL 11

```
### Build and Flash firmware

```shell
$ git clone https://github.com/TeschRenan/ESP-NOW-C3
$ cd ESP-NOW-C3
$ idf.py set-target esp32C3
$ idf.py erase_flash
$ idf.py flash monitor
``` 

## Resources
- [ESP-NOW API guide](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/network/esp_now.html)
- [ESP-NOW Repository ](https://github.com/espressif/esp-now)
- [ESP-Wi-Fi API Guide ](https://docs.espressif.com/projects/esp-idf/en/latest/esp32c3/api-reference/network/esp_wifi.html)

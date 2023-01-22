#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "dht.h"
#include "nvs.h"
#include "driver/gpio.h"
#include "esp_sleep.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_http_client.h"
#include "mqtt_client.h"
#include "esp_flash_partitions.h"
#include "esp_https_ota.h"
#include "esp_ota_ops.h"

EventGroupHandle_t tasks_events;
#define TASK1_EVT (1<<0)
#define TASK2_EVT (1<<1)

QueueHandle_t counter_queue;
TaskHandle_t http_task_handle;
TaskHandle_t dht_task_handle;
TaskHandle_t counter_task_handle;
TaskHandle_t ota_task_handle;
nvs_handle_t nv_handle;


gpio_config_t gp_config = {
    .pin_bit_mask= (1<< GPIO_NUM_25),
    .mode= GPIO_MODE_INPUT,
};

gpio_config_t test_config = {
    .pin_bit_mask= (1<< GPIO_NUM_26),
    .mode= GPIO_MODE_OUTPUT,
};

typedef struct dht_data_t{
    float temp;
    float hum;
    uint8_t counter;
}dht_data_t;

#define ESP_WIFI_SSID      "CAFE AL JAZEERA"
#define ESP_WIFI_PASS      "2022@2022#"
#define ESP_MAXIMUM_RETRY  10

#define HASH_LEN 32
#define OTA_URL_SIZE 256

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static const char *TAG = "wifi station";
static int s_retry_num = 0;
int counter_mem;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;
    esp_mqtt_client_handle_t mqtt_client = event->client;
    int msg_id;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(mqtt_client, "weather_data_api", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        xTaskNotifyGive(ota_task_handle);
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

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
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

esp_http_client_config_t config_post = {
        .url = "http://196.217.65.81:5000/weather",
        .method = HTTP_METHOD_POST,
        .cert_pem = NULL,
        .event_handler = client_event_post_handler};
esp_http_client_handle_t client;



void sleep_rtc(void){
    esp_sleep_enable_timer_wakeup(60*1000000);
    esp_deep_sleep_start();
}

void simple_ota_example_task(void *pvParameters)
{   while(1){
    ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    ESP_LOGI(TAG, "Starting OTA example task");
    esp_http_client_config_t config = {
        .url = "http://196.217.65.81:5000/firmware/app.bin",
        .keep_alive_enable= true,
        .cert_pem= NULL,
    };

    esp_https_ota_config_t ota_config = {
        .http_config = &config,
    };

    ESP_LOGI(TAG, "Attempting to download update from %s", config.url);
    esp_err_t ret = esp_https_ota(&ota_config);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA Succeed, Rebooting...");
        esp_restart();
    } else {
        ESP_LOGE(TAG, "Firmware upgrade failed");
        vTaskDelay(5000/portTICK_PERIOD_MS);
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    }
}

static void print_sha256(const uint8_t *image_hash, const char *label)
{
    char hash_print[HASH_LEN * 2 + 1];
    hash_print[HASH_LEN * 2] = 0;
    for (int i = 0; i < HASH_LEN; ++i) {
        sprintf(&hash_print[i * 2], "%02x", image_hash[i]);
    }
    ESP_LOGI(TAG, "%s %s", label, hash_print);
}

static void get_sha256_of_partitions(void)
{
    uint8_t sha_256[HASH_LEN] = { 0 };
    esp_partition_t partition;

    // get sha256 digest for bootloader
    partition.address   = ESP_BOOTLOADER_OFFSET;
    partition.size      = ESP_PARTITION_TABLE_OFFSET;
    partition.type      = ESP_PARTITION_TYPE_APP;
    esp_partition_get_sha256(&partition, sha_256);
    print_sha256(sha_256, "SHA-256 for bootloader: ");

    // get sha256 digest for running partition
    esp_partition_get_sha256(esp_ota_get_running_partition(), sha_256);
    print_sha256(sha_256, "SHA-256 for current firmware: ");
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = "mqtt://mqtt.eclipseprojects.io",
    };
    esp_mqtt_client_handle_t mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(mqtt_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(mqtt_client);
}

void wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = ESP_WIFI_SSID,
            .password = ESP_WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 ESP_WIFI_SSID, ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}

void counter_task(void *pvParameters) {
    dht_data_t dht;
    dht.counter=0;
    nvs_open("storage", NVS_READWRITE, &nv_handle);
    while (1) {
        nvs_get_u8(nv_handle, "counter", &dht.counter);
        dht.counter++;
        ESP_LOGI(TAG, "Counter incremented,");
        nvs_set_u8(nv_handle, "counter", &dht.counter);
        nvs_commit(nv_handle);
        xQueueSend(counter_queue, &dht, portMAX_DELAY);
        xEventGroupSetBits(tasks_events, TASK1_EVT);
        vTaskDelay(5000 /portTICK_PERIOD_MS);
        }
    }

void dht_task(void *pvParameters) {
    dht_data_t dht;
    float temperature, humidity;
    while (1) {
        xQueueReceive(counter_queue, &dht, portMAX_DELAY);
        ESP_LOGI(TAG, "Counter received %d", dht.counter);
        if (dht_read_float_data(DHT_TYPE_DHT11, GPIO_NUM_25, &humidity, &temperature) == ESP_OK){
            dht.temp= temperature;
            dht.hum= humidity;
            ESP_LOGI(TAG, "temp received %.2f and humidity %.2f", temperature, humidity);
            xQueueSend(counter_queue, &dht, portMAX_DELAY);
            xEventGroupSetBits(tasks_events, TASK2_EVT);
            vTaskDelay(5000 /portTICK_PERIOD_MS);
        }
        else{
            ESP_LOGE(TAG, "BRO DHT FAIL !!!!!!");
        }
    }
}

void http_task(void *pvParameters) {
    dht_data_t dht_received;
    char body[70];
    while(1){
    EventBits_t bits= xEventGroupWaitBits(tasks_events, TASK1_EVT | TASK2_EVT , pdFALSE, pdTRUE, portMAX_DELAY);
    if((bits & TASK1_EVT) && (bits & TASK2_EVT)){
    xQueueReceive(counter_queue, &dht_received, portMAX_DELAY);
    sprintf(body, "{\"temp\": %.2f, \"humidity\": %.2f, \"counter\": %d}", dht_received.temp, dht_received.hum, dht_received.counter);
    esp_http_client_set_post_field(client, body, strlen(body));
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_err_t err=esp_http_client_perform(client);
    vTaskDelay(500/portTICK_PERIOD_MS);
    if (err== ESP_OK){
        ESP_LOGI(TAG, "humidity SENT %.2f and temp sent %.2f and counter sent %d", dht_received.hum, dht_received.temp, dht_received.counter);
        xEventGroupClearBits(tasks_events, TASK1_EVT | TASK2_EVT);
        vTaskDelay(5000/portTICK_PERIOD_MS);
        //sleep_rtc();
    }
    }
    }
}


void app_main(void)
{
    //Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    get_sha256_of_partitions();
    ESP_LOGI(TAG, "ESP_WIFI_MODE_STA");
    gpio_config(&gp_config);
    //gpio_config(&test_config);
    //gpio_set_level(GPIO_NUM_26, 1);
    wifi_init_sta();
    vTaskDelay(2000/ portTICK_PERIOD_MS);
    client = esp_http_client_init(&config_post);
    mqtt_app_start();
    tasks_events = xEventGroupCreate();
    counter_queue = xQueueCreate(1, sizeof(dht_data_t));
    xTaskCreate(&counter_task, "counter_task", 2048, NULL, 4, &counter_task_handle);
    xTaskCreate(&dht_task, "dht_task", 2048, NULL, 4, &dht_task_handle);
    xTaskCreate(&http_task, "http_task", 4096, NULL, 5, &http_task_handle);
    xTaskCreate(&simple_ota_example_task, "ota_example_task", 8192, NULL, 7, &ota_task_handle);
}
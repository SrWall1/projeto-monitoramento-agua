#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "driver/adc.h"
#include "mqtt_client.h"

#define LED_PIN GPIO_NUM_2
#define PH_PIN ADC1_CHANNEL_0      // GPIO 36
#define TURBIDITY_PIN ADC1_CHANNEL_3 // GPIO 39
#define PH_MIN_SAFE 6.5
#define PH_MAX_SAFE 8.5
#define TURBIDITY_MAX_SAFE 5.0


#define WIFI_SSID      "Wokwi-GUEST"
#define WIFI_PASS      ""
#define MQTT_BROKER_URL "mqtt://test.mosquitto.org"


#define MQTT_TOPIC_PH            "agua/sim/idf/ph"
#define MQTT_TOPIC_TURBIDITY     "agua/sim/idf/turbidez"
#define MQTT_TOPIC_VALVE_STATUS  "agua/sim/idf/valvula/status"

static const char *TAG = "WATER_MONITOR";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

esp_mqtt_client_handle_t client; 


long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}


static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGI(TAG, "Falha ao conectar ao Wi-Fi. Tentando novamente...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP recebido:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
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
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    // Espera até que a conexão seja estabelecida ou falhe
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado ao Wi-Fi SSID:%s", WIFI_SSID);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Falha ao conectar ao SSID:%s", WIFI_SSID);
    } else {
        ESP_LOGE(TAG, "EVENTO INESPERADO");
    }
}


static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
    };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "Cliente MQTT iniciado.");
}


void app_main(void)
{
    // Inicialização necessária para o Wi-Fi
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, ">>> INICIANDO SISTEMA DE CONTROLE COMPLETO (ESP-IDF) <<<");

    wifi_init_sta();
    mqtt_app_start();

    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(PH_PIN, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(TURBIDITY_PIN, ADC_ATTEN_DB_11);
    
    ESP_LOGI(TAG, "Setup concluído. Iniciando loop principal.");

    while(1) {
        printf("\n-------------------------------------\n");
        int ph_raw = adc1_get_raw(PH_PIN);
        int turbidity_raw = adc1_get_raw(TURBIDITY_PIN);
        float phValue = (float)map(ph_raw, 0, 4095, 0, 1400) / 100.0;
        float turbidityValue = (float)map(turbidity_raw, 0, 4095, 0, 10000) / 100.0;
        bool isSafe = (phValue >= PH_MIN_SAFE && phValue <= PH_MAX_SAFE) && (turbidityValue <= TURBIDITY_MAX_SAFE);

        printf("Leituras: pH = %.2f | Turbidez = %.2f NTU\n", phValue, turbidityValue);

  
        if (isSafe) {
            gpio_set_level(LED_PIN, 0); // Desliga LED
        } else {
            gpio_set_level(LED_PIN, 1); // Liga LED
        }
  
        char buffer[10];
        sprintf(buffer, "%.2f", phValue);
        esp_mqtt_client_publish(client, MQTT_TOPIC_PH, buffer, 0, 1, 0);
        
        sprintf(buffer, "%.2f", turbidityValue);
        esp_mqtt_client_publish(client, MQTT_TOPIC_TURBIDITY, buffer, 0, 1, 0);

        esp_mqtt_client_publish(client, MQTT_TOPIC_VALVE_STATUS, isSafe ? "ABERTA" : "FECHADA", 0, 1, 0);
        printf("Dados publicados via MQTT.\n");

        vTaskDelay(10000 / portTICK_PERIOD_MS); // Aguarda 10 segundos
    }
}

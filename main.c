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
#include "mqtt_client.h"
#include "esp_random.h" // NOVO: Biblioteca para gerar números aleatórios

// --- Pinos ---
#define LED_PIN GPIO_NUM_2

// --- Limites de Qualidade ---
#define PH_MIN_SAFE 6.5
#define PH_MAX_SAFE 8.5
#define TURBIDITY_MAX_SAFE 5.0

// --- Configurações de Rede ---
#define WIFI_SSID      "Wokwi-GUEST"
#define WIFI_PASS      ""
#define MQTT_BROKER_URL "mqtt://test.mosquitto.org"

// --- Tópicos MQTT ---
#define MQTT_TOPIC_PH            "agua/sim/idf/ph"
#define MQTT_TOPIC_TURBIDITY     "agua/sim/idf/turbidez"
#define MQTT_TOPIC_VALVE_STATUS  "agua/sim/idf/valvula/status"

static const char *TAG = "WATER_MONITOR_RANDOM";
static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

esp_mqtt_client_handle_t client; 

// --- Funções de Rede (sem mudanças) ---
static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) { esp_wifi_connect(); } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        ESP_LOGI(TAG, "Falha ao conectar ao Wi-Fi. Tentando novamente...");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "IP recebido:" IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}
void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));
    wifi_config_t wifi_config = { .sta = { .ssid = WIFI_SSID, .password = WIFI_PASS, }, };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, portMAX_DELAY);
    if (bits & WIFI_CONNECTED_BIT) { ESP_LOGI(TAG, "Conectado ao Wi-Fi SSID:%s", WIFI_SSID); } 
    else if (bits & WIFI_FAIL_BIT) { ESP_LOGI(TAG, "Falha ao conectar ao SSID:%s", WIFI_SSID); } 
    else { ESP_LOGE(TAG, "EVENTO INESPERADO"); }
}
static void mqtt_app_start(void) {
    esp_mqtt_client_config_t mqtt_cfg = { .broker.address.uri = MQTT_BROKER_URL, };
    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
    ESP_LOGI(TAG, "Cliente MQTT iniciado.");
}


void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_LOGI(TAG, ">>> INICIANDO SISTEMA COM DADOS ALEATÓRIOS <<<");

    wifi_init_sta();
    mqtt_app_start();
    
    gpio_reset_pin(LED_PIN);
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    
    ESP_LOGI(TAG, "Setup concluído. Iniciando loop principal.");

    while(1) {
        printf("\n-------------------------------------\n");

        // MUDANÇA: Geração de valores aleatórios
        // Gera um pH aleatório entre 4.00 e 10.00
        float phValue = 4.0 + (float)(esp_random() % 601) / 100.0; 
        // Gera uma turbidez aleatória entre 0.00 e 40.00
        float turbidityValue = (float)(esp_random() % 4001) / 100.0;
        
        // Lógica de segurança original, mas agora com os dados aleatórios
        bool isSafe = (phValue >= PH_MIN_SAFE && phValue <= PH_MAX_SAFE) && (turbidityValue <= TURBIDITY_MAX_SAFE);

        printf("Valores Aleatórios Gerados: pH = %.2f | Turbidez = %.2f NTU\n", phValue, turbidityValue);

        // Controla o LED com base nos valores aleatórios
        if (isSafe) {
            gpio_set_level(LED_PIN, 0); // Desliga LED
        } else {
            gpio_set_level(LED_PIN, 1); // Liga LED
        }
        
        // Publica os dados via MQTT
        char buffer[10];
        sprintf(buffer, "%.2f", phValue);
        esp_mqtt_client_publish(client, MQTT_TOPIC_PH, buffer, 0, 1, 0);
        
        sprintf(buffer, "%.2f", turbidityValue);
        esp_mqtt_client_publish(client, MQTT_TOPIC_TURBIDITY, buffer, 0, 1, 0);

        esp_mqtt_client_publish(client, MQTT_TOPIC_VALVE_STATUS, isSafe ? "ABERTA" : "FECHADA", 0, 1, 0);
        printf("Dados publicados via MQTT. Válvula: %s\n", isSafe ? "ABERTA" : "FECHADA");

        vTaskDelay(5000 / portTICK_PERIOD_MS); // Aguarda 5 segundos para o próximo ciclo
    }
}

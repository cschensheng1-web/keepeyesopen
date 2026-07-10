/**
 * ============================================
 * 📡 WiFi + MQTT 连接测试
 * 覃晖测硬件用：TEST_MODE = 4
 * ============================================
 * 预期：连 WiFi → 连 MQTT Broker → 发一条测试 JSON
 * 注意：改 config.h 里的 WIFI_SSID/WIFI_PASSWORD/MQTT_BROKER_URL
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "config.h"

static const char *TAG = "MQTT";
static esp_mqtt_client_handle_t mqtt_client = NULL;

// ── FreeRTOS 事件标志位 ──
#define WIFI_CONNECTED_BIT  BIT0
#define MQTT_CONNECTED_BIT  BIT1
static EventGroupHandle_t evt_group;

// ── WiFi 事件回调 ──
static void wifi_event_handler(void *arg,
    esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        ESP_LOGI(TAG, "📶 WiFi 开始连接...");
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        ESP_LOGW(TAG, "⚠️ WiFi 断连，自动重连...");
        xEventGroupClearBits(evt_group, WIFI_CONNECTED_BIT);
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *evt = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "✅ WiFi 已连接！IP: " IPSTR, IP2STR(&evt->ip_info.ip));
        xEventGroupSetBits(evt_group, WIFI_CONNECTED_BIT);
    }
}

// ── MQTT 事件回调 ──
static void mqtt_event_handler(void *arg, esp_event_base_t base,
    int32_t id, void *data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)data;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "✅ MQTT 已连接 Broker！");
            xEventGroupSetBits(evt_group, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "⚠️ MQTT 断连");
            xEventGroupClearBits(evt_group, MQTT_CONNECTED_BIT);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "  📤 消息已发送 (msg_id=%d)", event->msg_id);
            break;
        default:
            break;
    }
}

// ── WiFi 初始化 ──
static void wifi_init(void)
{
    evt_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
        &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
        &wifi_event_handler, NULL));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid     = WIFI_SSID,
            .password = WIFI_PASSWORD,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
}

// ── MQTT 初始化 ──
static void mqtt_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URL,
    };
    mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    ESP_ERROR_CHECK(esp_mqtt_client_register_event(mqtt_client,
        ESP_EVENT_ANY_ID, mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(mqtt_client));
}

void wifi_mqtt_test(void)
{
    ESP_LOGI(TAG, "📡 WiFi + MQTT 连接测试开始");
    ESP_LOGI(TAG, "   SSID: %s", WIFI_SSID);
    ESP_LOGI(TAG, "   Broker: %s", MQTT_BROKER_URL);

    wifi_init();

    // 等待 WiFi 连接（最多 20 秒）
    EventBits_t bits = xEventGroupWaitBits(evt_group, WIFI_CONNECTED_BIT,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(20000));
    if (!(bits & WIFI_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "❌ WiFi 连接超时！检查 SSID/密码");
        return;
    }

    mqtt_init();

    // 等待 MQTT 连接（最多 10 秒）
    bits = xEventGroupWaitBits(evt_group, MQTT_CONNECTED_BIT,
        pdFALSE, pdTRUE, pdMS_TO_TICKS(10000));
    if (!(bits & MQTT_CONNECTED_BIT)) {
        ESP_LOGE(TAG, "❌ MQTT 连接超时！检查 Broker 地址和网络");
        return;
    }

    // 发送测试消息
    const char *test_msg = "{\"device_id\":\"ESP32_DMS_001\","
                           "\"test\":true,"
                           "\"msg\":\"MQTT硬件测试成功\"}";
    int msg_id = esp_mqtt_client_publish(mqtt_client, MQTT_TOPIC,
        test_msg, 0, 1, 0);
    ESP_LOGI(TAG, "📤 已发送测试消息 (msg_id=%d)", msg_id);
    ESP_LOGI(TAG, "   Topic: %s", MQTT_TOPIC);
    ESP_LOGI(TAG, "   Payload: %s", test_msg);
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "✅ MQTT 测试完成！在队长电脑上用 mosquitto_sub 验证：");
    ESP_LOGI(TAG, "   mosquitto_sub -h <broker_ip> -t \"dms/car/data\"");

    // 保持连接 10 秒让消息发完
    vTaskDelay(pdMS_TO_TICKS(10000));
    esp_mqtt_client_stop(mqtt_client);
    esp_mqtt_client_destroy(mqtt_client);
    esp_wifi_stop();
    esp_wifi_deinit();
}

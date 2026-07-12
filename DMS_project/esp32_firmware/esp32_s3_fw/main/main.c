/**
 * ESP32-S3 算力板固件
 * UART收CAM数据 → EAR/MAR → PERCLOS → I2S告警 + MQTT上报 + IR LED
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "config.h"
#include "ear_mar.h"
#include "perclos.h"

static const char *TAG = "S3";
static i2s_chan_handle_t i2s_h = NULL;
static esp_mqtt_client_handle_t mqtt = NULL;

// ── I2S音频 ──
static void audio_init(void) {
    i2s_chan_config_t cc = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
    i2s_new_channel(&cc, &i2s_h, NULL);
    i2s_std_config_t sc = {
        .clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
        .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=I2S_BCLK_PIN,.ws=I2S_LRC_PIN,.dout=I2S_DIN_PIN,.din=I2S_GPIO_UNUSED}
    };
    i2s_channel_init_std_mode(i2s_h, &sc);
    i2s_channel_enable(i2s_h);
    gpio_set_direction(I2S_SD_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(I2S_SD_PIN, 1);
    ESP_LOGI(TAG, "I2S OK");
}
static void audio_beep(int hz, int ms) {
    if(!i2s_h) return;
    int n=16000*ms/1000;
    int16_t *b=malloc(n*2);
    for(int i=0;i<n;i++) b[i]=(int16_t)(6000*sinf(2*M_PI*hz*i/16000.0f));
    size_t w; i2s_channel_write(i2s_h,b,n*2,&w,1000);
    free(b);
}

// ── 外设测试 ──
static void test_uart_rx(void) {
    ESP_LOGI(TAG,"UART RX test on GPIO%d...",UART_RX_PIN);
    uart_config_t uc={.baud_rate=UART_BAUD,.data_bits=UART_DATA_8_BITS,.parity=UART_PARITY_DISABLE,.stop_bits=UART_STOP_BITS_1,.flow_ctrl=UART_HW_FLOWCTRL_DISABLE};
    uart_driver_install(UART_NUM,4096,0,0,NULL,0);
    uart_param_config(UART_NUM,&uc);
    uart_set_pin(UART_NUM,UART_TX_PIN,UART_RX_PIN,UART_PIN_NO_CHANGE,UART_PIN_NO_CHANGE);
    uint8_t buf[512]; int cnt=0;
    while(1){
        int len=uart_read_bytes(UART_NUM,buf,sizeof(buf)-1,pdMS_TO_TICKS(500));
        if(len>0){ buf[len]=0; cnt++; if(cnt%10==0) ESP_LOGI(TAG,"收到%d包 最新%d字节",cnt,len); }
    }
}
static void test_i2s(void) {
    audio_init();
    ESP_LOGI(TAG,"播放测试音...");
    audio_beep(440,500); vTaskDelay(300);
    audio_beep(660,500); vTaskDelay(300);
    audio_beep(880,500);
    ESP_LOGI(TAG,"I2S测试完成");
}
static void test_ir_led(void) {
    ledc_timer_config_t t={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,.timer_num=LEDC_TIMER_0,.freq_hz=5000,.clk_cfg=LEDC_AUTO_CLK};
    ledc_timer_config(&t);
    ledc_channel_config_t c={.gpio_num=IR_LED_PIN,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=0};
    ledc_channel_config(&c);
    ESP_LOGI(TAG,"IR LED渐亮...");
    for(int d=0;d<=1023;d+=64){ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,d);ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);vTaskDelay(30);}
    vTaskDelay(2000);
    ledc_set_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0,0);ledc_update_duty(LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_0);
    ESP_LOGI(TAG,"IR LED测试完成");
}

// ── WiFi/MQTT测试 ──
static EventGroupHandle_t evt;
static void wifi_cb(void*a,esp_event_base_t b,int32_t id,void*d){
    if(b==WIFI_EVENT&&id==WIFI_EVENT_STA_START) esp_wifi_connect();
    else if(b==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){ ip_event_got_ip_t*e=d; ESP_LOGI(TAG,"WiFi OK IP:"IPSTR,IP2STR(&e->ip_info.ip)); xEventGroupSetBits(evt,1); }
}
static void mqtt_cb(void*a,esp_event_base_t b,int32_t id,void*d){
    if(id==MQTT_EVENT_CONNECTED){ESP_LOGI(TAG,"MQTT OK");xEventGroupSetBits(evt,2);}
    else if(id==MQTT_EVENT_PUBLISHED) ESP_LOGI(TAG,"MQTT sent");
}
static void test_mqtt(void) {
    evt=xEventGroupCreate();
    nvs_flash_init(); esp_netif_init(); esp_event_loop_create_default(); esp_netif_create_default_wifi_sta();
    wifi_init_config_t wc=WIFI_INIT_CONFIG_DEFAULT(); esp_wifi_init(&wc);
    esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,wifi_cb,NULL);
    esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wifi_cb,NULL);
    wifi_config_t wcfg={.sta={.ssid=WIFI_SSID,.password=WIFI_PASSWORD}};
    esp_wifi_set_mode(WIFI_MODE_STA); esp_wifi_set_config(WIFI_IF_STA,&wcfg); esp_wifi_start();
    if(!(xEventGroupWaitBits(evt,1,pdTRUE,pdTRUE,20000)&1)){ESP_LOGE(TAG,"WiFi失败");return;}
    esp_mqtt_client_config_t mc={.broker.address.uri=MQTT_BROKER_URL};
    mqtt=esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(mqtt,ESP_EVENT_ANY_ID,mqtt_cb,NULL);
    esp_mqtt_client_start(mqtt);
    if(!(xEventGroupWaitBits(evt,2,pdTRUE,pdTRUE,10000)&2)){ESP_LOGE(TAG,"MQTT失败");return;}
    esp_mqtt_client_publish(mqtt,MQTT_TOPIC,"{\"test\":\"ESP32-S3 MQTT OK\"}",0,1,0);
    ESP_LOGI(TAG,"MQTT测试完成, 10秒后退出"); vTaskDelay(10000);
}

void app_main(void) {
    gpio_set_direction(STATUS_LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(STATUS_LED_PIN, 1);
    ESP_LOGI(TAG,"ESP32-S3 DMS 算力板 TEST_MODE=%d", TEST_MODE);
    switch(TEST_MODE){
        case 1: test_uart_rx(); break;
        case 2: test_i2s(); break;
        case 3: test_ir_led(); break;
        case 4: test_mqtt(); break;
        case 5: test_uart_rx(); break; // TODO:综合模式
        default: ESP_LOGW(TAG,"未知模式");
    }
}

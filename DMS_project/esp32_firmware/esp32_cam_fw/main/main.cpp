/**
 * ESP32-CAM —— 只做一件事：拍照 → MQTT 发 JPEG
 * 全部算法在 PC 端跑
 */
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "config.h"

static const char *TAG = "CAM";
static esp_mqtt_client_handle_t mq = NULL;

static camera_config_t cam_cfg = {
    .pin_pwdn=CAM_PIN_PWDN,.pin_reset=CAM_PIN_RESET,.pin_xclk=CAM_PIN_XCLK,
    .pin_sccb_sda=CAM_PIN_SIOD,.pin_sccb_scl=CAM_PIN_SIOC,
    .pin_d7=CAM_PIN_D7,.pin_d6=CAM_PIN_D6,.pin_d5=CAM_PIN_D5,.pin_d4=CAM_PIN_D4,
    .pin_d3=CAM_PIN_D3,.pin_d2=CAM_PIN_D2,.pin_d1=CAM_PIN_D1,.pin_d0=CAM_PIN_D0,
    .pin_vsync=CAM_PIN_VSYNC,.pin_href=CAM_PIN_HREF,.pin_pclk=CAM_PIN_PCLK,
    .xclk_freq_hz=20000000,.ledc_timer=LEDC_TIMER_0,.ledc_channel=LEDC_CHANNEL_0,
    .pixel_format=PIXFORMAT_JPEG,.frame_size=FRAMESIZE_QVGA,
    .jpeg_quality=10,.fb_count=2,.grab_mode=CAMERA_GRAB_WHEN_EMPTY,
};

static EventGroupHandle_t evt;
static void wf_cb(void*a,esp_event_base_t b,int32_t id,void*d){
    if(b==WIFI_EVENT&&id==WIFI_EVENT_STA_START) esp_wifi_connect();
    else if(b==IP_EVENT&&id==IP_EVENT_STA_GOT_IP){
        ip_event_got_ip_t*ip=(ip_event_got_ip_t*)d;
        ESP_LOGI(TAG,"WiFi:"IPSTR,IP2STR(&ip->ip_info.ip)); xEventGroupSetBits(evt,1);
    }
}
static void mq_ok(void*a,esp_event_base_t b,int32_t id,void*d){
    ESP_LOGI(TAG,"MQTT OK");
}

extern "C" void app_main(void){
    ESP_LOGI(TAG,"=== CAM Edge Capture ===");
    if(esp_camera_init(&cam_cfg)!=ESP_OK){ESP_LOGE(TAG,"CAM FAIL");while(1)vTaskDelay(1000);}
    sensor_t *s=esp_camera_sensor_get(); s->set_vflip(s,1);
    ESP_LOGI(TAG,"OV2640 OK PID=0x%02X",s->id.PID);

    // 不闪灯——驾驶安全
    nvs_flash_init();esp_netif_init();esp_event_loop_create_default();esp_netif_create_default_wifi_sta();
    wifi_init_config_t wc=WIFI_INIT_CONFIG_DEFAULT();esp_wifi_init(&wc);
    esp_event_handler_instance_register(WIFI_EVENT,WIFI_EVENT_STA_START,wf_cb,NULL,NULL);
    evt=xEventGroupCreate();
    esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wf_cb,NULL,NULL);
    wifi_config_t w={.sta={.ssid=WIFI_SSID,.password=WIFI_PASSWORD}};
    esp_wifi_set_mode(WIFI_MODE_STA);esp_wifi_set_config(WIFI_IF_STA,&w);esp_wifi_start();
    xEventGroupWaitBits(evt,1,pdTRUE,pdTRUE,20000);

    esp_mqtt_client_config_t mc = {};
    mc.broker.address.uri = MQTT_BROKER_URL;
    mq=esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(mq,MQTT_EVENT_CONNECTED,mq_ok,NULL);
    esp_mqtt_client_start(mq);

    ESP_LOGI(TAG,"Capturing...");
    uint32_t fn=0;
    while(1){
        camera_fb_t *fb=esp_camera_fb_get();
        if(!fb){vTaskDelay(10);continue;}
        fn++;
        /* 只发 JPEG 到 MQTT，不做任何处理 */
        esp_mqtt_client_publish(mq,"dms/cam/img",(char*)fb->buf,fb->len,0,0);
        if(fn%25==0) ESP_LOGI(TAG,"#%lu %zuB",fn,fb->len);
        esp_camera_fb_return(fb);
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

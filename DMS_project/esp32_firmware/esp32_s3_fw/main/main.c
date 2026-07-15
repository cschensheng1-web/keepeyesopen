/**
 * ESP32-S3 —— OLED大眼动画 + I2S语音 + MQTT
 */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "mqtt_client.h"
#include "config.h"
#include "alert_sleep.h"
#include "alert_yawn.h"
#include "alert_wake.h"
#include "alert_rest.h"
#include "alert_safe.h"

static const char *TAG = "S3";
static i2s_chan_handle_t i2s_h;
static i2c_master_dev_handle_t oled;
static uint32_t last_alert = 0;
static uint8_t fb[1024]; // 128x64/8, 静态

#define OLED_ADDR 0x3C

static void o_cmd(uint8_t c){ uint8_t b[]={0x00,c}; i2c_master_transmit(oled,b,2,-1); }
static void o_dat(uint8_t *d,int n){ uint8_t *buf=malloc(n+1); buf[0]=0x40; memcpy(buf+1,d,n); i2c_master_transmit(oled,buf,n+1,-1); free(buf); }
static void fb_set(int x,int y,bool on){
    if(x<0||x>=128||y<0||y>=64) return;
    if(on) fb[x+(y>>3)*128] |= (1<<(y&7));
    else   fb[x+(y>>3)*128] &= ~(1<<(y&7));
}
static void fb_circle(int cx,int cy,int r,bool on){
    for(int y=-r;y<=r;y++) for(int x=-r;x<=r;x++)
        if(x*x+y*y<=r*r) fb_set(cx+x,cy+y,on);
}
static void fb_hline(int x1,int x2,int y,bool on){
    for(int x=x1;x<=x2;x++) fb_set(x,y,on);
}
static void fb_flush(void){
    for(int p=0;p<8;p++){
        o_cmd(0xB0+p); o_cmd(0x00); o_cmd(0x10);
        o_dat(fb+p*128,128);
    }
}

static void oled_init(void){
    i2c_master_bus_config_t bc={.clk_source=I2C_CLK_SRC_DEFAULT,.i2c_port=I2C_NUM_0,
        .scl_io_num=9,.sda_io_num=8,.glitch_ignore_cnt=7,.flags={.enable_internal_pullup=true}};
    i2c_master_bus_handle_t bus; i2c_new_master_bus(&bc,&bus);
    i2c_device_config_t dc={.dev_addr_length=I2C_ADDR_BIT_LEN_7,.device_address=OLED_ADDR,.scl_speed_hz=400000};
    i2c_master_bus_add_device(bus,&dc,&oled);
    uint8_t init[]={0xAE,0xD5,0x80,0xA8,0x3F,0xD3,0x00,0x40,0x8D,0x14,0x20,0x00,0xA1,0xC8,0xDA,0x12,0x81,0xCF,0xD9,0xF1,0xDB,0x40,0xA4,0xA6,0xAF};
    for(int i=0;i<sizeof(init);i++) o_cmd(init[i]);
    memset(fb,0,sizeof(fb)); fb_flush();
    ESP_LOGI(TAG,"OLED OK");
}

/* 大眼：左眼中心(33,28) 右眼(94,28) 半径18 */
static void draw_eyes(bool closed, int look_x, int look_y){
    memset(fb,0,sizeof(fb));
    int lx=33, rx=94, ey=28, r=18, pr=5;
    if(closed){
        // 闭眼：两条粗横线 + 睫毛
        fb_hline(lx-r,lx+r,ey,true);   fb_hline(lx-r,lx+r,ey+1,true);
        fb_hline(rx-r,rx+r,ey,true);   fb_hline(rx-r,rx+r,ey+1,true);
    }else{
        // 眼白
        fb_circle(lx,ey,r,true);
        fb_circle(rx,ey,r,true);
        // 瞳孔
        fb_circle(lx+look_x,ey+look_y,pr,false);
        fb_circle(rx+look_x,ey+look_y,pr,false);
        // 高光
        fb_set(lx-5,ey-8,false); fb_set(lx-4,ey-8,false);
        fb_set(rx-5,ey-8,false); fb_set(rx-4,ey-8,false);
    }
    fb_flush();
}

/* I2S */
static void audio_init(void){
    i2s_chan_config_t cc=I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0,I2S_ROLE_MASTER);
    i2s_new_channel(&cc,&i2s_h,NULL);
    i2s_std_config_t sc={.clk_cfg=I2S_STD_CLK_DEFAULT_CONFIG(16000),
        .slot_cfg=I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO),
        .gpio_cfg={.mclk=I2S_GPIO_UNUSED,.bclk=I2S_BCLK_PIN,.ws=I2S_LRC_PIN,.dout=I2S_DIN_PIN,.din=I2S_GPIO_UNUSED}};
    i2s_channel_init_std_mode(i2s_h,&sc); i2s_channel_enable(i2s_h);
    gpio_set_direction(I2S_SD_PIN,GPIO_MODE_OUTPUT); gpio_set_level(I2S_SD_PIN,1);
}
static void beep(int hz,int ms){
    int n=16000*ms/1000; int16_t*b=malloc(n*2);
    for(int i=0;i<n;i++) b[i]=(int16_t)(6000*sinf(2*M_PI*hz*i/16000.0f));
    size_t w; i2s_channel_write(i2s_h,b,n*2,&w,1000); free(b);
}
static void play_voice(const uint8_t*data,int len){
    draw_eyes(true,0,0); beep(880,100); vTaskDelay(300);
    draw_eyes(false,0,0);
    size_t w; i2s_channel_write(i2s_h,data,len,&w,portMAX_DELAY);
    draw_eyes(true,0,0); vTaskDelay(150);
    draw_eyes(false,0,0);
}

/* WiFi/MQTT */
static EventGroupHandle_t evt;
static void wf_s(void*a,esp_event_base_t b,int32_t id,void*d){esp_wifi_connect();}
static void wf_ip(void*a,esp_event_base_t b,int32_t id,void*d){
    ip_event_got_ip_t*ip=d; ESP_LOGI(TAG,"WiFi:"IPSTR,IP2STR(&ip->ip_info.ip)); xEventGroupSetBits(evt,1);
}
static void mq_ok(void*a,esp_event_base_t b,int32_t id,void*d){
    ESP_LOGI(TAG,"MQTT OK"); esp_mqtt_client_subscribe((esp_mqtt_client_handle_t)a,MQTT_TOPIC,0);
}
static void mq_data(void*a,esp_event_base_t b,int32_t id,void*d){
    esp_mqtt_event_handle_t e=(esp_mqtt_event_handle_t)d;
    char buf[256]={0}; memcpy(buf,e->data,e->data_len<255?e->data_len:255);
    int level=0; char*lp=strstr(buf,"\"fatigue_level\":"); if(lp) level=atoi(lp+16);
    uint32_t now=xTaskGetTickCount()*portTICK_PERIOD_MS;
    if(level>=2&&(now-last_alert)>COOLDOWN_MS){
        last_alert=now; ESP_LOGW(TAG,"FATIGUE L%d",level);
        static int si=0,yi=0;
        if(level==3||!strstr(buf,"Yawning")){
            int i=si++%3; const uint8_t *d; int l;
            if(i==0){d=alert_sleep;l=ALERT_SLEEP_LEN;}
            else if(i==1){d=alert_wake;l=ALERT_WAKE_LEN;}
            else{d=alert_safe;l=ALERT_SAFE_LEN;}
            play_voice(d,l);
        }else{
            int i=yi++%2;
            if(i==0) play_voice(alert_yawn,ALERT_YAWN_LEN);
            else play_voice(alert_rest,ALERT_REST_LEN);
        }
    }
}

void app_main(void){
    gpio_set_direction(STATUS_LED_PIN,GPIO_MODE_OUTPUT); gpio_set_level(STATUS_LED_PIN,1);
    ESP_LOGI(TAG,"=== S3 ===");
    oled_init();
    audio_init();
    ledc_timer_config_t lt={.speed_mode=LEDC_LOW_SPEED_MODE,.duty_resolution=LEDC_TIMER_10_BIT,.timer_num=LEDC_TIMER_0,.freq_hz=5000,.clk_cfg=LEDC_AUTO_CLK};
    ledc_timer_config(&lt);
    ledc_channel_config_t lc={.gpio_num=IR_LED_PIN,.speed_mode=LEDC_LOW_SPEED_MODE,.channel=LEDC_CHANNEL_0,.timer_sel=LEDC_TIMER_0,.duty=800};
    ledc_channel_config(&lc);
    nvs_flash_init();esp_netif_init();esp_event_loop_create_default();esp_netif_create_default_wifi_sta();
    wifi_init_config_t wc=WIFI_INIT_CONFIG_DEFAULT();esp_wifi_init(&wc);
    esp_event_handler_instance_register(WIFI_EVENT,WIFI_EVENT_STA_START,wf_s,NULL,NULL);
    evt=xEventGroupCreate();
    esp_event_handler_instance_register(IP_EVENT,IP_EVENT_STA_GOT_IP,wf_ip,NULL,NULL);
    wifi_config_t w={.sta={.ssid=WIFI_SSID,.password=WIFI_PASSWORD}};
    esp_wifi_set_mode(WIFI_MODE_STA);esp_wifi_set_config(WIFI_IF_STA,&w);esp_wifi_start();
    xEventGroupWaitBits(evt,1,pdTRUE,pdTRUE,20000);
    esp_mqtt_client_config_t mc={}; mc.broker.address.uri=MQTT_BROKER_URL;
    esp_mqtt_client_handle_t mq=esp_mqtt_client_init(&mc);
    esp_mqtt_client_register_event(mq,MQTT_EVENT_CONNECTED,mq_ok,mq);
    esp_mqtt_client_register_event(mq,MQTT_EVENT_DATA,mq_data,NULL);
    esp_mqtt_client_start(mq);
    ESP_LOGI(TAG,"Ready");
    draw_eyes(false,0,0);
    int cnt=0, look_x=0, look_y=0;
    while(1){
        cnt++;
        bool blink=(cnt%60<2);
        if(cnt%30==0){look_x=(look_x+1)%3-1; look_y=(look_y+1)%3-1;}
        draw_eyes(blink,look_x,look_y);
        vTaskDelay(pdMS_TO_TICKS(50));
        gpio_set_level(STATUS_LED_PIN,cnt%20<2);
    }
}

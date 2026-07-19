#include "alert_sleep.h"
static void play_alert_sleep(i2s_chan_handle_t h) {
    size_t w;
    i2s_channel_write(h, alert_sleep, alert_sleep_LEN, &w, portMAX_DELAY);
}

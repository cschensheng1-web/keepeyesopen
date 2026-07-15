#include "alert_safe.h"
static void play_alert_safe(i2s_chan_handle_t h) {
    size_t w;
    i2s_channel_write(h, alert_safe, alert_safe_LEN, &w, portMAX_DELAY);
}

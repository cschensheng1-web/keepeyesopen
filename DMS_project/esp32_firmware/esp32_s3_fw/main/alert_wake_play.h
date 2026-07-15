#include "alert_wake.h"
static void play_alert_wake(i2s_chan_handle_t h) {
    size_t w;
    i2s_channel_write(h, alert_wake, alert_wake_LEN, &w, portMAX_DELAY);
}

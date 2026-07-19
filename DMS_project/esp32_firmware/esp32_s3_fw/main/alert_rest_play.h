#include "alert_rest.h"
static void play_alert_rest(i2s_chan_handle_t h) {
    size_t w;
    i2s_channel_write(h, alert_rest, alert_rest_LEN, &w, portMAX_DELAY);
}

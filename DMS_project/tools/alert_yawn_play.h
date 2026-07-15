#include "alert_yawn.h"
static void play_alert_yawn(i2s_chan_handle_t h) {
    size_t w;
    i2s_channel_write(h, alert_yawn, alert_yawn_LEN, &w, portMAX_DELAY);
}

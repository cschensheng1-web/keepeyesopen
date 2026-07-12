#include "perclos.h"
#include "config.h"

void perclos_update(float ear, float mar, uint32_t now, perclos_t *s) {
    if (ear < EAR_THRESHOLD) {
        if (!s->blink_ms) s->blink_ms = now;
        else {
            uint32_t d = now - s->blink_ms;
            if (d >= BLINK_DEEP_SLEEP_MS) { s->level=LV_3_SLEEP; s->desc="LEVEL3:Deep Sleep"; }
            else if (d >= BLINK_MICRO_SLEEP_MS) { s->level=LV_2_MICRO; s->desc="LEVEL2:Micro-sleep"; }
        }
    } else s->blink_ms = 0;

    if (mar > MAR_THRESHOLD) {
        if (!s->yawn_ms) s->yawn_ms = now;
        else if (now - s->yawn_ms >= YAWN_DURATION_MS) { s->level=LV_2_YAWN; s->desc="LEVEL2:Yawning"; }
    } else {
        if (s->yawn_ms && now - s->yawn_ms > 1000) s->yawn_ms = 0;
    }
}
void perclos_reset(perclos_t *s) {
    s->blink_ms=s->yawn_ms=0; s->level=LV_NORMAL; s->desc="Normal";
}

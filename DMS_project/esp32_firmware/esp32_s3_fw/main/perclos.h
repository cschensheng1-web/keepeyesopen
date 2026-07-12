#ifndef PERCLOS_H
#define PERCLOS_H
#include <stdint.h>

typedef enum { LV_NORMAL=0, LV_2_MICRO=2, LV_2_YAWN=2, LV_3_SLEEP=3 } level_t;

typedef struct {
    uint32_t blink_ms, yawn_ms;
    level_t level;
    const char *desc;
} perclos_t;

void perclos_update(float ear, float mar, uint32_t now_ms, perclos_t *s);
void perclos_reset(perclos_t *s);
#endif

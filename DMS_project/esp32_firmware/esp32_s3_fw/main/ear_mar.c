#include <math.h>
#include "ear_mar.h"

static float dist(point_t a, point_t b) {
    float dx=a.x-b.x, dy=a.y-b.y;
    return sqrtf(dx*dx+dy*dy);
}
static float ear_single(point_t e[6]) {
    float n=dist(e[1],e[5])+dist(e[2],e[4]);
    float d=2.0f*dist(e[0],e[3]);
    return (d<1e-6f)?1.0f:n/d;
}
void dms_compute_ear_mar(point_t p[68], float *ear, float *mar) {
    point_t le[6]={p[36],p[37],p[38],p[39],p[40],p[41]};
    point_t re[6]={p[42],p[43],p[44],p[45],p[46],p[47]};
    *ear = (ear_single(le)+ear_single(re))/2.0f;
    mouth_points_t m = {{p[48],p[51],p[62],p[54],p[57],p[66],p[56],p[50]}};
    float num=dist(m.p[1],m.p[7])+dist(m.p[2],m.p[6])+dist(m.p[3],m.p[5]);
    float den=3.0f*dist(m.p[0],m.p[4]);
    *mar = (den<1e-6f)?0.0f:num/den;
}
bool dms_is_hand_blocked(point_t p[68]) {
    if (dist(p[36],p[39])<5.0f || dist(p[42],p[45])<5.0f) return true;
    float e,m; dms_compute_ear_mar(p,&e,&m);
    return (e<0.02f);
}

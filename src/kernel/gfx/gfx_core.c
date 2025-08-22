#include "gfx.h"
#include "gfx_driver.h"
#include "kprintf.h"

static void bzero(void* p,size_t n){ unsigned char* d=p; while(n--)*d++=0; }
static void bcopy(const void* s,void* d,size_t n){ const unsigned char* p=s; unsigned char* q=d; while(n--)*q++=*p++; }

#define MAX_DRIVERS 8
static const struct gfx_driver_ops* g_list[MAX_DRIVERS];
static unsigned g_n=0;
static const struct gfx_driver_ops* g = 0;

void gfx_register_driver(const struct gfx_driver_ops* ops){
    if (g_n<MAX_DRIVERS) g_list[g_n++] = ops;
}

extern void gfx_register_virtio_gpu_legacy(void);

int gfx_init(void){
    gfx_register_virtio_gpu_legacy();

    for (unsigned i=0;i<g_n;i++){
        const struct gfx_driver_ops* ops = g_list[i];
        if (!ops) continue;
        if (ops->probe && !ops->probe()) continue;
        if (ops->init && ops->init()==0){
            g = ops;
            kprintf("[gfx] driver=%s %ux%u pitch=%u\n",
                g->name, g->get_w(), g->get_h(), g->get_pitch());
            return 0;
        }
    }
    kprintf("[gfx] no usable driver\n");
    return -1;
}

uint32_t  gfx_width(void){ return g? g->get_w():0; }
uint32_t  gfx_height(void){ return g? g->get_h():0; }
uint32_t  gfx_pitch_bytes(void){ return g? g->get_pitch():0; }
uint32_t* gfx_fb(void){ return g? g->get_fb():0; }
int       gfx_present(const struct gfx_rect* r){ return g? g->present(r): -1; }

void gfx_clear(gfx_color c){
    uint32_t* fb = gfx_fb(); if(!fb) return;
    uint32_t W=gfx_width(), H=gfx_height(), P=gfx_pitch_bytes()/4;
    for (uint32_t y=0;y<H;y++){ uint32_t* row=fb+y*P; for(uint32_t x=0;x<W;x++) row[x]=c; }
}
void gfx_fill_rect(uint32_t x,uint32_t y,uint32_t w,uint32_t h,gfx_color c){
    uint32_t* fb=gfx_fb(); if(!fb) return;
    uint32_t W=gfx_width(), H=gfx_height(), P=gfx_pitch_bytes()/4;
    if (x>=W||y>=H) return; if (x+w>W) w=W-x; if (y+h>H) h=H-y;
    for(uint32_t yy=0;yy<h;yy++){ uint32_t* row=fb+(y+yy)*P+x; for(uint32_t xx=0;xx<w;xx++) row[xx]=c; }
}
void gfx_put_pixel(uint32_t x,uint32_t y,gfx_color c){
    uint32_t* fb=gfx_fb(); if(!fb) return;
    uint32_t W=gfx_width(), H=gfx_height(), P=gfx_pitch_bytes()/4;
    if (x<W && y<H) fb[y*P+x]=c;
}
void gfx_hline(uint32_t x,uint32_t y,uint32_t w,gfx_color c){ gfx_fill_rect(x,y,w,1,c); }
void gfx_vline(uint32_t x,uint32_t y,uint32_t h,gfx_color c){ gfx_fill_rect(x,y,1,h,c); }
void gfx_line(uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,gfx_color c){
    int dx = (int)(x1>x0?x1-x0:x0-x1), sx = x0<x1?1:-1;
    int dy = (int)(y0>y1?y1-y0:y0-y1), sy = y0<y1?1:-1;
    int err = dx+dy;
    for(;;){
        gfx_put_pixel(x0,y0,c);
        if (x0==x1 && y0==y1) break;
        int e2=2*err;
        if (e2>=dy){ err+=dy; x0+=sx; }
        if (e2<=dx){ err+=dx; y0+=sy; }
    }
}
void gfx_blit_bgra32(const uint32_t* src,uint32_t sw,uint32_t sh,uint32_t dx,uint32_t dy){
    uint32_t* fb=gfx_fb(); if(!fb||!src) return;
    uint32_t W=gfx_width(), H=gfx_height(), P=gfx_pitch_bytes()/4;
    if (dx>=W||dy>=H) return; if (dx+sw>W) sw=W-dx; if (dy+sh>H) sh=H-dy;
    for (uint32_t y=0;y<sh;y++){
        const uint32_t* s=src+y*sw; uint32_t* d=fb+(dy+y)*P+dx;
        bcopy(s,d,sw*4);
    }
}

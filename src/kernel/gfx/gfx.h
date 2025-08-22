#ifndef __GFX_H__
#define __GFX_H__
#include "ktypes.h"

typedef uint32_t gfx_color;

struct gfx_rect {
    uint32_t x, y, w, h;
};

static inline gfx_color GFX_BGRA(uint8_t b,uint8_t g,uint8_t r){
    return (uint32_t)b | ((uint32_t)g<<8) | ((uint32_t)r<<16) | 0xFF000000u;
}

/* public API */
int       gfx_init(void);
uint32_t  gfx_width(void);
uint32_t  gfx_height(void);
uint32_t  gfx_pitch_bytes(void);
uint32_t* gfx_fb(void);
int       gfx_present(const struct gfx_rect* dirty);

void gfx_clear(gfx_color c);
void gfx_fill_rect(uint32_t x,uint32_t y,uint32_t w,uint32_t h,gfx_color c);
void gfx_put_pixel(uint32_t x,uint32_t y,gfx_color c);
void gfx_hline(uint32_t x,uint32_t y,uint32_t w,gfx_color c);
void gfx_vline(uint32_t x,uint32_t y,uint32_t h,gfx_color c);
void gfx_line(uint32_t x0,uint32_t y0,uint32_t x1,uint32_t y1,gfx_color c);
void gfx_blit_bgra32(const uint32_t* src,uint32_t sw,uint32_t sh,uint32_t dx,uint32_t dy);

#endif
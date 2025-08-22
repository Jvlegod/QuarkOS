#ifndef __GFX_DRIVER_H__
#define __GFX_DRIVER_H__
#include "ktypes.h"

struct gfx_rect;

struct gfx_driver_ops {
    const char* name;
    int        (*probe)(void);
    int        (*init)(void);
    uint32_t*  (*get_fb)(void);
    uint32_t   (*get_w)(void);
    uint32_t   (*get_h)(void);
    uint32_t   (*get_pitch)(void);
    int        (*present)(const struct gfx_rect* dirty);
};

/* 供驱动注册 */
void gfx_register_driver(const struct gfx_driver_ops* ops);

/* 由各驱动提供的注册入口（这里先只有 virtio-legacy） */
void gfx_register_virtio_gpu_legacy(void);

#endif
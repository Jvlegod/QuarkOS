#ifndef __VIRTIO_NET_H__
#define __VIRTIO_NET_H__
#include "ktypes.h"
#include "virtio.h"

/* Virtio device ID for network */
enum { VIRTIO_ID_NET = 1 };

#define NET_RXQ          0
#define NET_TXQ          1
#define NET_MAX_PKT      1514

struct virtio_net_hdr {
    uint8_t  flags;
    uint8_t  gso_type;
    uint16_t hdr_len;
    uint16_t gso_size;
    uint16_t csum_start;
    uint16_t csum_offset;
    uint16_t num_buffers;
} __attribute__((packed));


int virtio_net_init(void);
int virtio_net_send(const void *data, uint16_t len);
int virtio_net_recv(void *buf, uint16_t buflen);

#endif /* __VIRTIO_NET_H__ */
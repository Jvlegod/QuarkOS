#ifndef __VIRTIO_NET_H__
#define __VIRTIO_NET_H__
#include "ktypes.h"
#include "virtio.h"

/* Virtio device ID for network */
enum { VIRTIO_ID_NET = 1 };

int virtio_net_init(void);
int virtio_net_send(const void *data, uint16_t len);
int virtio_net_recv(void *buf, uint16_t buflen);

#endif /* __VIRTIO_NET_H__ */
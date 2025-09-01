## 1 概述


VirtIO 是一个让"虚拟设备像真设备一样"工作的标准,  来宾系统里的驱动并不需要知道自己在虚拟机里,  它像给物理网卡/磁盘写驱动那样去用中断, DMA 等"常规手段". 规范把每个 virtio 设备都拆成四个基本部件: 

1. 设备状态字段(Device Status)

2. 特性位(feature bits)

3. 设备配置空间(Device-specific Configuration Space)

4. 一个或多个 virtqueue(队列)

### 1.1 设备状态位

一组位标志,  描述"驱动与设备的握手/生命周期状态". 驱动按顺序置这些位,  设备据此进入下一阶段.

```bash
ACKNOWLEDGE: 驱动已发现设备, 准备驱动它.

DRIVER: 驱动已加载并能处理这个设备.

FEATURES_OK: 特性协商完成.

DRIVER_OK: 驱动已完成队列初始化, 设备可开始工作.

NEEDS_RESET(设备置位): 设备出错, 需要驱动复位.

FAILED: 驱动宣告失败(通常用于致命错误后停止).

写 0 到状态寄存器表示"设备复位".
```

#### 1.1.1 驱动过程

ACKNOWLEDGE → DRIVER →(完成特性协商)→ FEATURES_OK →(建好队列并就绪)→ DRIVER_OK

- ACKNOWLEDGE → DRIVER

- 读取设备特性, 选择并写回 → 置 FEATURES_OK(见下节)

- 建队列, 把队列标记就绪

- 置 DRIVER_OK, 设备才会开始取队列里的请求

- 运行期若设备置 NEEDS_RESET, 驱动需复位并重来

### 1.2 特性位(Feature Bits)

一组 64 位(或更宽, 分段读取)的"能力开关".设备发布"我会什么", 驱动只选择"我懂什么".谈妥后双方按这个"交集"工作, 保证前/后兼容.

设备把自己支持的能力"开菜单", 驱动只勾自己懂的那部分——写回去后设备如果接受, 就把 FEATURES_OK 置起来.这个机制保证了前后兼容；此外, VIRTIO_F_VERSION_1 用来区分"现代接口(1.0+)/传统接口(0.9x).

> https://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.html

```bash
VERSION_1：表示"现代"接口(≥1.0).

RING_INDIRECT_DESC：允许"间接描述符", 一次请求可以指向一小块"描述符表", 减少描述符消耗.

RING_EVENT_IDX：更细粒度的通知/中断抑制.

NOTIFY_ON_EMPTY：队列空时通知(设备类型不同, 意义略异).
```

### 1.3 设备配置空间(Device-specific Configuration Space)

与设备类型相关的一块寄存器/内存映射区域, 放该设备"业务语义"的配置：

- 例如 virtio-blk 里有容量(sectors), 只读标志, 块大小等；

- virtio-net 有 MAC 地址, 最大队列数, MTU, 控制队列开关等.

### 1.4 virtqueue

见 2.4 节.

驱动与设备之间传数据的通道.每个设备有 1 个或多个队列(比如网卡常见 tx/rx 两个或多队列).Virtqueue 在 1.0 中使用 split ring 结构, 由三部分组成：

- Descriptor Table(数组)

- Available Ring(驱动→设备)

- Used Ring(设备→驱动)

```bash
+---------------------------+  <-- 基址 (你代码里的 g_vring_area)
| desc[qsize]               |  // 16 * qsize 字节, 对齐 16
+---------------------------+
| avail                     |  // 4 + 2*qsize (+2) 字节, 对齐 2
|  flags(2), idx(2),        |
|  ring[qsize](2 each),     |
|  [used_event(2)]          |
+---------------------------+  <-- 这里要按 VRING_ALIGN(常用 4096) 再对齐
| used                      |  // 4 + 8*qsize (+2) 字节, 通常要求按 VRING_ALIGN 对齐
|  flags(2), idx(2),        |
|  ring[qsize]{id(4),len(4)}|
|  [avail_event(2)]         |
+---------------------------+
```

```c
/* ---- Virtqueue structs ---- */
#define VIRTQ_DESC_F_NEXT     1   // 有下一个描述符
#define VIRTQ_DESC_F_WRITE    2   // 设备可写(=响应缓冲)
#define VIRTQ_DESC_F_INDIRECT 4   // 间接描述符表(需协商特性)

#ifndef QUEUE_SIZE
#define QUEUE_SIZE 16   /* 8/16/32 均可; 16 足够起步 */
#endif

struct virtq_desc {
    uint64_t addr; // 物理地址
    uint32_t len; // 缓冲区长度
    uint16_t flags;
    uint16_t next;
} __attribute__((packed));

struct virtq_avail {
    uint16_t flags; // =1 可抑制设备中断(VRING_AVAIL_F_NO_INTERRUPT)
    uint16_t idx; // 驱动递增(16位回卷)
    uint16_t ring[QUEUE_SIZE];
    /* uint16_t used_event; // optional */
} __attribute__((packed));

struct virtq_used_elem {
    uint32_t id; // = 完成的“链头描述符索引”
    uint32_t len; // 设备写入到可写缓冲(WRITE)的总字节数
} __attribute__((packed));

struct virtq_used {
    uint16_t flags; // =1 可抑制驱动通知(VRING_USED_F_NO_NOTIFY)
    uint16_t idx; // 设备递增(16位回卷)
    struct virtq_used_elem ring[QUEUE_SIZE];
    /* uint16_t avail_event; // optional */
} __attribute__((packed));
```

设备处理完, 把"完成的第一个描述符索引 + 实际写入长度"写入 used.ring[j], 再递增 used.idx.

驱动轮询/中断处理完成项, 归还描述符.

## 2 寄存器对照表(virtio-mmio)

| ID       | 名称                         | 说明 / 用途                                |
| -------- | -------------------------- | -------------------------------------- |
| 0        | Reserved                   | 保留                                     |
| 1        | Network card               | virtio-net, 虚拟网卡                        |
| 2        | Block device               | virtio-blk, 虚拟块设备                       |
| 3        | Console                    | virtio-console, 虚拟控制台                   |
| 4        | Entropy source             | virtio-rng, 随机数发生器                      |
| 5        | Memory balloon             | virtio-balloon, 内存气球                    |
| 6        | I/O memory                 | 已废弃                                    |
| 7        | RPMSG                      | 远程处理器消息 (remoteproc)                   |
| 8        | SCSI host                  | virtio-scsi, SCSI 主机适配器                 |
| 9        | 9P transport               | virtio-9p, 9P 文件系统共享                    |
| 10       | mac80211 WLAN              | 很少使用                                   |
| 11       | RPROC serial               | 远程处理器串口                                |
| 12       | CAIF                       | 通信协议 (已过时)                             |
| 13       | Memory balloon (legacy)    | 旧版本气球设备                                |
| 16       | GPU                        | virtio-gpu, 虚拟 GPU                      |
| 17       | Timer / clock              | virtio-timer                           |
| 18       | Input device               | virtio-input, 如键盘, 鼠标, 触屏                 |
| 19       | Socket device              | virtio-vsock, 主机/虚拟机通信                  |
| 20       | Crypto device              | virtio-crypto, 加密加速                     |
| 21       | Signal Distribution Module | 较少使用                                   |
| 22       | Pstore                     | 崩溃转储/持久存储                              |
| 23       | IOMMU                      | virtio-iommu, I/O 内存管理单元                |
| 24       | Memory device              | virtio-mem, 可热插拔内存                      |
| 25       | Sound device               | virtio-snd, 虚拟声卡                        |
| 26       | File system device         | virtio-fs, 共享文件系统                       |
| 27       | PMEM device                | virtio-pmem, 持久内存                       |
| 28       | RPMB device                | 受保护存储区 (Replay Protected Memory Block) |
| 29       | Watchdog device            | virtio-watchdog, 看门狗                    |
| 30       | CAN device                 | virtio-can, CAN 总线                      |
| 31       | DMABUF device              | virtio-dmabuf, 跨设备 DMA 缓冲               |
| 32       | Param server               | 参数服务器                                  |
| 33       | Audio policy               | 音频策略                                   |
| 34       | Bluetooth                  | virtio-bluetooth                       |
| 35       | GPIO                       | virtio-gpio, 虚拟 GPIO 控制器                |
| 36       | I2C adapter                | virtio-i2c                             |
| 37       | SPI adapter                | virtio-spi                             |
| 38–65535 | Reserved                   | 保留给未来设备                                |

可以自己看文档 4.2 节和 5.0 节.

### 2.1 morden

| 名称                 |          偏移 | 方向 | 作用(摘要)                                          |
| ------------------ | ----------: | -- | ----------------------------------------------- |
| MagicValue         |       0x000 | R  | 常量 `0x74726976`("virt")用于识别设备.                  |
| Version            |       0x004 | R  | 设备版本；现代应为 `0x2`.                                |
| DeviceID           |       0x008 | R  | 设备类型(如 GPU, NET, BLK…).                           |
| VendorID           |       0x00C | R  | 子系统厂商 ID.                                       |
| DeviceFeatures     |       0x010 | R  | 设备支持的特性位(与 *DeviceFeaturesSel* 配合分段读取).         |
| DeviceFeaturesSel  |       0x014 | W  | 选择读取哪 32 位特性段.                                  |
| DriverFeatures     |       0x020 | W  | 驱动确认启用的特性位(与 *DriverFeaturesSel* 配合).           |
| DriverFeaturesSel  |       0x024 | W  | 选择写入哪 32 位特性段.                                  |
| QueueSel           |       0x030 | W  | 选择后续队列寄存器所作用的 virtqueue.                        |
| QueueNumMax        |       0x034 | R  | 该队列支持的最大项数.                                     |
| QueueNum           |       0x038 | W  | 驱动实际使用的队列大小.                                    |
| QueueReady         |       0x044 | RW | 队列就绪位：写 1 使能, 读返回当前状态.                           |
| QueueNotify        |       0x050 | W  | 写入队列索引以通知设备有新 buffer.                           |
| InterruptStatus    |       0x060 | R  | 中断原因位：bit0=已更新 used ring；bit1=配置变更.读本寄存器获知原因.   |
| InterruptACK       |       0x064 | W  | 写入已处理事件的位掩码, 用于**确认/清除**中断.                      |
| Status             |       0x070 | RW | 设备状态位(ACKNOWLEDGE/DRIVER/…/DRIVER\_OK；写 0 复位).  |
| QueueDescLow/High  | 0x080/0x084 | W  | **描述符表**物理地址(低/高 32 位).                         |
| QueueAvailLow/High | 0x090/0x094 | W  | **available ring** 物理地址(低/高 32 位).              |
| QueueUsedLow/High  | 0x0A0/0x0A4 | W  | **used ring** 物理地址(低/高 32 位).                   |
| ConfigGeneration   |       0x0FC | R  | 配置一致性计数(读配置前后各读一次比对).                           |
| Config(设备配置空间)     |      0x100+ | RW | 设备类型专属配置区(字节对齐, 含义由具体设备章节定义).                    |

### 2.2 legacy

| 名称                                               |                            偏移 | 方向        | 作用(与现代差异)                               |
| ------------------------------------------------ | ----------------------------: | --------- | --------------------------------------- |
| MagicValue / Version(=0x1) / DeviceID / VendorID | 0x000 / 0x004 / 0x008 / 0x00C | R         | 基本识别信息(Version=0x1 表示 legacy).          |
| **HostFeatures / HostFeaturesSel**               |                 0x010 / 0x014 | R / W     | 设备特性位(“Host”命名).                        |
| **GuestFeatures / GuestFeaturesSel**             |                 0x020 / 0x024 | W / W     | 驱动确认启用的特性(“Guest”命名).                   |
| **GuestPageSize**                                |                         0x028 | W         | **队列页大小**(以字节计, 2 的幂)；legacy 用于 PFN 计算.  |
| QueueSel / QueueNumMax / QueueNum                |         0x030 / 0x034 / 0x038 | W / R / W | 选择队列与设置大小.                              |
| **QueueAlign**                                   |                         0x03C | W         | **Used ring** 对齐(字节, 2 的幂).              |
| **QueuePFN**                                     |                         0x040 | RW        | 队列**首页页号**(整块 vring 的物理页帧号).停止使用写 0.    |
| QueueNotify / InterruptStatus / InterruptACK     |         0x050 / 0x060 / 0x064 | W / R / W | 通知与中断处理.                                |
| Status                                           |                         0x070 | RW        | 设备状态(写 0 复位).                           |
| Config(设备配置空间)                                   |                        0x100+ | RW        | 设备类型专属配置区.                              |


> Device status Field: 
> 
> ACKNOWLEDGE (1)
>     Indicates that the guest OS has found the device and recognized it as a valid virtio device.
> 
> DRIVER (2)
>     Indicates that the guest OS knows how to drive the device. Note: There could be a significant (or > infinite) delay before setting this bit. For example, under Linux, drivers can be loadable modules.
> 
> FAILED (128)
>     Indicates that something went wrong in the guest, and it has given up on the device. This could be an internal error, or the driver didn’t like the device for some reason, or even a fatal error during > device operation.
> 
> FEATURES_OK (8)
>     Indicates that the driver has acknowledged all the features it understands, and feature negotiation is > complete.
> 
> DRIVER_OK (4)
>     Indicates that the driver is set up and ready to drive the device.
> 
> DEVICE_NEEDS_RESET (64)
>     Indicates that the device has experienced an error from which it can’t recover.


## 3 References

- virtio:  https://docs.oasis-open.org/virtio/virtio/v1.0/virtio-v1.0.html

- virtio-device: https://docs.oasis-open.org/virtio/virtio/v1.3/csd01/virtio-v1.3-csd01.html#x1-74600010
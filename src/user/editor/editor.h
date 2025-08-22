#ifndef __EDITOR_H__
#define __EDITOR_H__

#include "ktypes.h"
#include "kprintf.h"

// p 打印全文
// e 进入“整体编辑模式”(以 . 单独一行结束)
// a 追加模式(以 . 单独一行结束)
// w 写盘(保存)
// r 重新从盘读(丢弃修改)
// q 退出
// h 显示帮助

#define EDITOR_PRINTF(fmt, ...) \
    kprintf(fmt , ##__VA_ARGS__)

void editor_run(const char* path);

#endif
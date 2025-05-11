#include "vsnprintf.h"
#include <stdarg.h>

typedef struct {
    char *buffer;
    size_t size;
    size_t offset;
} PrintContext;

static void vsnprint_char(PrintContext *ctx, char c) {
    if (ctx->offset < ctx->size) {
        ctx->buffer[ctx->offset++] = c;
    }
}

static void vsnprint_str(PrintContext *ctx, const char *s) {
    while (*s && ctx->offset < ctx->size) {
        vsnprint_char(ctx, *s++);
    }
}

static void vsnprint_num(PrintContext *ctx, uint64_t num, int base, 
                    bool is_signed, int width, char pad, bool uppercase) {
    char buf[32];
    const char *digits = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    int pos = 0;
    bool negative = false;

    /* 处理有符号数 */
    if (is_signed && (int64_t)num < 0) {
        negative = true;
        num = (uint64_t)(-(int64_t)num);
    }

    /* 转换为字符串（逆序） */
    do {
        buf[pos++] = digits[num % base];
        num /= base;
    } while (num > 0);

    /* 处理符号和填充 */
    int total_len = pos + (negative ? 1 : 0);
    if (width > total_len) {
        for (int i = 0; i < width - total_len; i++) {
            vsnprint_char(ctx, pad);
        }
    }

    if (negative) {
        vsnprint_char(ctx, '-');
    }

    /* 逆序输出数字 */
    while (--pos >= 0) {
        vsnprint_char(ctx, buf[pos]);
    }
}

int vsnprintf(char *buf, size_t size, const char *fmt, va_list args) {
    PrintContext ctx = {.buffer = buf, .size = size, .offset = 0};

    while (*fmt) {
        if (*fmt != '%') {
            vsnprint_char(&ctx, *fmt++);
            continue;
        }

        fmt++;
        int width = 0;
        char pad = ' ';
        bool left_align = false;

        while (*fmt == '-' || *fmt == '0') {
            if (*fmt == '-') left_align = true;
            else if (*fmt == '0') pad = '0';
            fmt++;
        }
        while (*fmt >= '0' && *fmt <= '9') {
            width = width * 10 + (*fmt - '0');
            fmt++;
        }

        switch (*fmt) {
            case 'd': {
                int num = va_arg(args, int);
                vsnprint_num(&ctx, num, 10, true, width, pad, false);
                break;
            }
            case 'u': {
                unsigned num = va_arg(args, unsigned);
                vsnprint_num(&ctx, num, 10, false, width, pad, false);
                break;
            }
            case 'x':
            case 'X': {
                unsigned num = va_arg(args, unsigned);
                vsnprint_num(&ctx, num, 16, false, width, pad, (*fmt == 'X'));
                break;
            }
            case 's': {
                char *s = va_arg(args, char*);
                vsnprint_str(&ctx, s ? s : "(null)");
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                vsnprint_char(&ctx, c);
                break;
            }
            case '%':
                vsnprint_char(&ctx, '%');
                break;
            default:
                vsnprint_char(&ctx, '?');
                break;
        }
        fmt++;
    }

    if (ctx.offset < ctx.size) {
        ctx.buffer[ctx.offset] = '\0';
    } else if (size > 0) {
        ctx.buffer[size - 1] = '\0';
    }

    return ctx.offset;
}
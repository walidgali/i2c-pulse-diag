/* i2c_linux.c - Linux /dev/i2c-N backend */
#include "i2c.h"

#include <errno.h>
#include <fcntl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#define I2C_DEFAULT_RETRIES   3u
#define I2C_DEFAULT_DELAY_US  1000u
#define I2C_BUS_TIMEOUT_JIFF  10u

typedef struct { int fd; uint8_t bus_id; } linux_priv_t;

int i2c_is_valid_addr(uint8_t addr) {
    return (addr >= I2C_ADDR_MIN && addr <= I2C_ADDR_MAX) ? 1 : 0;
}

static void set_errno_default(int fallback) { if (errno == 0) errno = fallback; }

static int linux_read(void *priv, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    linux_priv_t *lp = (linux_priv_t *)priv;
    if (!lp || !buf || len == 0 || len > 0xFFFF) { errno = EINVAL; return -1; }
    struct i2c_msg msgs[2] = {
        { .addr = addr, .flags = 0,        .len = 1,             .buf = &reg },
        { .addr = addr, .flags = I2C_M_RD, .len = (uint16_t)len, .buf = buf  },
    };
    struct i2c_rdwr_ioctl_data msgset = { .msgs = msgs, .nmsgs = 2 };
    if (ioctl(lp->fd, I2C_RDWR, &msgset) < 0) { set_errno_default(EIO); return -1; }
    return 0;
}

static int linux_write(void *priv, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len) {
    linux_priv_t *lp = (linux_priv_t *)priv;
    if (!lp || len > 0xFE || (len > 0 && !buf)) { errno = EINVAL; return -1; }
    uint8_t tmp[255];
    tmp[0] = reg;
    if (len) memcpy(tmp + 1, buf, len);
    struct i2c_msg msg = { .addr = addr, .flags = 0, .len = (uint16_t)(len + 1), .buf = tmp };
    struct i2c_rdwr_ioctl_data msgset = { .msgs = &msg, .nmsgs = 1 };
    if (ioctl(lp->fd, I2C_RDWR, &msgset) < 0) { set_errno_default(EIO); return -1; }
    return 0;
}

static int linux_probe(void *priv, uint8_t addr) {
    linux_priv_t *lp = (linux_priv_t *)priv;
    if (!lp || !i2c_is_valid_addr(addr)) { errno = EINVAL; return -1; }
    if (ioctl(lp->fd, I2C_SLAVE_FORCE, (unsigned long)addr) < 0) { set_errno_default(EIO); return -1; }
    struct i2c_smbus_ioctl_data args = {
        .read_write = I2C_SMBUS_WRITE, .command = 0,
        .size = I2C_SMBUS_QUICK, .data = NULL,
    };
    if (ioctl(lp->fd, I2C_SMBUS, &args) < 0)
        return (errno == ENXIO || errno == EREMOTEIO) ? 0 : -1;
    return 1;
}

static void linux_destroy(void *priv) {
    linux_priv_t *lp = (linux_priv_t *)priv;
    if (!lp) return;
    if (lp->fd >= 0) close(lp->fd);
    free(lp);
}

static const i2c_transport_t linux_transport = {
    .read = linux_read, .write = linux_write,
    .probe = linux_probe, .destroy = linux_destroy,
};

int i2c_ctx_open(i2c_ctx_t *ctx, uint8_t bus_id) {
    if (!ctx) { errno = EINVAL; return -1; }
    memset(ctx, 0, sizeof(*ctx));
    ctx->retries = I2C_DEFAULT_RETRIES;
    ctx->retry_delay_us = I2C_DEFAULT_DELAY_US;
    ctx->bus_id = bus_id;

    char path[32];
    int n = snprintf(path, sizeof(path), "/dev/i2c-%u", (unsigned)bus_id);
    if (n < 0 || (size_t)n >= sizeof(path)) { errno = EINVAL; return -1; }

    int flags = O_RDWR;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    int fd = open(path, flags);
    if (fd < 0) return -1;

    (void)ioctl(fd, I2C_TIMEOUT, (unsigned long)I2C_BUS_TIMEOUT_JIFF);
    (void)ioctl(fd, I2C_RETRIES, 2);

    linux_priv_t *lp = malloc(sizeof(*lp));
    if (!lp) { int s = errno; close(fd); errno = s; return -1; }
    lp->fd = fd; lp->bus_id = bus_id;

    ctx->ops = &linux_transport; ctx->priv = lp;
    return 0;
}

extern int i2c_ctx_open_mock_impl(i2c_ctx_t *ctx);
int i2c_ctx_open_mock(i2c_ctx_t *ctx) { return i2c_ctx_open_mock_impl(ctx); }

void i2c_ctx_close(i2c_ctx_t *ctx) {
    if (!ctx) return;
    if (ctx->ops && ctx->ops->destroy && ctx->priv) ctx->ops->destroy(ctx->priv);
    memset(ctx, 0, sizeof(*ctx));
}

static int retry_loop(i2c_ctx_t *ctx, int (*op)(void*,uint8_t,uint8_t,uint8_t*,size_t),
                      void *priv, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    uint32_t attempts = ctx->retries ? ctx->retries : 1;
    int last = 0;
    for (uint32_t i = 0; i < attempts; i++) {
        errno = 0;
        if (op(priv, addr, reg, buf, len) == 0) return 0;
        last = errno ? errno : EIO;
        if (last == ENXIO || last == EREMOTEIO || last == EINVAL || last == EFAULT) {
            errno = last; return -1;
        }
        if (i + 1 < attempts && ctx->retry_delay_us) usleep(ctx->retry_delay_us * (i + 1));
    }
    errno = last; return -1;
}

int i2c_read_reg(i2c_ctx_t *ctx, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    if (!ctx || !ctx->ops || !buf || len == 0 || !i2c_is_valid_addr(addr)) { errno = EINVAL; return -1; }
    return retry_loop(ctx, ctx->ops->read, ctx->priv, addr, reg, buf, len);
}

int i2c_write_reg(i2c_ctx_t *ctx, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len) {
    if (!ctx || !ctx->ops || (len > 0 && !buf) || !i2c_is_valid_addr(addr)) { errno = EINVAL; return -1; }
    uint8_t tmp[256];
    if (len > 0) memcpy(tmp, buf, len);
    return retry_loop(ctx, ctx->ops->write, ctx->priv, addr, reg, tmp, len);
}

int i2c_probe(i2c_ctx_t *ctx, uint8_t addr) {
    if (!ctx || !ctx->ops || !i2c_is_valid_addr(addr)) { errno = EINVAL; return -1; }
    return ctx->ops->probe(ctx->priv, addr);
}

int i2c_scan_bus_ex(i2c_ctx_t *ctx, uint8_t *addrs, size_t max, size_t *found) {
    if (!ctx || (!addrs && max > 0) || !found) { errno = EINVAL; return -1; }
    *found = 0;
    for (unsigned a = I2C_ADDR_MIN; a <= I2C_ADDR_MAX; a++) {
        int r = i2c_probe(ctx, (uint8_t)a);
        if (r == 1) {
            if (*found < max && addrs) addrs[*found] = (uint8_t)a;
            (*found)++;
        }
    }
    return 0;
}

int i2c_scan_bus(uint8_t bus_id) {
    i2c_ctx_t ctx;
    if (i2c_ctx_open(&ctx, bus_id) != 0) {
        fprintf(stderr, "Error: cannot open bus %u: %s\n", (unsigned)bus_id, strerror(errno));
        return -1;
    }
    printf("Scanning I2C bus %u (0x%02X..0x%02X):\n", (unsigned)bus_id, I2C_ADDR_MIN, I2C_ADDR_MAX);
    unsigned found = 0;
    for (unsigned a = I2C_ADDR_MIN; a <= I2C_ADDR_MAX; a++) {
        if (i2c_probe(&ctx, (uint8_t)a) == 1) {
            printf("  [FOUND] 0x%02X\n", a);
            found++;
        }
    }
    printf("Scan complete. Active devices found: %u\n", found);
    i2c_ctx_close(&ctx);
    return 0;
}

int i2c_read_reg16(uint8_t bus_id, uint8_t dev_addr, uint8_t reg_addr, uint16_t *out_val) {
    if (!out_val) { errno = EINVAL; return -1; }
    i2c_ctx_t ctx;
    if (i2c_ctx_open(&ctx, bus_id) != 0) {
        fprintf(stderr, "Error: open bus %u: %s\n", (unsigned)bus_id, strerror(errno));
        return -1;
    }
    uint8_t buf[2] = {0, 0};
    int rc = i2c_read_reg(&ctx, dev_addr, reg_addr, buf, sizeof(buf));
    int saved = errno;
    i2c_ctx_close(&ctx);
    if (rc != 0) {
        fprintf(stderr, "Error: read reg 0x%02X from 0x%02X failed: %s\n",
                (unsigned)reg_addr, (unsigned)dev_addr, strerror(saved));
        errno = saved;
        return -1;
    }
    *out_val = (uint16_t)(((uint16_t)buf[0] << 8) | (uint16_t)buf[1]);
    return 0;
}

/* i2c_mock.c - In-memory I2C backend for unit tests and CI */
#include "i2c.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

typedef struct { int present; uint8_t regs[256]; } mock_dev_t;

static mock_dev_t g_devs[256];
static int g_nack_count;
static int g_read_fail_count;
static unsigned int g_delay_us;

void i2c_mock_reset(void) {
    memset(g_devs, 0, sizeof(g_devs));
    g_nack_count = 0; g_read_fail_count = 0; g_delay_us = 0;
}
void i2c_mock_set_present(uint8_t addr, int present) { g_devs[addr].present = present ? 1 : 0; }
void i2c_mock_set_reg(uint8_t addr, uint8_t reg, uint8_t value) {
    g_devs[addr].regs[reg] = value; g_devs[addr].present = 1;
}
int i2c_mock_get_reg(uint8_t addr, uint8_t reg, uint8_t *value) {
    if (!value) { errno = EINVAL; return -1; }
    *value = g_devs[addr].regs[reg]; return 0;
}
void i2c_mock_set_nack_count(int count) { g_nack_count = count; }
void i2c_mock_set_read_fail_count(int count) { g_read_fail_count = count; }
void i2c_mock_set_delay_us(unsigned int us) { g_delay_us = us; }

static int mock_read(void *priv, uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    (void)priv;
    if (!buf || len == 0) { errno = EINVAL; return -1; }
    if (!g_devs[addr].present) { errno = ENXIO; return -1; }
    if (g_delay_us) usleep(g_delay_us);
    if (g_nack_count > 0) { g_nack_count--; errno = EREMOTEIO; return -1; }
    if (g_read_fail_count > 0) { g_read_fail_count--; errno = EIO; return -1; }
    for (size_t i = 0; i < len; i++) buf[i] = g_devs[addr].regs[(uint8_t)(reg + i)];
    return 0;
}

static int mock_write(void *priv, uint8_t addr, uint8_t reg, const uint8_t *buf, size_t len) {
    (void)priv;
    if (len > 0 && !buf) { errno = EINVAL; return -1; }
    if (!g_devs[addr].present) { errno = ENXIO; return -1; }
    if (g_delay_us) usleep(g_delay_us);
    if (g_nack_count > 0) { g_nack_count--; errno = EREMOTEIO; return -1; }
    for (size_t i = 0; i < len; i++) g_devs[addr].regs[(uint8_t)(reg + i)] = buf[i];
    return 0;
}

static int mock_probe(void *priv, uint8_t addr) {
    (void)priv;
    if (!i2c_is_valid_addr(addr)) { errno = EINVAL; return -1; }
    if (g_delay_us) usleep(g_delay_us);
    if (g_nack_count > 0) { g_nack_count--; return 0; }
    return g_devs[addr].present ? 1 : 0;
}

static void mock_destroy(void *priv) { free(priv); }

static const i2c_transport_t mock_transport = {
    .read = mock_read, .write = mock_write,
    .probe = mock_probe, .destroy = mock_destroy,
};

int i2c_ctx_open_mock_impl(i2c_ctx_t *ctx) {
    if (!ctx) { errno = EINVAL; return -1; }
    memset(ctx, 0, sizeof(*ctx));
    ctx->retries = 3; ctx->retry_delay_us = 100; ctx->bus_id = 0;
    void *priv = calloc(1, 1);
    if (!priv) return -1;
    ctx->ops = &mock_transport; ctx->priv = priv;
    return 0;
}

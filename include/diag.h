#ifndef DIAG_H
#define DIAG_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define I2C_ADDR_MIN 0x03U
#define I2C_ADDR_MAX 0x77U

typedef enum {
    STATUS_OK   = 0,
    STATUS_WARN = 1,
    STATUS_CRIT = 2,
    STATUS_ERR  = 3
} diag_status_t;

typedef enum {
    PROFILE_RAW     = 0,
    PROFILE_INA219  = 1,
    PROFILE_ADS1115 = 2,
    PROFILE_TMP102  = 3,
    PROFILE_BME280P = 4
} diag_profile_t;

typedef enum {
    METRIC_VOLTAGE  = 0,
    METRIC_TEMP_C   = 1,
    METRIC_PRESSURE = 2
} diag_metric_t;

typedef struct {
    uint8_t bus_id;
    uint8_t dev_addr;
    uint8_t reg_addr;
    uint16_t raw_val;
    double scaled_val;
    const char *unit;
    diag_status_t status;
    const char *message;
} diag_reading_t;

typedef struct {
    uint32_t samples;
    double min_val;
    double max_val;
    double avg_val;
    double drift_val;
    bool initialized;
} diag_stats_t;

typedef struct {
    uint8_t bus_id;
    uint8_t dev_addr;
    uint8_t reg_addr;
    double scale_factor;
    bool custom_scale;
    diag_profile_t profile;
    diag_metric_t metric;
    bool sim_mode;
    uint16_t sim_raw;
    bool scan_mode;
    uint32_t count;
    uint32_t interval_ms;
    const char *json_path;
} diag_config_t;

const char *status_to_str(diag_status_t status);
const char *profile_to_str(diag_profile_t profile);
const char *metric_unit_str(diag_metric_t metric);
double profile_default_scale(diag_profile_t profile);
diag_metric_t profile_default_metric(diag_profile_t profile);
uint16_t apply_sensor_profile_raw(uint16_t raw_in, diag_profile_t profile);

diag_status_t evaluate_signal(uint16_t raw_val,
                              double scale_factor,
                              diag_metric_t metric,
                              diag_reading_t *out);

void stats_init(diag_stats_t *st);
int stats_update(diag_stats_t *st, double val);
int export_json_report(const char *filepath,
                       const diag_reading_t *reading,
                       const diag_stats_t *stats);

int i2c_is_valid_addr(uint8_t addr);
int i2c_scan_bus(uint8_t bus_id);
int i2c_read_reg16(uint8_t bus_id, uint8_t dev_addr, uint8_t reg_addr, uint16_t *out_val);

#endif

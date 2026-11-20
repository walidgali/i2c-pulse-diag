#include "diag.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

const char *status_to_str(diag_status_t status) {
    switch (status) {
        case STATUS_OK:   return "OK";
        case STATUS_WARN: return "WARNING";
        case STATUS_CRIT: return "CRITICAL";
        default:          return "ERROR";
    }
}

const char *profile_to_str(diag_profile_t profile) {
    switch (profile) {
        case PROFILE_INA219:  return "ina219";
        case PROFILE_ADS1115: return "ads1115";
        case PROFILE_TMP102:  return "tmp102";
        case PROFILE_BME280P: return "bme280_p";
        case PROFILE_RAW:
        default:              return "raw";
    }
}

const char *metric_unit_str(diag_metric_t metric) {
    switch (metric) {
        case METRIC_TEMP_C:   return "C";
        case METRIC_PRESSURE: return "hPa";
        case METRIC_VOLTAGE:
        default:              return "V";
    }
}

double profile_default_scale(diag_profile_t profile) {
    switch (profile) {
        case PROFILE_INA219:  return 0.004;
        case PROFILE_ADS1115: return 0.000125;
        case PROFILE_TMP102:  return 0.0625;
        case PROFILE_BME280P: return 0.01;
        case PROFILE_RAW:
        default:              return 0.01;
    }
}

diag_metric_t profile_default_metric(diag_profile_t profile) {
    switch (profile) {
        case PROFILE_TMP102:  return METRIC_TEMP_C;
        case PROFILE_BME280P: return METRIC_PRESSURE;
        case PROFILE_INA219:
        case PROFILE_ADS1115:
        case PROFILE_RAW:
        default:              return METRIC_VOLTAGE;
    }
}

uint16_t apply_sensor_profile_raw(uint16_t raw_in, diag_profile_t profile) {
    switch (profile) {
        case PROFILE_INA219:
            return (uint16_t)(raw_in >> 3);
        case PROFILE_TMP102:
            return (uint16_t)(raw_in >> 4);
        case PROFILE_ADS1115:
        case PROFILE_BME280P:
        case PROFILE_RAW:
        default:
            return raw_in;
    }
}

static diag_status_t eval_voltage(double v, const char **msg) {
    if (v < 0.0) {
        *msg = "Invalid negative voltage reading";
        return STATUS_ERR;
    }
    if (v < 3.0 || v > 14.8) {
        *msg = "Voltage outside safe operating threshold";
        return STATUS_CRIT;
    }
    if (v < 3.3 || v > 14.2) {
        *msg = "Voltage approaching threshold limit";
        return STATUS_WARN;
    }
    *msg = "Voltage within normal range";
    return STATUS_OK;
}

static diag_status_t eval_temp_c(double c, const char **msg) {
    if (c < -40.0 || c > 125.0) {
        *msg = "Temperature outside sensor physical range";
        return STATUS_ERR;
    }
    if (c < 0.0 || c > 75.0) {
        *msg = "Temperature critical threshold exceeded";
        return STATUS_CRIT;
    }
    if (c < 5.0 || c > 60.0) {
        *msg = "Temperature approaching warning threshold";
        return STATUS_WARN;
    }
    *msg = "Temperature within normal range";
    return STATUS_OK;
}

static diag_status_t eval_pressure_hpa(double p, const char **msg) {
    if (p < 300.0 || p > 1200.0) {
        *msg = "Pressure outside expected atmospheric range";
        return STATUS_ERR;
    }
    if (p < 850.0 || p > 1080.0) {
        *msg = "Pressure critical anomaly detected";
        return STATUS_CRIT;
    }
    if (p < 950.0 || p > 1050.0) {
        *msg = "Pressure approaching boundary";
        return STATUS_WARN;
    }
    *msg = "Pressure within normal range";
    return STATUS_OK;
}

diag_status_t evaluate_signal(uint16_t raw_val,
                              double scale_factor,
                              diag_metric_t metric,
                              diag_reading_t *out) {
    if (!out) {
        return STATUS_ERR;
    }

    out->raw_val = raw_val;
    out->unit = metric_unit_str(metric);

    if (!isfinite(scale_factor) || scale_factor <= 0.0) {
        out->scaled_val = 0.0;
        out->status = STATUS_ERR;
        out->message = "Invalid scale factor";
        return out->status;
    }

    out->scaled_val = (double)raw_val * scale_factor;
    if (!isfinite(out->scaled_val)) {
        out->scaled_val = 0.0;
        out->status = STATUS_ERR;
        out->message = "Non-finite scaled reading";
        return out->status;
    }

    const char *msg = "";
    switch (metric) {
        case METRIC_TEMP_C:
            out->status = eval_temp_c(out->scaled_val, &msg);
            break;
        case METRIC_PRESSURE:
            out->status = eval_pressure_hpa(out->scaled_val, &msg);
            break;
        case METRIC_VOLTAGE:
        default:
            out->status = eval_voltage(out->scaled_val, &msg);
            break;
    }

    out->message = msg;
    return out->status;
}

void stats_init(diag_stats_t *st) {
    if (!st) {
        return;
    }
    memset(st, 0, sizeof(*st));
    st->min_val = 0.0;
    st->max_val = 0.0;
    st->avg_val = 0.0;
    st->drift_val = 0.0;
    st->initialized = false;
}

int stats_update(diag_stats_t *st, double val) {
    if (!st || !isfinite(val)) {
        return -1;
    }

    if (!st->initialized) {
        st->samples = 1U;
        st->min_val = val;
        st->max_val = val;
        st->avg_val = val;
        st->drift_val = 0.0;
        st->initialized = true;
        return 0;
    }

    if (st->samples == UINT32_MAX) {
        return -1;
    }

    st->samples += 1U;
    if (val < st->min_val) {
        st->min_val = val;
    }
    if (val > st->max_val) {
        st->max_val = val;
    }

    double prev_avg = st->avg_val;
    st->avg_val = prev_avg + ((val - prev_avg) / (double)st->samples);
    st->drift_val = st->max_val - st->min_val;
    return 0;
}

static int has_control_chars(const char *s) {
    if (!s) {
        return 1;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        if (*p < 0x20 || *p == 0x7F) {
            return 1;
        }
    }
    return 0;
}

static int write_json_escaped_str(FILE *fp, const char *s) {
    if (!fp || !s) {
        return -1;
    }
    if (fputc('"', fp) == EOF) {
        return -1;
    }
    for (const unsigned char *p = (const unsigned char *)s; *p != '\0'; p++) {
        unsigned char c = *p;
        if (c == '"' || c == '\\') {
            if (fputc('\\', fp) == EOF || fputc((int)c, fp) == EOF) {
                return -1;
            }
        } else if (c == '\n') {
            if (fputs("\\n", fp) == EOF) {
                return -1;
            }
        } else if (c == '\r') {
            if (fputs("\\r", fp) == EOF) {
                return -1;
            }
        } else if (c < 0x20) {
            if (fprintf(fp, "\\u%04x", (unsigned int)c) < 0) {
                return -1;
            }
        } else {
            if (fputc((int)c, fp) == EOF) {
                return -1;
            }
        }
    }
    if (fputc('"', fp) == EOF) {
        return -1;
    }
    return 0;
}

int export_json_report(const char *filepath,
                       const diag_reading_t *reading,
                       const diag_stats_t *stats) {
    if (!filepath || filepath[0] == '\0' || has_control_chars(filepath) || !reading) {
        fprintf(stderr, "Error: invalid report filepath or reading structure\n");
        return -1;
    }

    int flags = O_WRONLY | O_CREAT | O_TRUNC;
#ifdef O_NOFOLLOW
    flags |= O_NOFOLLOW;
#endif

    int fd = open(filepath, flags, 0600);
    if (fd < 0) {
        fprintf(stderr, "Error: cannot open report '%s': %s\n", filepath, strerror(errno));
        return -1;
    }

    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        fprintf(stderr, "Error: fdopen failed for '%s': %s\n", filepath, strerror(errno));
        close(fd);
        return -1;
    }

    double safe_scaled = isfinite(reading->scaled_val) ? reading->scaled_val : 0.0;
    const char *unit_str = reading->unit ? reading->unit : "V";
    const char *msg_str = reading->message ? reading->message : "";

    uint32_t samples = (stats && stats->initialized) ? stats->samples : 1U;
    double min_v = (stats && stats->initialized && isfinite(stats->min_val)) ? stats->min_val : safe_scaled;
    double max_v = (stats && stats->initialized && isfinite(stats->max_val)) ? stats->max_val : safe_scaled;
    double avg_v = (stats && stats->initialized && isfinite(stats->avg_val)) ? stats->avg_val : safe_scaled;
    double drift_v = (stats && stats->initialized && isfinite(stats->drift_val)) ? stats->drift_val : 0.0;

    if (fprintf(fp, "{\n") < 0 ||
        fprintf(fp, "  \"bus_id\": %u,\n", (unsigned int)reading->bus_id) < 0 ||
        fprintf(fp, "  \"device_addr\": \"0x%02X\",\n", (unsigned int)reading->dev_addr) < 0 ||
        fprintf(fp, "  \"register_addr\": \"0x%02X\",\n", (unsigned int)reading->reg_addr) < 0 ||
        fprintf(fp, "  \"raw_value\": %u,\n", (unsigned int)reading->raw_val) < 0 ||
        fprintf(fp, "  \"scaled_value\": %.4f,\n", safe_scaled) < 0 ||
        fprintf(fp, "  \"unit\": ") < 0 ||
        write_json_escaped_str(fp, unit_str) != 0 ||
        fprintf(fp, ",\n  \"status\": ") < 0 ||
        write_json_escaped_str(fp, status_to_str(reading->status)) != 0 ||
        fprintf(fp, ",\n  \"message\": ") < 0 ||
        write_json_escaped_str(fp, msg_str) != 0 ||
        fprintf(fp, ",\n  \"stats\": {\n") < 0 ||
        fprintf(fp, "    \"samples\": %u,\n", (unsigned int)samples) < 0 ||
        fprintf(fp, "    \"min\": %.4f,\n", min_v) < 0 ||
        fprintf(fp, "    \"max\": %.4f,\n", max_v) < 0 ||
        fprintf(fp, "    \"avg\": %.4f,\n", avg_v) < 0 ||
        fprintf(fp, "    \"drift\": %.4f\n", drift_v) < 0 ||
        fprintf(fp, "  }\n}\n") < 0) {
        fprintf(stderr, "Error: failed writing JSON report to '%s'\n", filepath);
        fclose(fp);
        return -1;
    }

    if (fflush(fp) != 0 || fclose(fp) != 0) {
        fprintf(stderr, "Error: failed flushing/closing '%s': %s\n", filepath, strerror(errno));
        return -1;
    }

    return 0;
}

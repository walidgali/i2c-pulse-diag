#include "diag.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void print_usage(const char *prog) {
    const char *name = (prog && prog[0] != '\0') ? prog : "i2c-pulse-diag";
    fprintf(stderr,
        "Usage:\n"
        "  %s --scan [-b bus]\n"
        "  %s [-b bus] [-a addr] [-r reg] [--profile P] [-s scale] [-n count] [-i ms] [-o json]\n"
        "  %s --sim <raw_u16> [--profile P] [-s scale] [-n count] [-i ms] [-o json]\n\n"
        "Profiles: raw, ina219, ads1115, tmp102, bme280_p\n",
        name, name, name);
}

static int parse_u32_range(const char *str, uint32_t min_v, uint32_t max_v, uint32_t *out) {
    if (!str || *str == '\0' || !out) {
        return -1;
    }
    errno = 0;
    char *endptr = NULL;
    unsigned long val = strtoul(str, &endptr, 0);
    if (errno != 0 || endptr == str || *endptr != '\0') {
        return -1;
    }
    if (val < (unsigned long)min_v || val > (unsigned long)max_v) {
        return -1;
    }
    *out = (uint32_t)val;
    return 0;
}

static int parse_scale(const char *str, double *out) {
    if (!str || *str == '\0' || !out) {
        return -1;
    }
    errno = 0;
    char *endptr = NULL;
    double val = strtod(str, &endptr);
    if (errno != 0 || endptr == str || *endptr != '\0' || !isfinite(val) || val <= 0.0) {
        return -1;
    }
    *out = val;
    return 0;
}

static int parse_profile(const char *str, diag_profile_t *out) {
    if (!str || !out) {
        return -1;
    }
    if (strcmp(str, "raw") == 0) {
        *out = PROFILE_RAW;
        return 0;
    }
    if (strcmp(str, "ina219") == 0) {
        *out = PROFILE_INA219;
        return 0;
    }
    if (strcmp(str, "ads1115") == 0) {
        *out = PROFILE_ADS1115;
        return 0;
    }
    if (strcmp(str, "tmp102") == 0) {
        *out = PROFILE_TMP102;
        return 0;
    }
    if (strcmp(str, "bme280_p") == 0) {
        *out = PROFILE_BME280P;
        return 0;
    }
    return -1;
}

static int sleep_ms(uint32_t ms) {
    if (ms == 0) {
        return 0;
    }
    struct timespec req = {
        .tv_sec = (time_t)(ms / 1000U),
        .tv_nsec = (long)((ms % 1000U) * 1000000L)
    };
    while (nanosleep(&req, &req) != 0) {
        if (errno != EINTR) {
            return -1;
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    diag_config_t cfg = {
        .bus_id = 1,
        .dev_addr = 0x40,
        .reg_addr = 0x02,
        .scale_factor = 0.01,
        .custom_scale = false,
        .profile = PROFILE_RAW,
        .metric = METRIC_VOLTAGE,
        .sim_mode = false,
        .sim_raw = 1245,
        .scan_mode = false,
        .count = 1,
        .interval_ms = 100,
        .json_path = "report.json"
    };

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return EXIT_SUCCESS;
        } else if (strcmp(argv[i], "--scan") == 0) {
            cfg.scan_mode = true;
        } else if (strcmp(argv[i], "--sim") == 0) {
            uint32_t tmp = 0;
            if (i + 1 >= argc || parse_u32_range(argv[++i], 0U, 65535U, &tmp) != 0) {
                fprintf(stderr, "Error: --sim requires raw value (0..65535)\n");
                return EXIT_FAILURE;
            }
            cfg.sim_raw = (uint16_t)tmp;
            cfg.sim_mode = true;
        } else if (strcmp(argv[i], "--profile") == 0) {
            if (i + 1 >= argc || parse_profile(argv[++i], &cfg.profile) != 0) {
                fprintf(stderr, "Error: --profile must be raw, ina219, ads1115, tmp102, or bme280_p\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-b") == 0) {
            uint32_t tmp = 0;
            if (i + 1 >= argc || parse_u32_range(argv[++i], 0U, 255U, &tmp) != 0) {
                fprintf(stderr, "Error: invalid -b bus_id (0..255)\n");
                return EXIT_FAILURE;
            }
            cfg.bus_id = (uint8_t)tmp;
        } else if (strcmp(argv[i], "-a") == 0) {
            uint32_t tmp = 0;
            if (i + 1 >= argc || parse_u32_range(argv[++i], I2C_ADDR_MIN, I2C_ADDR_MAX, &tmp) != 0) {
                fprintf(stderr, "Error: invalid -a address (0x03..0x77)\n");
                return EXIT_FAILURE;
            }
            cfg.dev_addr = (uint8_t)tmp;
        } else if (strcmp(argv[i], "-r") == 0) {
            uint32_t tmp = 0;
            if (i + 1 >= argc || parse_u32_range(argv[++i], 0U, 255U, &tmp) != 0) {
                fprintf(stderr, "Error: invalid -r register (0x00..0xFF)\n");
                return EXIT_FAILURE;
            }
            cfg.reg_addr = (uint8_t)tmp;
        } else if (strcmp(argv[i], "-s") == 0) {
            if (i + 1 >= argc || parse_scale(argv[++i], &cfg.scale_factor) != 0) {
                fprintf(stderr, "Error: invalid -s scale factor (positive finite number)\n");
                return EXIT_FAILURE;
            }
            cfg.custom_scale = true;
        } else if (strcmp(argv[i], "-n") == 0) {
            if (i + 1 >= argc || parse_u32_range(argv[++i], 1U, 100000U, &cfg.count) != 0) {
                fprintf(stderr, "Error: invalid -n count (1..100000)\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-i") == 0) {
            if (i + 1 >= argc || parse_u32_range(argv[++i], 0U, 60000U, &cfg.interval_ms) != 0) {
                fprintf(stderr, "Error: invalid -i interval_ms (0..60000)\n");
                return EXIT_FAILURE;
            }
        } else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc || argv[i + 1][0] == '\0') {
                fprintf(stderr, "Error: -o requires non-empty output path\n");
                return EXIT_FAILURE;
            }
            cfg.json_path = argv[++i];
        } else {
            fprintf(stderr, "Error: unknown argument '%s'\n", argv[i]);
            print_usage(argv[0]);
            return EXIT_FAILURE;
        }
    }

    if (cfg.scan_mode) {
        return (i2c_scan_bus(cfg.bus_id) == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    cfg.metric = profile_default_metric(cfg.profile);
    if (!cfg.custom_scale) {
        cfg.scale_factor = profile_default_scale(cfg.profile);
    }

    diag_stats_t stats;
    stats_init(&stats);

    diag_reading_t last_reading = {
        .bus_id = cfg.bus_id,
        .dev_addr = cfg.dev_addr,
        .reg_addr = cfg.reg_addr,
        .raw_val = 0,
        .scaled_val = 0.0,
        .unit = metric_unit_str(cfg.metric),
        .status = STATUS_OK,
        .message = ""
    };

    for (uint32_t iter = 0; iter < cfg.count; iter++) {
        uint16_t raw_wire = 0;
        if (cfg.sim_mode) {
            raw_wire = cfg.sim_raw;
        } else {
            if (i2c_read_reg16(cfg.bus_id, cfg.dev_addr, cfg.reg_addr, &raw_wire) != 0) {
                return EXIT_FAILURE;
            }
        }

        uint16_t proc_raw = apply_sensor_profile_raw(raw_wire, cfg.profile);

        diag_reading_t reading = {
            .bus_id = cfg.bus_id,
            .dev_addr = cfg.dev_addr,
            .reg_addr = cfg.reg_addr,
            .raw_val = 0,
            .scaled_val = 0.0,
            .unit = metric_unit_str(cfg.metric),
            .status = STATUS_OK,
            .message = ""
        };

        evaluate_signal(proc_raw, cfg.scale_factor, cfg.metric, &reading);

        if (stats_update(&stats, reading.scaled_val) != 0) {
            fprintf(stderr, "Error: failed updating reading statistics\n");
            return EXIT_FAILURE;
        }

        printf("[%u/%u] Mode=%s Profile=%s Raw=%u Val=%.4f %s [%s] %s\n",
               iter + 1U,
               cfg.count,
               cfg.sim_mode ? "SIM" : "HW",
               profile_to_str(cfg.profile),
               (unsigned int)reading.raw_val,
               reading.scaled_val,
               reading.unit,
               status_to_str(reading.status),
               reading.message);

        last_reading = reading;

        if (iter + 1U < cfg.count && cfg.interval_ms > 0U) {
            if (sleep_ms(cfg.interval_ms) != 0) {
                fprintf(stderr, "Error: sleep interrupted/failed\n");
                return EXIT_FAILURE;
            }
        }
    }

    if (stats.initialized) {
        printf("Summary (n=%u): min=%.4f %s, max=%.4f %s, avg=%.4f %s, drift=%.4f %s\n",
               (unsigned int)stats.samples,
               stats.min_val, last_reading.unit,
               stats.max_val, last_reading.unit,
               stats.avg_val, last_reading.unit,
               stats.drift_val, last_reading.unit);
    }

    if (export_json_report(cfg.json_path, &last_reading, &stats) != 0) {
        return EXIT_FAILURE;
    }

    printf("Saved report to %s\n", cfg.json_path);
    return EXIT_SUCCESS;
}

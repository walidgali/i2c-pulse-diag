#define _POSIX_C_SOURCE 200809L

#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_REPORT_BYTES (1024U * 1024U)
#define READ_CHUNK_BYTES 4096U

static void print_usage(const char *prog_name) {
    fprintf(stderr,
            "Usage: %s <path_to_json_report>\n"
            "Audits a JSON diagnostic report file for size limits, safe bytes,\n"
            "and JSON object/array structure.\n",
            (prog_name && prog_name[0] != '\0') ? prog_name : "qa_report_audit");
}

static bool is_safe_json_byte(unsigned char b) {
    if (b == '\n' || b == '\r' || b == '\t') {
        return true;
    }
    if (b >= 0x20U && b <= 0x7EU) {
        return true;
    }
    if (b >= 0x80U && b <= 0xF7U) {
        return true;
    }
    return false;
}

int main(int argc, char **argv) {
    if (argc != 2 || !argv[1] || argv[1][0] == '\0') {
        print_usage((argc > 0) ? argv[0] : NULL);
        return 2;
    }

    const char *path = argv[1];
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) {
        perror("open report failed");
        return 3;
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        perror("fstat failed");
        close(fd);
        return 3;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "Error: target path is not a regular file.\n");
        close(fd);
        return 4;
    }

    if (st.st_size <= 0) {
        fprintf(stderr, "Error: report file is empty.\n");
        close(fd);
        return 5;
    }

    if ((uint64_t)st.st_size > MAX_REPORT_BYTES) {
        fprintf(stderr, "Error: report exceeds maximum allowed size (%u bytes).\n",
                MAX_REPORT_BYTES);
        close(fd);
        return 5;
    }

    uint8_t buf[READ_CHUNK_BYTES];
    size_t total_bytes = 0;
    size_t non_space_chars = 0;
    bool has_brace = false;
    bool invalid_byte = false;

    for (;;) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("read failed");
            close(fd);
            return 3;
        }
        if (n == 0) {
            break;
        }

        size_t chunk_len = (size_t)n;
        total_bytes += chunk_len;

        for (size_t i = 0; i < chunk_len; i++) {
            unsigned char c = buf[i];
            if (!is_safe_json_byte(c)) {
                invalid_byte = true;
                break;
            }
            if (c > 0x20U && c < 0x7FU) {
                non_space_chars++;
            }
            if (c == '{' || c == '[') {
                has_brace = true;
            }
        }

        if (invalid_byte) {
            break;
        }
    }

    close(fd);

    if (invalid_byte) {
        fprintf(stderr, "Error: unsafe control or invalid byte detected in report.\n");
        return 6;
    }

    if (non_space_chars == 0 || !has_brace) {
        fprintf(stderr, "Error: file does not look like a valid JSON object/array.\n");
        return 6;
    }

    printf("AUDIT_OK bytes=%zu status=valid\n", total_bytes);
    return 0;
}

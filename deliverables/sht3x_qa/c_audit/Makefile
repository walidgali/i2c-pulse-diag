CC ?= gcc
CFLAGS ?= -std=c11 -O2 -Wall -Wextra -Werror -Wpedantic -D_POSIX_C_SOURCE=200809L -D_FORTIFY_SOURCE=2 -fstack-protector-strong

TARGET := qa_report_audit
SRC := qa_report_audit.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(TARGET) *.o

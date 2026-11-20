CC = gcc
CFLAGS = -Wall -Wextra -Werror -Wpedantic -std=c11 -D_POSIX_C_SOURCE=200809L -D_FORTIFY_SOURCE=2 -O2 -fstack-protector-strong -Iinclude
LDFLAGS = -lm

TARGET = i2c-pulse-diag
SRCS = src/main.c src/diag.c src/i2c_linux.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

src/%.o: src/%.c include/diag.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET) report.json

.PHONY: all clean

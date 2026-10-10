CC ?= gcc
CFLAGS = -Wall -Wextra -O2 -Iinclude
LDFLAGS = -lm

ALL_SRCS = src/filter.c src/protocol.c src/storage_format.c src/mpu6050.c src/w25qxx.c
TEST_SRCS = tests/test_firmware_core.c $(ALL_SRCS)
MAIN_SRCS = src/main.c $(ALL_SRCS)

.PHONY: all test sim clean

all: test sim

test: build/test_runner
	./build/test_runner

sim: build/tilt_sensor_sim
	./build/tilt_sensor_sim

build/test_runner: $(TEST_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/tilt_sensor_sim: $(MAIN_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf build

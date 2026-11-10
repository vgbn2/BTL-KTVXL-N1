CC ?= gcc
BUILD_DIR ?= build
CFLAGS = -Wall -Wextra -O2 -Iinclude -Isrc -Isrc/lib -Isrc/lib/shared -Isrc/lib/drivers -Itests/modules
LDFLAGS = -lm

ARM_CC = arm-none-eabi-gcc
ARM_OBJCOPY = arm-none-eabi-objcopy
ARM_CFLAGS = -mcpu=cortex-m3 -mthumb -Os -ffreestanding -fdata-sections -ffunction-sections -Wall -Wextra
ARM_LDFLAGS = -Tsrc/lib/stm32/STM32F103C8TX_FLASH.ld -nostdlib -Wl,--gc-sections

LIB_SHARED_SRCS = src/lib/shared/filter.c src/lib/shared/protocol.c src/lib/shared/storage_format.c
LIB_DRIVER_SRCS = src/lib/drivers/mpu6050.c src/lib/drivers/w25qxx.c src/lib/drivers/uart.c
LIB_SRCS = $(LIB_SHARED_SRCS) $(LIB_DRIVER_SRCS)

TEAM_SRCS = src/lib/shared/angle_filter.c \
            src/lib/shared/calibration.c \
            src/lib/shared/uart_cmd.c \
            src/lib/drivers/system_time.c \
            src/lib/drivers/display_system.c \
            src/lib/drivers/flash_config.c \
            src/lib/drivers/ili9341.c

TEST_MODULE_SRCS = tests/modules/test_filter.c \
                   tests/modules/test_protocol.c \
                   tests/modules/test_storage.c \
                   tests/modules/test_mpu6050.c \
                   tests/modules/test_w25qxx.c \
                   tests/modules/test_uart.c \
                   tests/modules/test_hardware_contract.c

TEST_SRCS = tests/test_runner.c $(TEST_MODULE_SRCS) $(LIB_SRCS)
COMPONENT_SRCS = tests/component/test_pipeline_integration.c $(LIB_SRCS)
STRESS_SRCS = tests/stress/stress_test_runner.c $(LIB_SRCS)
LEGACY_TEST_SRCS = tests/test_firmware_core.c $(LIB_SRCS)

MAIN_SRCS = src/main.c $(LIB_SRCS) $(TEAM_SRCS)
STM32_BRINGUP_SRCS = src/lib/stm32/startup_stm32f103c8tx.s src/lib/stm32/main.c

.PHONY: all test test-modules test-component test-stress test-hw test-hw-single test-hw-combined test-hw-all test-hw-diag test-hw-diag-watch test-hw-flash test-hw-watch hw-test flash-hw-test sim stm32-bringup flash-stm32-bringup firmware flash-firmware flash-putty putty monitor clean

all: test test-component test-stress sim

monitor:
	python3 scripts/monitor.py

putty:
	@./scripts/linux/launch_putty.sh

flash-putty: flash-firmware putty

test-hw: test-hw-all

test-hw-single:
	python3 scripts/linux/agent_hardware_test.py --mode single

test-hw-combined:
	python3 scripts/linux/agent_hardware_test.py --mode combined

test-hw-all:
	python3 scripts/linux/agent_hardware_test.py --mode all

test-hw-diag:
	python3 scripts/linux/agent_hardware_test.py --mode diag

test-hw-diag-watch:
	python3 scripts/linux/agent_hardware_test.py --mode diag --watch

test-hw-flash:
	python3 scripts/linux/agent_hardware_test.py --mode all --flash

test-hw-watch:
	python3 scripts/linux/agent_hardware_test.py --mode all --flash --watch

test: build/test_runner build/test_firmware_core
	./build/test_runner
	./build/test_firmware_core

test-modules: build/test_runner
	./build/test_runner

test-component: build/component_runner
	./build/component_runner

test-stress: build/stress_runner
	./build/stress_runner

sim: build/tilt_sensor_sim
	./build/tilt_sensor_sim

stm32-bringup: build/stm32_bringup.bin

flash-stm32-bringup: build/stm32_bringup.bin
	st-flash --reset write $< 0x08000000

build/test_runner: $(TEST_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/test_firmware_core: $(LEGACY_TEST_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/component_runner: $(COMPONENT_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/stress_runner: $(STRESS_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/tilt_sensor_sim: $(MAIN_SRCS)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

build/stm32_bringup.elf: $(STM32_BRINGUP_SRCS)
	@mkdir -p build
	$(ARM_CC) $(ARM_CFLAGS) $(ARM_LDFLAGS) -o $@ $^ -lgcc -lm

build/stm32_bringup.bin: build/stm32_bringup.elf
	$(ARM_OBJCOPY) -O binary $< $@

FIRMWARE_ELF = $(BUILD_DIR)/firmware/firmware.elf
FIRMWARE_BIN = $(BUILD_DIR)/firmware/firmware.bin
FIRMWARE_HEX = $(BUILD_DIR)/firmware/firmware.hex

SPL_DIR ?= third_party/stm32f10x
SPL_SRC_DIR = $(SPL_DIR)/Libraries/STM32F10x_StdPeriph_Driver/src
SPL_INC_DIR = $(SPL_DIR)/Libraries/STM32F10x_StdPeriph_Driver/inc

FIRMWARE_INC = -Isrc -Isrc/lib/shared -Isrc/lib/drivers -I$(SPL_INC_DIR) \
               -I$(SPL_DIR)/Libraries/CMSIS/CM3/CoreSupport \
               -I$(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x
FIRMWARE_CFLAGS = -mcpu=cortex-m3 -mthumb -Os -Wall -Wextra -ffunction-sections -fdata-sections \
                  -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER $(FIRMWARE_INC)
FIRMWARE_LDFLAGS = -Tsrc/lib/stm32/STM32F103C8TX_FLASH.ld --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections

FIRMWARE_C_SRCS = src/main.c \
                  src/lib/drivers/ili9341.c \
                  src/lib/drivers/display_system.c \
                  src/lib/drivers/mpu6050.c \
                  src/lib/drivers/flash_config.c \
                  src/lib/drivers/system_time.c \
                  src/lib/shared/angle_filter.c \
                  src/lib/shared/calibration.c \
                  src/lib/shared/uart_cmd.c
FIRMWARE_STARTUP = "$(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/startup/TrueSTUDIO/startup_stm32f10x_md.s"

firmware: $(FIRMWARE_BIN) $(FIRMWARE_HEX)
	@echo "--> Firmware build complete:"
	@arm-none-eabi-size $(FIRMWARE_ELF)

$(FIRMWARE_BIN): $(FIRMWARE_ELF)
	$(ARM_OBJCOPY) -O binary $< $@

$(FIRMWARE_HEX): $(FIRMWARE_ELF)
	$(ARM_OBJCOPY) -O ihex $< $@

FIRMWARE_OBJS = $(addprefix $(BUILD_DIR)/firmware/,startup.o main.o ili9341.o \
                display_system.o mpu6050.o flash_config.o system_time.o \
                angle_filter.o calibration.o uart_cmd.o misc.o stm32f10x_gpio.o \
                stm32f10x_rcc.o stm32f10x_spi.o stm32f10x_tim.o stm32f10x_usart.o \
                system_stm32f10x.o)
FIRMWARE_HEADERS = $(wildcard src/lib/drivers/*.h src/lib/shared/*.h $(SPL_INC_DIR)/*.h) \
                   $(wildcard $(SPL_DIR)/Libraries/CMSIS/CM3/CoreSupport/*.h) \
                   $(wildcard $(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/*.h)
FIRMWARE_SPL_SRCS = $(addprefix $(SPL_SRC_DIR)/,misc.c stm32f10x_gpio.c \
                    stm32f10x_rcc.c stm32f10x_spi.c stm32f10x_tim.c stm32f10x_usart.c)

$(FIRMWARE_ELF): $(FIRMWARE_C_SRCS) $(FIRMWARE_SPL_SRCS) $(FIRMWARE_HEADERS) \
                $(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c \
                $(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/startup/TrueSTUDIO/startup_stm32f10x_md.s \
                src/lib/stm32/STM32F103C8TX_FLASH.ld Makefile
	@mkdir -p "$(BUILD_DIR)/firmware"
	$(ARM_CC) $(FIRMWARE_CFLAGS) -c $(FIRMWARE_STARTUP) -o $(BUILD_DIR)/firmware/startup.o
	@set -e; for source in $(FIRMWARE_C_SRCS) $(FIRMWARE_SPL_SRCS) \
	    $(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c; do \
	    object=$$(basename "$$source" .c); \
	    $(ARM_CC) $(FIRMWARE_CFLAGS) -c "$$source" -o "$(BUILD_DIR)/firmware/$$object.o"; \
	done
	$(ARM_CC) $(FIRMWARE_CFLAGS) $(FIRMWARE_LDFLAGS) -Wl,-Map=$(BUILD_DIR)/firmware/firmware.map -o $@ $(FIRMWARE_OBJS) -lm

flash-firmware: $(FIRMWARE_BIN)
	st-flash --reset write $< 0x08000000

HW_TEST_ELF = $(BUILD_DIR)/firmware/hw_diagnostic_test.elf
HW_TEST_BIN = $(BUILD_DIR)/firmware/hw_diagnostic_test.bin

hw-test: $(HW_TEST_BIN)
	@echo "--> Target Hardware Diagnostic Test binary build complete:"
	@arm-none-eabi-size $(HW_TEST_ELF)

$(HW_TEST_BIN): $(HW_TEST_ELF)
	$(ARM_OBJCOPY) -O binary $< $@

$(HW_TEST_ELF): $(FIRMWARE_ELF) src/hw_diagnostic_test.c
	@mkdir -p "$(BUILD_DIR)/firmware"
	$(ARM_CC) $(FIRMWARE_CFLAGS) -c src/hw_diagnostic_test.c -o $(BUILD_DIR)/firmware/hw_diagnostic_test.o
	$(ARM_CC) $(FIRMWARE_CFLAGS) $(FIRMWARE_LDFLAGS) -Wl,-Map=$(BUILD_DIR)/firmware/hw_diagnostic_test.map -o $@ \
		$(BUILD_DIR)/firmware/hw_diagnostic_test.o \
		$(BUILD_DIR)/firmware/startup.o \
		$(BUILD_DIR)/firmware/system_stm32f10x.o \
		$(BUILD_DIR)/firmware/misc.o \
		$(BUILD_DIR)/firmware/stm32f10x_gpio.o \
		$(BUILD_DIR)/firmware/stm32f10x_rcc.o \
		$(BUILD_DIR)/firmware/stm32f10x_spi.o \
		$(BUILD_DIR)/firmware/stm32f10x_tim.o \
		$(BUILD_DIR)/firmware/stm32f10x_usart.o \
		$(BUILD_DIR)/firmware/system_time.o \
		$(BUILD_DIR)/firmware/mpu6050.o \
		$(BUILD_DIR)/firmware/ili9341.o \
		$(BUILD_DIR)/firmware/uart_cmd.o \
		-lm

flash-hw-test: $(HW_TEST_BIN)
	st-flash --reset write $< 0x08000000

clean:
	rm -rf build

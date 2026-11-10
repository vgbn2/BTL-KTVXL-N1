BUILD_DIR ?= build
SPL_DIR   ?= third_party/stm32f10x

ARM_CC      = arm-none-eabi-gcc
ARM_OBJCOPY = arm-none-eabi-objcopy

SPL_SRC_DIR = $(SPL_DIR)/Libraries/STM32F10x_StdPeriph_Driver/src
SPL_INC_DIR = $(SPL_DIR)/Libraries/STM32F10x_StdPeriph_Driver/inc

BAUD ?= 115200

FIRMWARE_INC = -Isrc -Isrc/lib/shared -Isrc/lib/drivers -I$(SPL_INC_DIR) \
               -I$(SPL_DIR)/Libraries/CMSIS/CM3/CoreSupport \
               -I$(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x
FIRMWARE_CFLAGS = -mcpu=cortex-m3 -mthumb -Os -Wall -Wextra -ffunction-sections -fdata-sections \
                  -DSTM32F10X_MD -DUSE_STDPERIPH_DRIVER -DUART_BAUDRATE=$(BAUD) $(FIRMWARE_INC)
FIRMWARE_LDFLAGS = -Tsrc/lib/stm32/STM32F103C8TX_FLASH.ld -u _printf_float --specs=nano.specs --specs=nosys.specs -Wl,--gc-sections

FIRMWARE_C_SRCS = src/main.c \
                  src/lib/drivers/ili9341.c \
                  src/lib/drivers/display_system.c \
                  src/lib/drivers/mpu6050.c \
                  src/lib/drivers/flash_config.c \
                  src/lib/drivers/uart_cmd.c \
                  src/lib/shared/system_time.c \
                  src/lib/shared/angle_filter.c \
                  src/lib/shared/calibration.c
FIRMWARE_STARTUP = $(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/startup/TrueSTUDIO/startup_stm32f10x_md.s
FIRMWARE_SPL_SRCS = $(addprefix $(SPL_SRC_DIR)/,misc.c stm32f10x_gpio.c \
                    stm32f10x_rcc.c stm32f10x_spi.c stm32f10x_tim.c stm32f10x_usart.c)

FIRMWARE_DIR  = $(BUILD_DIR)/firmware
FIRMWARE_ELF  = $(FIRMWARE_DIR)/firmware.elf
FIRMWARE_BIN  = $(FIRMWARE_DIR)/firmware.bin
FIRMWARE_HEX  = $(FIRMWARE_DIR)/firmware.hex
FIRMWARE_OBJS = $(addprefix $(FIRMWARE_DIR)/,startup.o main.o ili9341.o \
                display_system.o mpu6050.o flash_config.o system_time.o \
                angle_filter.o calibration.o uart_cmd.o misc.o stm32f10x_gpio.o \
                stm32f10x_rcc.o stm32f10x_spi.o stm32f10x_tim.o stm32f10x_usart.o \
                system_stm32f10x.o)
FIRMWARE_HEADERS = $(wildcard src/lib/drivers/*.h src/lib/shared/*.h $(SPL_INC_DIR)/*.h) \
                   $(wildcard $(SPL_DIR)/Libraries/CMSIS/CM3/CoreSupport/*.h) \
                   $(wildcard $(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/*.h)

HW_TEST_ELF = $(FIRMWARE_DIR)/hw_diagnostic_test.elf
HW_TEST_BIN = $(FIRMWARE_DIR)/hw_diagnostic_test.bin

TARGET    ?= firmware
FLASH_BIN  = $(if $(filter hw-test,$(TARGET)),$(HW_TEST_BIN),$(FIRMWARE_BIN))
FLASH_ELF  = $(if $(filter hw-test,$(TARGET)),$(HW_TEST_ELF),$(FIRMWARE_ELF))
TEST_ARGS  = $(if $(filter 1,$(FIRMWARE)),--firmware,) \
             $(if $(filter command line,$(origin BUILD_DIR)),--build-dir "$(BUILD_DIR)",)

.PHONY: all test firmware hw-test flash monitor flash-monitor putty flash-putty picocom flash-picocom clean

all: test firmware

test:
	@bash scripts/agent_test.sh $(TEST_ARGS)

firmware: $(FIRMWARE_BIN) $(FIRMWARE_HEX)
	@echo "--> Firmware build complete:"
	@arm-none-eabi-size $(FIRMWARE_ELF)

$(FIRMWARE_BIN): $(FIRMWARE_ELF)
	$(ARM_OBJCOPY) -O binary $< $@

$(FIRMWARE_HEX): $(FIRMWARE_ELF)
	$(ARM_OBJCOPY) -O ihex $< $@

$(FIRMWARE_ELF): $(FIRMWARE_C_SRCS) $(FIRMWARE_SPL_SRCS) $(FIRMWARE_HEADERS) \
                $(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c \
                $(FIRMWARE_STARTUP) src/lib/stm32/STM32F103C8TX_FLASH.ld Makefile
	@mkdir -p "$(FIRMWARE_DIR)"
	$(ARM_CC) $(FIRMWARE_CFLAGS) -c "$(FIRMWARE_STARTUP)" -o "$(FIRMWARE_DIR)/startup.o"
	@set -e; for source in $(FIRMWARE_C_SRCS) $(FIRMWARE_SPL_SRCS) \
	    $(SPL_DIR)/Libraries/CMSIS/CM3/DeviceSupport/ST/STM32F10x/system_stm32f10x.c; do \
	    object=$$(basename "$$source" .c); \
	    $(ARM_CC) $(FIRMWARE_CFLAGS) -c "$$source" -o "$(FIRMWARE_DIR)/$$object.o"; \
	done
	$(ARM_CC) $(FIRMWARE_CFLAGS) $(FIRMWARE_LDFLAGS) -Wl,-Map=$(FIRMWARE_DIR)/firmware.map -o $@ $(FIRMWARE_OBJS) -lm

hw-test: $(HW_TEST_BIN)
	@echo "--> Target Hardware Diagnostic Test binary build complete:"
	@arm-none-eabi-size $(HW_TEST_ELF)

$(HW_TEST_BIN): $(HW_TEST_ELF)
	$(ARM_OBJCOPY) -O binary $< $@

$(HW_TEST_ELF): $(FIRMWARE_ELF) src/hw_diagnostic_test.c
	@mkdir -p "$(FIRMWARE_DIR)"
	$(ARM_CC) $(FIRMWARE_CFLAGS) -c src/hw_diagnostic_test.c -o $(FIRMWARE_DIR)/hw_diagnostic_test.o
	$(ARM_CC) $(FIRMWARE_CFLAGS) $(FIRMWARE_LDFLAGS) -Wl,-Map=$(FIRMWARE_DIR)/hw_diagnostic_test.map -o $@ \
		$(FIRMWARE_DIR)/hw_diagnostic_test.o \
		$(FIRMWARE_DIR)/startup.o $(FIRMWARE_DIR)/system_stm32f10x.o \
		$(FIRMWARE_DIR)/misc.o $(FIRMWARE_DIR)/stm32f10x_gpio.o \
		$(FIRMWARE_DIR)/stm32f10x_rcc.o $(FIRMWARE_DIR)/stm32f10x_spi.o \
		$(FIRMWARE_DIR)/stm32f10x_tim.o $(FIRMWARE_DIR)/stm32f10x_usart.o \
		$(FIRMWARE_DIR)/system_time.o $(FIRMWARE_DIR)/mpu6050.o \
		$(FIRMWARE_DIR)/ili9341.o $(FIRMWARE_DIR)/uart_cmd.o -lm

flash: $(TARGET)
	@command -v st-info >/dev/null 2>&1 && st-info --probe || true
	@st-flash --reset write $(FLASH_BIN) 0x08000000 || ( \
		command -v openocd >/dev/null 2>&1 && \
		openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program $(FLASH_ELF) verify reset exit" \
	) || { echo "[HINT] No ST-Link? Set BOOT0=1, reset, and run: stm32flash -w $(FLASH_BIN) -v -g 0x08000000 /dev/ttyUSB0" >&2; exit 1; }
	@python3 -B scripts/agent_uart.py --probe-ports
	@if [ "$(UART)" = "1" ]; then \
		PORT="$(PORT)"; \
		if [ -z "$$PORT" ]; then \
			for p in /dev/ttyUSB* /dev/ttyACM*; do [ -e "$$p" ] && { PORT="$$p"; break; }; done; \
		fi; \
		[ -n "$$PORT" ] && python3 -B scripts/agent_uart.py --port "$$PORT" --command GET_ANGLE --timeout 0.5 || { echo "[ERROR] UART=1 requested but no serial check succeeded" >&2; exit 1; }; \
	fi
	@if [ "$(PUTTY)" = "1" ] || [ "$(MONITOR)" = "1" ]; then \
		PORT="$(PORT)"; \
		if [ -z "$$PORT" ]; then \
			for p in /dev/ttyUSB* /dev/ttyACM*; do [ -e "$$p" ] && { PORT="$$p"; break; }; done; \
		fi; \
		if [ -n "$$DISPLAY" ] && command -v putty >/dev/null 2>&1; then \
			bash scripts/linux/launch_putty.sh $$PORT; \
		elif command -v plink >/dev/null 2>&1; then \
			bash scripts/linux/launch_putty.sh --cli $$PORT; \
		elif [ -t 0 ]; then \
			bash scripts/linux/launch_serial.sh; \
		fi; \
	fi

monitor:
	@PORT="$(PORT)"; \
	if [ -z "$$PORT" ]; then \
		for p in /dev/ttyUSB* /dev/ttyACM*; do [ -e "$$p" ] && { PORT="$$p"; break; }; done; \
	fi; \
	if [ -n "$$DISPLAY" ] && command -v putty >/dev/null 2>&1; then \
		bash scripts/linux/launch_putty.sh $$PORT; \
	elif command -v plink >/dev/null 2>&1; then \
		bash scripts/linux/launch_putty.sh --cli $$PORT; \
	else \
		bash scripts/linux/launch_serial.sh; \
	fi

flash-monitor: flash monitor

putty:
	@PORT="$(PORT)"; \
	if [ -z "$$PORT" ]; then \
		for p in /dev/ttyUSB* /dev/ttyACM*; do [ -e "$$p" ] && { PORT="$$p"; break; }; done; \
	fi; \
	BAUD="$(BAUD)" bash scripts/linux/launch_putty.sh $$PORT

flash-putty: flash putty

picocom:
	@PORT="$(PORT)"; \
	if [ -z "$$PORT" ]; then \
		for p in /dev/ttyUSB* /dev/ttyACM*; do [ -e "$$p" ] && { PORT="$$p"; break; }; done; \
	fi; \
	if [ -t 0 ]; then \
		picocom -b $(BAUD) "$$PORT" --echo --imap lfcrlf --emap crcrlf; \
	elif [ -n "$$DISPLAY" ] && command -v gnome-terminal >/dev/null 2>&1; then \
		echo "--> Non-interactive shell detected; spawning and raising picocom in GUI terminal window..."; \
		gnome-terminal --title="BTL_KTVXL picocom ($$PORT)" -- picocom -b $(BAUD) "$$PORT" --echo --imap lfcrlf --emap crcrlf; \
		sleep 0.3; command -v wmctrl >/dev/null 2>&1 && wmctrl -a "BTL_KTVXL picocom" || true; \
	elif [ -n "$$DISPLAY" ] && command -v xterm >/dev/null 2>&1; then \
		echo "--> Non-interactive shell detected; spawning and raising picocom in xterm..."; \
		xterm -T "BTL_KTVXL picocom ($$PORT)" -e picocom -b $(BAUD) "$$PORT" --echo --imap lfcrlf --emap crcrlf; \
		sleep 0.3; command -v wmctrl >/dev/null 2>&1 && wmctrl -a "BTL_KTVXL picocom" || true; \
	else \
		picocom -b $(BAUD) "$$PORT" --echo --imap lfcrlf --emap crcrlf; \
	fi

flash-picocom: flash picocom

clean:
	rm -rf "$(BUILD_DIR)"

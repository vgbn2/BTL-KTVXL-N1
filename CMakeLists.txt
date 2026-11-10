cmake_minimum_required(VERSION 3.16)
project(BTL_KTVXL C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ------------------------------------------------------------------------------
# Include Directories
# ------------------------------------------------------------------------------
include_directories(
    include
    src
    src/lib
    src/lib/shared
    src/lib/drivers
    tests/modules
)

# ------------------------------------------------------------------------------
# Native Host Targets (Unit & Component Tests)
# ------------------------------------------------------------------------------
set(COMMON_HOST_SRCS
    src/lib/shared/filter.c
    src/lib/shared/protocol.c
    src/lib/shared/storage_format.c
    src/lib/drivers/mpu6050.c
    src/lib/drivers/w25qxx.c
    src/lib/drivers/uart.c
)

# 1. Modular Unit Tests
add_executable(test_runner
    tests/test_runner.c
    tests/modules/test_filter.c
    tests/modules/test_protocol.c
    tests/modules/test_storage.c
    tests/modules/test_mpu6050.c
    tests/modules/test_w25qxx.c
    tests/modules/test_uart.c
    tests/modules/test_hardware_contract.c
    ${COMMON_HOST_SRCS}
)
target_link_libraries(test_runner m)

# 2. Firmware Core Tests
add_executable(test_firmware_core
    tests/test_firmware_core.c
    ${COMMON_HOST_SRCS}
)
target_link_libraries(test_firmware_core m)

# 3. Component Pipeline Integration Test
add_executable(component_runner
    tests/component/test_pipeline_integration.c
    ${COMMON_HOST_SRCS}
)
target_link_libraries(component_runner m)

# 4. Stress & Fuzzing Runner
add_executable(stress_runner
    tests/stress/stress_test_runner.c
    ${COMMON_HOST_SRCS}
)
target_link_libraries(stress_runner m)

# ------------------------------------------------------------------------------
# CTest Integration
# ------------------------------------------------------------------------------
enable_testing()
add_test(NAME ModularUnitTests COMMAND test_runner)
add_test(NAME FirmwareCoreTests COMMAND test_firmware_core)
add_test(NAME ComponentTests COMMAND component_runner)
add_test(NAME StressTests COMMAND stress_runner)

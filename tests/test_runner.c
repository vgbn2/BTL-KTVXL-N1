#include <stdio.h>
#include <stdlib.h>

extern int test_filter_module(void);
extern int test_protocol_module(void);
extern int test_storage_module(void);
extern int test_mpu6050_module(void);
extern int test_w25qxx_module(void);
extern int test_uart_module(void);
extern int test_hardware_contract(void);

typedef struct {
    const char *name;
    int (*run)(void);
} test_suite_t;

int main(void) {
    printf("====================================================\n");
    printf("   STM32 TILT SENSOR: MODULAR UNIT TEST SUITE       \n");
    printf("====================================================\n");

    test_suite_t suites[] = {
        {"Filter Module (Complementary Filter)", test_filter_module},
        {"Protocol Module (CRC16 + Telemetry Codec + ASCII Parser)", test_protocol_module},
        {"Storage Module (CRC32 + Sector 0 Block)", test_storage_module},
        {"MPU6050 Driver Module (I2C Burst + Offsets)", test_mpu6050_module},
        {"W25Qxx Driver Module (SPI Nor Flash Sector RW + Config)", test_w25qxx_module},
        {"UART Driver Module (RM0008 Baud Calculation + Buffer)", test_uart_module},
        {"Hardware Contract Verification (RM0008, MPU6050, W25Qxx, ILI9341)", test_hardware_contract}
    };

    size_t num_suites = sizeof(suites) / sizeof(suites[0]);
    int total_failures = 0;
    int passed_suites = 0;

    for (size_t i = 0; i < num_suites; i++) {
        printf("\n[SUITE %zu/%zu] %s\n", i + 1, num_suites, suites[i].name);
        int failures = suites[i].run();
        if (failures == 0) {
            passed_suites++;
        } else {
            total_failures += failures;
        }
    }

    printf("\n====================================================\n");
    if (total_failures == 0) {
        printf("   ALL %zu UNIT TEST SUITES PASSED CLEANLY (100%%) ✅\n", num_suites);
        printf("====================================================\n");
        return 0;
    } else {
        fprintf(stderr, "   UNIT TEST FAILURES DETECTED: %d failed checks in %zu suites ❌\n",
                total_failures, num_suites - passed_suites);
        fprintf(stderr, "====================================================\n");
        return 1;
    }
}

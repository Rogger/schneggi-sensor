#ifndef TEST_CRC_H_
#define TEST_CRC_H_
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
uint8_t crc8(const uint8_t *src, size_t len, uint8_t polynomial,
             uint8_t initial_value, bool reversed);
#endif

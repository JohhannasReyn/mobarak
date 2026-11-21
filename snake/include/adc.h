#pragma once
#include <stdint.h>

void     adc_init();
uint16_t adc_read(uint8_t channel);   // 0..1023, blocks ~104 us

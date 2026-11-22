#pragma once
#include <avr/io.h>

// Hardware SPI master, mode 0, MSB first: 8 MHz (F_CPU/2), or 4 MHz with TFT_SPI_SLOW.
void spi_init();

static inline void spi_write(uint8_t b) {
    SPDR = b;
    while (!(SPSR & (1 << SPIF))) {}
}

// Send one byte and return the one clocked back in (for the touch controller)
static inline uint8_t spi_transfer(uint8_t b) {
    SPDR = b;
    while (!(SPSR & (1 << SPIF))) {}
    return SPDR;
}

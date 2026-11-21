#include "spi.h"
#include "config.h"

void spi_init() {
    // MOSI (PB3), SCK (PB5) and SS (PB2) must be outputs for master mode.
    // PB2 doubles as the TFT's chip select.
    DDRB |= (1 << PB3) | (1 << PB5) | (1 << PB2);
    SPCR  = (1 << SPE) | (1 << MSTR);   // enable, master, mode 0, clk/4 = 4 MHz
#if !TFT_SPI_SLOW
    SPSR  = (1 << SPI2X);               // double speed: clk/2 = 8 MHz
#endif
}

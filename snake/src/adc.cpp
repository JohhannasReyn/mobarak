#include "adc.h"
#include <avr/io.h>

void adc_init() {
    ADMUX  = (1 << REFS0);                                   // AVCC reference
    ADCSRA = (1 << ADEN) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);   // clk/128 = 125 kHz
    DIDR0  = (1 << ADC0D) | (1 << ADC1D);                  // A0-A1 analog only
}

uint16_t adc_read(uint8_t channel) {
    ADMUX = (ADMUX & 0xF0) | (channel & 0x0F);
    ADCSRA |= (1 << ADSC);
    while (ADCSRA & (1 << ADSC)) {}
    return ADC;
}

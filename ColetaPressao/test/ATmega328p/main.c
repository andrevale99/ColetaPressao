#include <stdint.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/delay.h>

uint16_t adc_value = 0;

ISR(ADC_vect);

int main(void)
{
    ADMUX |= (1 << REFS0);
    ADCSRA |= (1 << ADIE) |  (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    ADCSRA |= (1 << ADEN);
    ADCSRA |= (1 << ADSC);

    sei();

    while (1)
    {
    }

    return 0;
}

ISR(ADC_vect)
{
    uint8_t status = SREG;

    adc_value = ADC;

    ADCSRA |= (1 << ADSC);

    SREG = status;
}
#ifndef F_CPU
#define F_CPU 16000000UL
#endif

#include <stdint.h>
#include <stdio.h>

#include <avr/io.h>
#include <avr/interrupt.h>
#include <util/delay.h>

#define SetBit(port, pin) (port |= (1 << pin))
#define ClrBit(port, pin) (port &= ~(1 << pin))
#define ToggleBit(port, pin) (port ^= (1 << pin))
#define TestBit(port, pin) (port & (1 << pin))

#define BAUD 9600
#define MYUBRR F_CPU / 16 / BAUD - 1

//=========================================
//  VARS
//=========================================

volatile uint16_t adc_value = 0;
char buffer[UINT8_MAX];

//=========================================
//  FUNCS
//=========================================

void USART_Init(unsigned int ubrr);
void USART_Transmit(unsigned char data);

ISR(ADC_vect);

//=========================================
// MAIN
//=========================================
int main(void)
{
    ADMUX |= (1 << REFS0);
    ADCSRA |= (1 << ADIE) | (1 << ADPS2) | (1 << ADPS1) | (1 << ADPS0);
    ADCSRA |= (1 << ADEN);
    ADCSRA |= (1 << ADSC);

    USART_Init(MYUBRR);

    sei();

    while (1)
    {
        sprintf(buffer, "%d\n", adc_value);

        for (uint8_t i = 0; buffer[i] != '\0'; i++)
            USART_Transmit(buffer[i]);

        _delay_ms(1);
    }

    return 0;
}

//=========================================
// FUNCS
//=========================================

void USART_Init(unsigned int ubrr)
{
    /*Set baud rate */
    UBRR0H = (unsigned char)(ubrr >> 8);
    UBRR0L = (unsigned char)ubrr;

    /*Enable transmitter
    Desativa a interrupcao Data Empty
    Desativa a interrupcao complete*/
    SetBit(UCSR0B, TXEN0);
    ClrBit(UCSR0B, TXCIE0);
    ClrBit(UCSR0B, UDRIE0);

    /* Set frame format: 8data, 2stop bit */
    SetBit(UCSR0C, USBS0);
    SetBit(UCSR0C, UCSZ00);
}

void USART_Transmit(unsigned char data)
{
    /* Wait for empty transmit buffer */
    while (!(UCSR0A & (1 << UDRE0)))
        ;
    /* Put data into buffer, sends the data */
    UDR0 = data;
}

ISR(ADC_vect)
{
    uint8_t status = SREG;

    adc_value = ADC;

    ADCSRA |= (1 << ADSC);

    SREG = status;
}
/* oscilloscopio_trigger.c
 * Compila con: avr-gcc -mmcu=atmega328p -Os -o scope.elf oscilloscopio_trigger.c
 *              avr-objcopy -O ihex scope.elf scope.hex
 */

#include <avr/io.h>
#include <avr/interrupt.h>
#include <string.h>
#include <stdbool.h>

#define NSAMPLES 256
volatile uint16_t buf[NSAMPLES];
volatile uint16_t idx         = 0;
volatile bool     buf_ready   = false;
volatile bool     capturing   = false;

/* === Parametri campionamento === */
#define FS_HZ      40000UL      /* frequenza di campionamento desiderata     */
#define F_CPU_HZ   16000000UL   /* clock MCU                                 */
#define PRESC      8            /* prescaler scelto per Timer1               */
#define OCR1A_VAL ((F_CPU_HZ/(PRESC*FS_HZ))-1)

/* === Funzioni di supporto ========================================== */
static void uart_init(uint32_t baud)
{
    uint16_t ubrr = (F_CPU_HZ/16/baud) - 1;
    UBRR0H = (ubrr >> 8);
    UBRR0L = ubrr;
    UCSR0B = (1<<TXEN0);                /* solo trasmissione               */
}

static void uart_write(const void *data, uint16_t len)
{
    const uint8_t *p = (const uint8_t*)data;
    while (len--) {
        while (!(UCSR0A & (1<<UDRE0)));
        UDR0 = *p++;
    }
}

/* === Interrupt: esterno su INT0 (pin D2) =========================== */
ISR(INT0_vect)
{
    if (!capturing) {                   /* start capture su fronte di salita */
        idx       = 0;
        capturing = true;
        buf_ready = false;
        ADCSRA   |= (1<<ADSC) | (1<<ADIE);  /* abilita ADC + interrupt */
    }
}

/* === Interrupt: ADC completo ======================================= */
ISR(ADC_vect)
{
    buf[idx++] = ADC;                   /* 10 bit, già allineato a destra   */
    if (idx >= NSAMPLES) {
        capturing   = false;
        buf_ready   = true;
        ADCSRA     &= ~(1<<ADIE);       /* stop ADC interrupt               */
    }
}

/* === Inizializzazione hardware ===================================== */
static void adc_timer_init(void)
{
    /* --- Timer1 in CTC ------------------------------------------------ */
    TCCR1A = 0;
    TCCR1B = 0;
    OCR1A  = (uint16_t)OCR1A_VAL;
    TCCR1B = (1<<WGM12) | (1<<CS11);    /* CTC, prescaler = 8              */

    /* --- ADC ---------------------------------------------------------- */
    ADMUX  = (1<<REFS0);                /* riferimento AVcc, canale 0 (A0) */
    ADCSRA = (1<<ADEN) | (1<<ADATE) | (1<<ADPS2);  /* ADC on, auto-trigger, presc = 16 */
    ADCSRB = (1<<ADTS2)|(1<<ADTS0);     /* trigger = Timer1 Compare A      */

    /* --- Trigger esterno INT0 (rising edge) --------------------------- */
    EICRA = (1<<ISC01)|(1<<ISC00);      /* fronte di salita                */
    EIFR  = (1<<INTF0);                 /* clear eventuali flag pendenti   */
    EIMSK = (1<<INT0);                  /* abilita INT0                    */
}

int main(void)
{
    uart_init(500000);
    adc_timer_init();
    sei();                              /* global interrupt enable         */

    for (;;) {
        if (buf_ready) {
            /* copia locale per ridurre tempo in sezione critica */
            uint16_t local[NSAMPLES];
            cli();
            memcpy(local, (void*)buf, sizeof(local));
            buf_ready = false;
            sei();

            /* trasmetti in binario little-endian */
            uart_write(local, sizeof(local));
        }
    }
}

/****************************************************************************
 * STC15W204S DTMF †o Parallel Relay Driver for
 * OKI AC125A Crossbar PBX Touch-Tone Adapter
 *
 * Copyright (c) 2019, Howard M. Harte
 *
 * Uses the readily-available AD22A08 DTMF to Relay PCB.
 * Compile with Small Device C Compiler (SDCC)
 ****************************************************************************/

#include <stc12.h>
#include <stdint.h>
#include <stdio.h>

#define printf printf_small     // Regular printf() will not fit in STC15 RAM, use small.

SFR(AUXR1, 0xA2);
SFR(T2H, 0xD6);
SFR(T2L, 0xD7);
SBIT(P5_4, 0xC8, 4);
SBIT(P5_5, 0xC8, 5);

/* AD22A08 PCB GPIOs */
#define LED         P1_2
#define BUTTON      P1_3

/* MT8870D DTMF Decoder GPIOs */
#define DTMF_Q1     P1_0
#define DTMF_Q2     P3_7
#define DTMF_Q3     P3_6
#define DTMF_Q4     P3_3
#define DTMF_ST_D   P3_2

/* SN74HC595 8-Bit Shift Register GPIOs */
#define SRCLK       P5_5    /* Shift register clock */
#define RCLK        P5_4    /* Register strobe */
#define OE_         P1_5    /* Output Enable */
#define SER         P1_4    /* Serial Data */

/* The interface from the OKI Originating Registers (ORs) consists
 * of a STart output which is grounded when the B relay in the OR
 * operates.  We will not use that in this design, although it would
 * be nice to use it to reset the DTMF converter.
 *
 * Additionally, there are six outputs from the DTMF converter to
 * the OKI OR.  Five are pulse counting relay outputs (PA-PE) and the
 * sixth is a C relay output, used to store the pulse counting relays
 * into the reed relays of the OR.
 */
#define RELAY_PA    (1 << 0)
#define RELAY_PB    (1 << 1)
#define RELAY_PC    (1 << 2)
#define RELAY_PD    (1 << 3)
#define RELAY_PE    (1 << 4)
#define RELAY_C     (1 << 5)

/* Function Prototypes */
void _delay_ms(unsigned short ms);
void pio_write(unsigned char data);
unsigned char relay_fixup(unsigned char c);
void operate_relays(unsigned char c);
void InitUART();
int putchar(int);

/* DTMF Digit to OKI OR Pulse Counting Relay Mapping */
const unsigned char pc_relay_table[16] = { 
    0,                                                      // Not valid
    RELAY_PA | RELAY_PB,                                    // 1
    RELAY_PC,                                               // 2
    RELAY_PA | RELAY_PB | RELAY_PC | RELAY_PD,              // 3
    RELAY_PC | RELAY_PD,                                    // 4
    RELAY_PA | RELAY_PB | RELAY_PD,                         // 5
    RELAY_PE,                                               // 6
    RELAY_PA | RELAY_PB | RELAY_PE,                         // 7
    RELAY_PC | RELAY_PE,                                    // 8
    RELAY_PA | RELAY_PB | RELAY_PC | RELAY_PD | RELAY_PE,   // 9
    RELAY_PC | RELAY_PD | RELAY_PE,                         // 0
    0, 0, 0, 0, 0                                           // Not valid
};

volatile unsigned char dtmf_data = 0;
volatile unsigned char busy = 0;

int main()
{
    unsigned char dtmf_digit;
    unsigned char pc_relays;

    LED = 0;

    P1M0 = 0b00000000;
    P1M1 = 0b00000001;  // P1.0 as input.

    P3M0 = 0b00000000;
    P3M1 = 0b11001100;  // P3.7, P3.6, P3.3, P3.2 as input.

    INT0 = 1;
    IT0 = 0;            // Both falling and rising edges.
    EX0 = 1;            // Enable external interrupt 0

    /* init the software uart */
    InitUART();

    puts("OKI AC125A Parallel DTMF Decoder\n\r(c) 2019 Howard M. Harte\n\r");

    while(1)
    {                
        LED = 0;

        if (dtmf_data > 0) {
            dtmf_digit = dtmf_data;
            dtmf_data = 0;
            printf("DTMF received: %d\t\n\r", dtmf_digit);
            pc_relays = pc_relay_table[dtmf_digit];

            /* If valid (0-9) DTMF digit, set appropriate relays */
            if (pc_relays > 0) {
                /* Set PA-PE and C relays */
                operate_relays(pc_relays | RELAY_C);
                _delay_ms(500);

                _delay_ms(500);
                operate_relays(0);
                _delay_ms(500);
            }
        }

        /* Clear watchdog timer */
        WDT_CONTR |= 1 << 4;
    }
}

void int0_isr() __interrupt 0 __using 2
{
    unsigned char c;
    if (DTMF_ST_D == 1) {   // Store DTMF digit to FIFO on rising edge of DTMF_ST_D
        c = DTMF_Q1 |
                    (DTMF_Q2 << 1) |
                    (DTMF_Q3 << 2) |
                    (DTMF_Q4 << 3);
        dtmf_data = c;
    }
}

int putchar(int c)
{
    while (busy);
    busy = 1;
    SBUF = c;

    return 0;
}

void InitUART()
{
    SCON = 0x50;    // 8-bit variable baud rate.
    T2L = 0xE1;     // Timer2 values for 9600 baud
    T2H = 0xFE;
    AUXR = 0x14;    // T2 in 1T Mode, and start T2
    AUXR |= 0x01;   // T2 as UART1 baud rate generator
    ES = 1;         // Enable UART1 interrupt
    EA = 1;         // Enable interrupts
    busy = 0;
}

void uart_isr() __interrupt 4 __using 1
{
    if (RI) {
        RI = 0;
    }
    if (TI) {
        TI = 0;
        busy = 0;
    }
}

// Relay drivers connected to 74HC595 are not in the correct order on the PCB,
// remap to proper order.
// 7, 6, 5, 4, 3, 2, 8, 1
unsigned char relay_fixup(unsigned char c)
{
    return ((c & 1) | ((c & 2) << 6) | ((c & 0xFC) >> 1));
}

void operate_relays(unsigned char c)
{
    pio_write(relay_fixup(c));
}       

/* Write byte to 74HC595 Shift Register */
void pio_write(unsigned char data)
{
    int i;
    SRCLK = 0;
    RCLK = 0;

    for (i=0; i < 8; i++) {
        SER = data & 1;
        data >>= 1;
        SRCLK = 1;
        SRCLK = 0;
    }   

    RCLK = 1;
    RCLK = 0;
    OE_ = 0;    

}

void _delay_ms(unsigned short ms)
{   
    unsigned char i, j;
    do {
        i = 4;
        j = 200;
        do
        {
            while (--j);
        } while (--i);
    } while (--ms);
}

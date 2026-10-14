#include <msp430.h>
#include <stdio.h>
#include "LiquidCrystal_I2C.h"

#define PWM_PERIOD 500u
#define MOTOR_PIN BIT0
#define MOTOR_DIR BIT6
#define LCD_ADDRESS 0x27

static volatile unsigned int duty_percent = 0;

static const char keys[4][4] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

static void delay_ms(unsigned int ms)
{
    while (ms--) {
        __delay_cycles(1000);  // Assumes a 1 MHz CPU clock.
    }
}

static char read_key(void)
{
    unsigned int row, col;

    for (row = 0; row < 4; row++) {
        P3OUT = (unsigned char)(0xF0 | (0x0F & ~(1u << row)));
        for (col = 0; col < 4; col++) {
            unsigned char column = (unsigned char)(BIT4 << col);
            if (!(P3IN & column)) {
                delay_ms(20);
                if (P3IN & column) {
                    continue;
                }
                while (!(P3IN & column)) { }
                delay_ms(20);
                P3OUT = 0xFF;
                return keys[row][col];
            }
        }
    }
    P3OUT = 0xFF;
    return 0;
}

static void set_speed(unsigned int percent)
{
    if (percent > 100) {
        percent = 100;
    }

    // Set both the duty and compare interrupt together to avoid edge glitches.
    __disable_interrupt();
    duty_percent = percent;
    TB0CCTL1 &= ~CCIE;
    if (percent == 0) {
        P4OUT &= ~MOTOR_PIN;
    } else if (percent == 100) {
        P4OUT |= MOTOR_PIN;
    } else {
        TB0CCR1 = (unsigned int)((PWM_PERIOD * percent) / 100u);
        TB0CCTL1 &= ~CCIFG;
        TB0CCTL1 |= CCIE;
    }
    __enable_interrupt();
}

int main(void)
{
    char display[17];
    unsigned int input = 0;
    unsigned int digits = 0;
    char key;

    WDTCTL = WDTPW | WDTHOLD;
    P3DIR = 0x0F;
    P3REN = 0xF0;
    P3OUT = 0xFF;

    P4DIR |= MOTOR_PIN | MOTOR_DIR;
    P4OUT = (P4OUT & ~MOTOR_PIN) | MOTOR_DIR;
    PM5CTL0 &= ~LOCKLPM5;

    TB0CCR0 = PWM_PERIOD - 1;
    TB0CCR1 = 0;
    TB0CCTL0 = CCIE;
    TB0CCTL1 = 0;
    TB0CTL = TBSSEL__ACLK | MC__UP | TBCLR;
    __enable_interrupt();

    I2C_Init(LCD_ADDRESS);
    LCD_Setup();

    while (1) {
        LCD_ClearDisplay();
        sprintf(display, "Speed=%u%%", duty_percent);
        LCD_SetCursor(0, 0);
        LCD_Write(display);
        LCD_SetCursor(0, 1);
        LCD_Write("Input:");
        LCD_SetCursor(6, 1);
        input = 0;
        digits = 0;

        while (1) {
            key = read_key();
            if (key == '#') {
                if (digits > 0) {
                    set_speed(input);
                }
                break;
            }
            if (key >= '0' && key <= '9' && digits < 3) {
                input = input * 10u + (unsigned int)(key - '0');
                LCD_WriteNum(key - '0');
                digits++;
            }
        }
    }
}

#pragma vector = TIMER0_B0_VECTOR
__interrupt void timer_period(void)
{
    if (duty_percent == 0) {
        P4OUT &= ~MOTOR_PIN;
    } else {
        P4OUT |= MOTOR_PIN;
    }
}

#pragma vector = TIMER0_B1_VECTOR
__interrupt void timer_compare(void)
{
    switch (__even_in_range(TB0IV, TB0IV_TBIFG)) {
    case TB0IV_TBCCR1:
        if (duty_percent < 100) {
            P4OUT &= ~MOTOR_PIN;
        }
        break;
    default:
        break;
    }
}
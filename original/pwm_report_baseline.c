// Reconstructed from the source printed in the assignment report.
// The printed listing is preserved, including its original assumptions.

#include <msp430.h>
#include "LiquidCrystal_I2C.h"
#include <stdio.h>
#include <string.h>
// Defines the key port and column detection macros
#define keyport P3OUT
#define COL1 (P3IN & BIT4)
#define COL2 (P3IN & BIT5)
#define COL3 (P3IN & BIT6)
#define COL4 (P3IN & BIT7)
unsigned int index, keyCounter, currentKey = 0;
unsigned char keyValue[] = {
    ' ',  // No key mapped to this index
    '1', '2', '3', 'A',   // Values for first row of keys
    '4', '5', '6', 'B',   // Values for second row of keys
    '7', '8', '9', 'C',   // Values for third row of keys
    '*', '0', '#', 'D'    // Values for fourth row of keys
};
// Function to detect which key is pressed
unsigned char get_key(void) {
    keyCounter = 1;  // Initialize key counter
    for (index = 0; index < 4; index++) {
        keyport = ~ (0x01 << index);  // Set one row low at a time
        if (!COL1 || !COL2 || !COL3 || !COL4) {
            if (!COL1) currentKey = keyCounter;
            else if (!COL2) currentKey = keyCounter + 1;
            else if (!COL3) currentKey = keyCounter + 2;
            else if (!COL4) currentKey = keyCounter + 3;
            while (!(P3IN & (BIT4 | BIT5 | BIT6 | BIT7)));  // Wait for all keys to be released
            delay_ms(300);  // Debounce time to avoid multiple reads
            return keyValue[currentKey];  // Return character of the pressed key
        }
        keyCounter += 4;  // Move to next group of keys
    }
    return 0;  // Return 0 if no key is pressed
}
// Delay function in milliseconds
void delay_ms(unsigned int ms) {
    while (ms--) __delay_cycles(1000);
}
unsigned int pwmDutyCycle = 0;  // Initialize duty cycle to 0%
// Main function
void main(void) {
    WDTCTL = WDTPW | WDTHOLD;  // Stop the watchdog timer
    // Configure port 3 for keypad
    P3DIR = 0x0F;  // Set lower bits as outputs, upper as inputs
    P3REN = 0xFF;  // Enable pull-up/pull-down resistors for all pins
    P3OUT = 0xF0;  // Enable pull-ups for upper pins
    // Configure port 4 for motor control
    P4DIR |= BIT0 + BIT6 + BIT7;
    P4OUT &= ~(BIT0 + BIT6 + BIT7);  // Clear bits first
    P4OUT |= BIT6;  // Set BIT6 to define motor direction
    PM5CTL0 &= ~LOCKLPM5;  // Disable high impedance mode
    // Configure Timer B
    TB0CTL |= TBCLR;  // Clear timer settings
    TB0CTL |= MC__UP;  // Set up mode
    TB0CTL |= TBSSEL__ACLK;  // Use ACLK as timer clock source
    TB0CCR0 = 500;  // Set timer period for PWM
    TB0CCR1 = 0;  // Start with duty cycle of 0%
    // Enable timer interrupts
    TB0CCTL0 |= CCIE;
    TB0CCTL1 |= CCIE;
    TB0CCTL0 &= ~(CCIFG);
    TB0CCTL1 &= ~(CCIFG);
    __enable_interrupt();  // Enable global interrupts
    // Initialize LCD
    I2C_Init(0x27);
    LCD_Setup();
    LCD_SetCursor(0, 0);
    while (1) {
        LCD_ClearDisplay();
        char displayBuffer[20];
        sprintf(displayBuffer, "Speed=%d%%", pwmDutyCycle);  // Prepare the speed display string
        LCD_SetCursor(0, 0);
        LCD_Write(displayBuffer);
        LCD_SetCursor(0, 1);
        LCD_Write("Input:");
        unsigned char numKey = '0';
        unsigned char numKeys[8] = {0};
        pwmDutyCycle = 0;
        unsigned int numCounter = 0;
        LCD_SetCursor(6, 1);  // Position cursor after "Input:"
        do {
            numKey = get_key();  // Read key press
            if (numKey >= '0' && numKey <= '9') {
                LCD_WriteNum((int)numKey - '0');  // Display digit
                numKeys[numCounter++] = numKey;  // Store the digit
                LCD_SetCursor(6 + numCounter, 1);  // Move cursor
            }
        } while (numKey != '#');
        numKeys[numCounter] = '\0';  // Null terminate the array
        sscanf((char*)numKeys, "%d", &pwmDutyCycle);  // Convert string to number
        pwmDutyCycle = (pwmDutyCycle > 100) ? 100 : pwmDutyCycle;  // Limit to max 100%
        TB0CCR1 = (pwmDutyCycle == 100) ? TB0CCR0 - 1 : pwmDutyCycle * 5;  // Set PWM duty cycle
    }
}
// Timer interrupt service routines
#pragma vector = TIMER0_B0_VECTOR
__interrupt void TMR0() {
    P4OUT &= ~1;  // Turn off motor control pin
    TB0CCTL0 &= ~(CCIFG);  // Clear interrupt flag
}
#pragma vector = TIMER0_B1_VECTOR
__interrupt void TMR1() {
    P4OUT |= 1;  // Turn on motor control pin
    TB0CCTL1 &= ~(CCIFG);  // Clear interrupt flag
}
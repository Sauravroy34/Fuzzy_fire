// PIC16F877A Configuration Bit Settings
#pragma config FOSC = HS        // HS oscillator
#pragma config WDTE = OFF       // Watchdog Timer disabled
#pragma config PWRTE = OFF      // Power-up Timer disabled
#pragma config BOREN = ON       // Brown-out Reset enabled
#pragma config LVP = OFF        // Low-Voltage Programming disabled
#pragma config CPD = OFF        // Data EEPROM code protection off
#pragma config WRT = OFF        // Flash Program Memory Write protection off
#pragma config CP = OFF         // Flash Program Memory Code protection off

#define _XTAL_FREQ 16000000
#define TMR2PRESCALE 4
#include <xc.h>
#include <stdint.h> // For uint32_t


long PWM_freq = 5000;

// PWM Initialization
void PWM_Initialize() {
    PR2 = (unsigned char)((_XTAL_FREQ / (PWM_freq * 4 * TMR2PRESCALE)) - 1); // Cast to fix warning
    CCP1M3 = 1; CCP1M2 = 1;  // Configure CCP1 for PWM
    T2CKPS0 = 1; T2CKPS1 = 0; TMR2ON = 1; // Timer2 settings
    TRISC2 = 0; // RC2/CCP1 as output
}

// PWM Duty Cycle
void PWM_Duty(unsigned int duty) {
    if (duty < 1023) {
        duty = (unsigned int)(((uint32_t)duty * (_XTAL_FREQ / TMR2PRESCALE)) / ((uint32_t)1023 * PWM_freq)); // Fix sign conversion
        CCP1X = duty & 1; // Store LSB
        CCP1Y = (duty & 2) >> 1; // Store second bit
        CCPR1L = (unsigned char)(duty >> 2); // Cast to fix warning
    }
}

// Servo Rotations
void Rotation0() { // 0 Degree
    unsigned int i;
    for (i = 0; i < 50; i++) {
        PORTB = 1;
        __delay_us(800);
        PORTB = 0;
        __delay_us(19200);
    }
}

void Rotation90() { // 90 Degree
    unsigned int i;
    for (i = 0; i < 50; i++) {
        PORTB = 1;
        __delay_us(1500);
        PORTB = 0;
        __delay_us(18500);
    }
}

void Rotation180() { // 180 Degree
    unsigned int i;
    for (i = 0; i < 50; i++) {
        PORTB = 1;
        __delay_us(2200);
        PORTB = 0;
        __delay_us(17800);
    }
}

// ADC Initialization
void ADC_Initialize() {
    ADCON0 = 0b00000001; // ADC ON, Fosc/16
    ADCON1 = 0b10000000; // Internal reference, all analog inputs
}

// ADC Read
unsigned int read_adc(unsigned char channel) {
    ADCON0bits.CHS = channel; // Select channel
    __delay_us(20); // Acquisition time
    GO_nDONE = 1; // Start conversion
    while (GO_nDONE); // Wait for completion
    return ((unsigned int)(ADRESH << 8) + ADRESL); // Fix warning
}

// ... (Keep all non-FIS code: configuration, PWM, ADC, servo functions, includes <xc.h>, <stdint.h>)
// Remove #include "fire_fis.h" and "qfis.h"

// Simplified Manual FIS
static const int16_t mf_params[9][4] = { // ROM: 9 MFs, up to 4 params each
    {-384, -43, 259, 514}, // flame_strength_low (trapmf)
    {259, 514, 765, 0},    // flame_strength_Medium (trimf, 0 padding)
    {514, 765, 1067, 1408}, // flame_strength_High (trapmf)
    {-384, -43, 259, 514}, // smoke_strength_low (trapmf)
    {259, 514, 765, 0},    // smoke_strength_Medium (trimf)
    {514, 765, 1067, 1408}, // smoke_strength_High (trapmf)
    {-384, -43, 259, 514}, // output1_Low (trapmf)
    {259, 514, 765, 0},    // output1_Medium (trimf)
    {515, 765, 1067, 1408}  // output1_High (trapmf)
};

int16_t trapmf(int16_t x, const int16_t *p) {
    if (x <= p[0] || x >= p[3]) return 0;
    if (x >= p[1] && x <= p[2]) return 1000; // Scale to 0?1000
    if (x > p[0] && x < p[1]) return (x - p[0]) * 1000 / (p[1] - p[0]);
    if (x > p[2] && x < p[3]) return (p[3] - x) * 1000 / (p[3] - p[2]);
    return 0;
}

int16_t trimf(int16_t x, const int16_t *p) {
    if (x <= p[0] || x >= p[2]) return 0;
    if (x == p[1]) return 1000;
    if (x > p[0] && x < p[1]) return (x - p[0]) * 1000 / (p[1] - p[0]);
    if (x > p[1] && x < p[2]) return (p[2] - x) * 1000 / (p[2] - p[1]);
    return 0;
}

int16_t eval_fis(int16_t flame, int16_t smoke) {
    // Membership degrees (scaled 0?1000)
    int16_t mu_flame[3], mu_smoke[3];
    mu_flame[0] = trapmf(flame, mf_params[0]); // low
    mu_flame[1] = trimf(flame, mf_params[1]);  // Medium
    mu_flame[2] = trapmf(flame, mf_params[2]); // High
    mu_smoke[0] = trapmf(smoke, mf_params[3]); // low
    mu_smoke[1] = trimf(smoke, mf_params[4]);  // Medium
    mu_smoke[2] = trapmf(smoke, mf_params[5]); // High

    // Rule strengths
    int16_t rs[6];
    rs[0] = (mu_flame[0] < mu_smoke[2]) ? mu_flame[0] : mu_smoke[2]; // low, High -> High
    rs[1] = (mu_flame[1] < mu_smoke[2]) ? mu_flame[1] : mu_smoke[2]; // Medium, High -> High
    rs[2] = (mu_flame[2] < mu_smoke[2]) ? mu_flame[2] : mu_smoke[2]; // High, High -> High
    rs[3] = (mu_flame[0] < mu_smoke[1]) ? mu_flame[0] : mu_smoke[1]; // low, Medium -> Medium
    rs[4] = (mu_flame[1] < mu_smoke[1]) ? mu_flame[1] : mu_smoke[1]; // Medium, Medium -> Medium
    rs[5] = (mu_flame[0] < mu_smoke[0]) ? mu_flame[0] : mu_smoke[0]; // low, low -> low

    // Simplified defuzzification (weighted average)
    int32_t num = 0, den = 0; // Use int32_t for intermediate calculations
    num += rs[0] * 1024; // High centroid ~1024
    num += rs[1] * 1024;
    num += rs[2] * 1024;
    num += rs[3] * 514;  // Medium centroid ~514
    num += rs[4] * 514;
    num += rs[5] * 0;    // Low centroid ~0
    den = rs[0] + rs[1] + rs[2] + rs[3] + rs[4] + rs[5];
    if (den == 0) return 514; // Default to Medium
    return (int16_t)(num / den);
}

void main() {
    TRISA = 0xFF; // Port A as input (ADC)
    TRISB = 0x00; // Port B as output (servo)
    ADC_Initialize();
    PWM_Initialize();
    PWM_Duty(0);
    
    while (1) {
        int16_t flameL = (int16_t)read_adc(0); // flameL
        int16_t flameR = (int16_t)read_adc(1); // flameC
        int16_t output = eval_fis(flameL,flameR);
        unsigned int duty = (unsigned int)((output * 1023) / 1024);
        PWM_Duty(duty);
        if (flameL > flameR &&output >= 912) {
            Rotation0();
            __delay_ms(50);
            Rotation90();
        } else if (flameR > flameL && output >= 912) {
            Rotation180();
            __delay_ms(50);
            Rotation90();
        } 
        
    }
}



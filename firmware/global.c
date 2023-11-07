#include "global.h"

// used for debugging and tracing execution.  see client's ".getDebugCodes()"
__xdata u32 clock;

void sleepMillis(int ms)
{
    int j;
    while (--ms > 0)
    {
        for (j = 0; j < SLEEPTIMER; j++)
            ; // about 1 millisecond
    }
}

void clock_init(void)
{
    //  SET UP CPU SPEED!  USE 26MHz for CC1110 and 24MHz for CC1111
    // Set the system clock source to HS XOSC and max CPU speed,
    // ref. [clk]=>[clk_xosc.c]
    SLEEP &= ~SLEEP_OSC_PD;
    while (!(SLEEP & SLEEP_XOSC_S))
        ;
    CLKCON = (CLKCON & ~(CLKCON_CLKSPD | CLKCON_OSC)) | CLKSPD_DIV_1;
    while (CLKCON & CLKCON_OSC)
        ;
    SLEEP |= SLEEP_OSC_PD;
    while (!IS_XOSC_STABLE())
        ;

    // FIXME: this should be defined so it works with 24/26mhz
    // setup TIMER 1
    // free running mode
    // time freq:       187.50 for cc1111 / 203.125kHz for cc1110
    CLKCON &= 0xc7; //( ~ 0b00111000);  - clearing out TICKSPD  freq = 24mhz on cc1111, 26mhz on cc1110

    T1CTL |= T1CTL_DIV_128; // T1 running at 187.500 kHz
    T1CTL |= T1CTL_MODE_FREERUN;
    T1IE = 1;
}

/* initialize the IO subsystems for the appropriate dongles */
void io_init(void)
{
    // amplifier configuration pins
    P2DIR |= 0x19;
    // initial states
    TX_AMP_EN = 0;
    RX_AMP_EN = 0;
    AMP_BYPASS_EN = 1;
}

void t1IntHandler(void) __interrupt(T1_VECTOR) // interrupt handler should trigger on T1 overflow
{
    clock++;
}

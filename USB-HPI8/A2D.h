#ifndef _A2D_H_INCLUDED
#define _A2D_H_INCLUDED

#include <inttypes.h>
#include <avr/io.h>
#include <avr/signal.h>
#include <avr/interrupt.h>

#ifdef ADATE // Compatibility for new Mega processors
    #define ADFR  ADATE
#endif

class A2D
{
    volatile bool completeFlag; // the ADC is complete

public:

    enum PRESCALEt
    {
        // ADC clock prescaler select
        //    - selects how much the CPU clock frequency is divided
        //      to create the ADC clock frequency
        //    - lower division ratios make conversion go faster
        //    - higher division ratios make conversions more accurate
        //
        PRESCALE_DIV2       = 0x00,   // 0x01,0x00 -> CPU clk/2
        PRESCALE_DIV4       = 0x02,   // 0x02 -> CPU clk/4
        PRESCALE_DIV8       = 0x03,   // 0x03 -> CPU clk/8
        PRESCALE_DIV16      = 0x04,   // 0x04 -> CPU clk/16
        PRESCALE_DIV32      = 0x05,   // 0x05 -> CPU clk/32
        PRESCALE_DIV64      = 0x06,   // 0x06 -> CPU clk/64
        PRESCALE_DIV128     = 0x07,   // 0x07 -> CPU clk/128

        // Bit mask for the prescaler select
        //
        PRESCALE_MASK       = 0x07
        };

    enum REFERENCEt
    {
        // ADC voltage reference select
        //    - this determines what is used as the
        //      full-scale voltage point for ADC conversions
        //
        REFERENCE_AREF      = 0x00,   // 0x00 -> AREF pin, internal VREF turned off
        REFERENCE_AVCC      = 0x01,   // 0x01 -> AVCC pin, internal VREF turned off
        REFERENCE_RSVD      = 0x02,   // 0x02 -> Reserved
        REFERENCE_256V      = 0x03,   // 0x03 -> Internal 2.56V VREF

        // Bit mask for the reference select
        //
        REFERENCE_MASK      = 0xC0
        };

    enum CHANNELt
    {
        // Channel defines (for reference and use in code)
        // these channels supported by all AVRs with ADC
        //
        CH_ADC0             = 0x00,
        CH_ADC1             = 0x01,
        CH_ADC2             = 0x02,
        CH_ADC3             = 0x03,
        CH_ADC4             = 0x04,
        CH_ADC5             = 0x05,
        CH_ADC6             = 0x06,
        CH_ADC7             = 0x07,
        CH_122V             = 0x1E,   // 1.22V voltage reference
        CH_AGND             = 0x1F,   // AGND

        // These channels supported only in ATmega128
        // differential with gain
        //
        CH_0_0_DIFF10X      = 0x08,
        CH_1_0_DIFF10X      = 0x09,
        CH_0_0_DIFF200X     = 0x0A,
        CH_1_0_DIFF200X     = 0x0B,
        CH_2_2_DIFF10X      = 0x0C,
        CH_3_2_DIFF10X      = 0x0D,
        CH_2_2_DIFF200X     = 0x0E,
        CH_3_2_DIFF200X     = 0x0F,

        // differential
        CH_0_1_DIFF1X       = 0x10,
        CH_1_1_DIFF1X       = 0x11,
        CH_2_1_DIFF1X       = 0x12,
        CH_3_1_DIFF1X       = 0x13,
        CH_4_1_DIFF1X       = 0x14,
        CH_5_1_DIFF1X       = 0x15,
        CH_6_1_DIFF1X       = 0x16,
        CH_7_1_DIFF1X       = 0x17,

        CH_0_2_DIFF1X       = 0x18,
        CH_1_2_DIFF1X       = 0x19,
        CH_2_2_DIFF1X       = 0x1A,
        CH_3_2_DIFF1X       = 0x1B,
        CH_4_2_DIFF1X       = 0x1C,
        CH_5_2_DIFF1X       = 0x1D,

        // Bit mask for ADC channel multiplexer
        MUX_MASK            = 0x1F
        };

    // Initializes the A/D converter (turns ADC on and prepares it for use)
    //
    void Initialize(void);

    // Turn off A/D converter
    //
    void TurnOff( void )
    {
        ADCSR &= ~_BV(ADIE);  // disable ADC interrupts
        ADCSR &= ~_BV(ADEN);  // disable ADC (turn off ADC power)
        }

    // Turn on A/D converter
    //
    void TurnOn( void )
    {
        ADCSR |= _BV(ADIE);   // enable ADC interrupts
        ADCSR |= _BV(ADEN);   // enable ADC (turn on ADC power)
        }

    // Sets the division ratio of the A/D converter clock.
    // This function is automatically called from Initialize ()
    // with a default value.
    //
    void SetPrescaler( PRESCALEt prescale = PRESCALE_DIV64 )
    {
        ADCSR = ( ADCSR & ~PRESCALE_MASK ) | prescale;
        }

    // Configures which voltage reference the A/D converter uses.
    // This function is automatically called from Initialize()
    // with a default value.
    //
    void SetReference( REFERENCEt ref = REFERENCE_256V )
    {
        ADMUX = ( ADMUX & ~REFERENCE_MASK ) | ( ref << 6 );
        }

    // Sets the ADC input channel
    //
    void SetChannel( CHANNELt ch )
    {
        ADMUX = ( ADMUX & ~MUX_MASK ) | ( ch & MUX_MASK );
        }

    // Start a conversion on the current ADC input channel
    //
    void StartConvert( void )
    {
        completeFlag = false; // clear conversion complete flag
        ADCSR |= _BV(ADIF);   // clear hardware "conversion complete" flag 
        ADCSR |= _BV(ADSC);   // start conversion
        }

    void SignalComplete( void )
    {
        completeFlag = true;
        }

    // Wait until conversion is complete
    //
    void WaitUntilComplete( void )
    {
        do; while( bit_is_set( ADCSR, ADSC ) );
        // do; while( ! completeFlag );
        // do; while( bit_is_clear( ADCSR, ADIF ) );
        }

    // Starts a conversion on A/D channel# ch,
    // returns the 10-bit value of the conversion when it is finished.
    //
    int16_t Convert10bit( CHANNELt ch );
    };

extern A2D adc;

#endif

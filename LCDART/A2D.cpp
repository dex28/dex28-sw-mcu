
#include "A2D.h"

A2D adc;

///////////////////////////////////////////////////////////////////////////////

// Interrupt handler for ADC complete interrupt
//
SIGNAL( SIG_ADC )
{
    // set the conversion flag to indicate "complete"
    //
    adc.SignalComplete ();
    }

// Initializes the A/D converter (turns ADC on and prepares it for use)
//
void A2D::Initialize( void )
{
    ADCSR |= _BV(ADEN);               // enable ADC (turn on ADC power)
    ADCSR &= ~_BV(ADFR);              // default to single sample convert mode

    SetPrescaler ();                  // set default prescaler
    SetReference ();                  // set default reference

    ADMUX &= ~_BV(ADLAR);             // set to right-adjusted result

    ADCSR |= _BV(ADIE);               // enable ADC interrupts

    completeFlag = false;             // clear conversion complete flag
    sei();                            // turn on interrupts (if not already on)
    }

// Starts a conversion on A/D channel# ch,
// returns the 10-bit value of the conversion when it is finished.
//
int16_t A2D::Convert10bit( CHANNELt ch )
{
    SetChannel( ch );
    StartConvert ();
    WaitUntilComplete ();

    // CAUTION: Must read ADCL *before* ADCH!!!
    return ADCL | ( ADCH << 8 ); // read ADC (full 10 bits);
    }

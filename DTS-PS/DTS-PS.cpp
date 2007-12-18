
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

#include <compat/ina90.h>

#include <string.h>

#include "A2D.h"
#include "Cadence.h"

/*
    MCU:    ATMega8 

    Fuses:  D9A1
            - Boot Flash size=1024
            - Brown-out Detection Level 2.7V
            - Brown-out Detection Enabled
            - Int RC. Osc 1MHz; Start 6 CK + 64ms

    MCU Pin Usage:
    --------------

    DTS Power Control

      PD2        OUT      DTS 1
      PD3        OUT      DTS 2
      PD4        OUT      DTS 3
      PD5        OUT      DTS 4
      PC6        OUT      DTS 5
      PC7        OUT      DTS 6

    DTS Power Feedback

      ADC6       IN       DTS 1
      ADC7       IN       DTS 2
      PC0/ADC0   IN       DTS 3
      PC1/ADC1   IN       DTS 4
      PC2/ADC2   IN       DTS 5
      PC3/ADC3   IN       DTS 6

*/

class DTS : public CADENCE
{
    int ID;
    A2D::CHANNELt adch;

    enum { PS_OFF, PS_ON, PS_BURST } state;

public:

    void Initialize( int delay, int port, A2D::CHANNELt channel )
    {
        ID = port;
        adch = channel;
        state = PS_OFF;
        SetCadence( 1, 1, delay );
        Increment ();
        }

    void StateMachine( bool do_adc )
    {
        Increment ();

        if ( state == PS_OFF )
        {
            if ( IsEOC () )
            {
                state = PS_ON;
                TurnOn ();
                }
            }
        else if ( state == PS_BURST )
        {
            if ( IsEOC () )
            {
                state = PS_OFF;
                SetCadence( 1, 1, 2000 );
                }
            }
        else if ( state == PS_ON && do_adc )
        {
            int current = adc.Convert10bit( adch );

#if 1 // R = 2.2 Ohm

            // ADC = current * 1024 * 2.2 Ohm / 1.2V
            //
            #define MAX_CURRENT 282 // 150mA

#else // R = 2.7 Ohm

            // ADC = current * 1024 * 2.7 Ohm / 1.2V
            // Experimental: AD value 109 == 47.5mA == 0.125V
            //
            #define MAX_CURRENT 345 // 150mA

#endif 
            if ( current >= MAX_CURRENT )
            {
                state = PS_BURST;
                SetCadence( 2, 1, 1 );
                }
            }
        
        if ( IsON () )
            PORTD |= _BV( ID );
        else
            PORTD &= ~_BV( ID );
        }

    };

DTS dts[ 6 ];

///////////////////////////////////////////////////////////////////////////////

int
main( void )
{
    // Enable PD2..PD7 as outputs (default low).
    //
    PORTD &= ~_BV(PD2) & ~_BV(PD3) & ~_BV(PD4) & ~_BV(PD5) & ~_BV(PD6) & ~_BV(PD7);
    DDRD  |= _BV(DDD2) | _BV(DDD3) | _BV(DDD4) | _BV(DDD5) | _BV(DDD6) & _BV(DDD7);
    _NOP ();

    dts[ 0 ].Initialize(  500, PD2, A2D::CH_ADC6 );
    dts[ 1 ].Initialize(  800, PD3, A2D::CH_ADC7 );
    dts[ 2 ].Initialize( 1100, PD4, A2D::CH_ADC0 );
    dts[ 3 ].Initialize( 1400, PD5, A2D::CH_ADC1 );
    dts[ 4 ].Initialize( 1700, PD6, A2D::CH_ADC2 );
    dts[ 5 ].Initialize( 2000, PD7, A2D::CH_ADC3 );

    adc.Initialize ();
    adc.TurnOn (); // Disable ADC; save power

    wdt_enable( WDTO_250MS ); // 250ms Watchdog

    bool do_adc = true; // will togle every second time

    for (;;)
    {
        wdt_reset (); // We are alive: Reset Watchdog timer

        ///////////////////////////////////////////////////////////////////////

        for ( int i = 0; i < 6; i++ )
            dts[ i ].StateMachine( do_adc );

        do_adc = ! do_adc;
        }

    return 0;
    }

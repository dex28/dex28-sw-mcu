
#include "Keyboard.h"

KEYBOARD kbd;

///////////////////////////////////////////////////////////////////////////////

void KEY:: Input( int x )
{
	last = current;
	current = ( x == 0 );
	
	if ( last != current ) 
		count = DEBOUNCE_DELAY;
		
	if ( count == 0 )
		return;
		
	if ( --count > 0 )
		return;
	
	if ( state != current )
	{	
		state = current;
		
		if ( state )
			pressed = 1;
		else
			depressed = 1;
		}
	}
		
void KEYBOARD:: Initialize( void )
{
    scanRow = 0;
    for ( int i = 0; i < 6; i++ )
        keys[ i ].Initialize ();

    // Configure PB3..PB5 as 3-state inputs
    //
    PORTB &= ~_BV(PB3) & ~_BV(PB4) & ~_BV(PB5);
    DDRB  &= ~_BV(DDB3) & ~_BV(DDB4) & ~_BV(DDB5);
    }

void KEYBOARD:: Scan( void )
{
    switch( scanRow )
    {
        case 0: // PB3 is low

            keys[ 0 ].Input( PINB & _BV(PB4) );
            keys[ 1 ].Input( PINB & _BV(PB5) );

            DDRB &= ~_BV(DDB3); // configure PB3 as 3-state input
            DDRB |= _BV(DDB4); // configure PB4 as output (low)
            break;

        case 1: // PB4 is low

            keys[ 2 ].Input( PINB & _BV(PB3) );
            keys[ 3 ].Input( PINB & _BV(PB5) );

            DDRB &= ~_BV(DDB4); // configure PB4 as 3-state input
            DDRB |= _BV(DDB5); // configure PB5 as output (low)
            break;

        case 2: // PB5 is low

            keys[ 4 ].Input( PINB & _BV(PB3) );
            keys[ 5 ].Input( PINB & _BV(PB4) );

            DDRB &= ~_BV(DDB5); // configure PB5 as 3-state input
            DDRB |= _BV(DDB3); // configure PB3 as output (low)
            break;
        }

    if ( ++scanRow >= 3 )
        scanRow = 0;
    }


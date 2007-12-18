
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <compat/ina90.h>

#define F_CPU 14.7456E6
#include <avr/delay.h>

#include <string.h>
#include <stdarg.h>

#include "xhfc.h"

/*
    MCU:    ATMega16  (Signature 1E 94 03)
    Fuses:  D92F

    MCU Pin Usage:
    --------------

    LED

      PD5   OUT   OC1A         PWM LED YELLOW
      PD4   OUT   OC1B         PWM LED BLUE
      PB3   OUT   OC0          PWM LED RED

    USART

      PD7   OUT                RS232 RTS#
      PD6   IN                 RS232 CTS#
      PD1   OUT                RS232 TXD
      PD0   IN                 RS232 RXD

    Data Bus

      PC[0:7] INOUT            D[0:7]     (common to USB FIFO & HPI)

    USB FIFO

      PA1   OUT                USB RD#
      PA0   OUT                USB WR
      PB4   OUT                USB SI/WU

      PD3   IN    INT1         USB TXE#   (negative level interrupt)
      PD2   IN    INT0         USB RXF#   (negative level interrupt)

    HPI (Host Port Interface)

      PA6   OUT                HPI CS#
      PA5   OUT                HPI RESET#
      PA4   OUT                HPI DS#
      PA3   OUT                HPI R/W#
      PA2   OUT                HPI A0  (HBIL)
      PB0   OUT                HPI A1 (HCNTL0)
      PB1   OUT                HPI A2 (HCNTL1)

      PB2   IN    INT2         HPI INT    (positive level interrupt)
*/

///////////////////////////////////////////////////////////////////////////////

void usb_Put( int ch )
{
    do ; while( PIND & _BV(PD3) );            // Wait TXE# to be asserted

    DDRC = 0xFF;                              // PC[0:7] as output
    PORTC = ch & 0xFF;                        // Set data on PC[0:7]

    PORTA |= _BV(PA0);                        // WR = 1

    _NOP ();                                  // Wait 60 ns

    PORTA &= ~_BV(PA0);                       // WR = 0

    DDRC = 0x00;                              // PC[0:7] as input
    PORTC = 0x00;                             // Tri-state PC[0:7]
    }

bool usb_RxAvailable( void )
{
    return ! ( PIND & _BV(PD2) );             // Return true if RXF# is asserted
    }

int usb_Get( void )
{
    PORTA &= ~_BV(PA1);                       // RD# = 0

    _NOP ();                                  // Wait 60 ns
    _NOP ();                                  // Wait 60 ns

    unsigned char data = PINC;                // Read octet from PC[0:7]

    PORTA |= _BV(PA1);                        // RD# = 1

    return data;
    }

///////////////////////////////////////////////////////////////////////////////

void tracef( const char* format... )
{
    static const char hextab [] = "0123456789ABCDEF";

    va_list marker;
    va_start( marker, format ); // Initialize variable arguments.


	while( *format )
	{
		if ( *format != '%' )
		{
			usb_Put( *format++ );
			continue;
			}

		format++; // Advance to next character

		if ( *format == '%' )
		{
			usb_Put( '%' );
			}
		else if ( *format == 'c' )
		{
			int c = va_arg( marker, int );
			usb_Put( hextab[ ( c >>  4 ) & 0xF ] );
			usb_Put( hextab[ ( c       ) & 0xF ] );
			}
		else if ( *format == 'a' )
		{
			unsigned char* c = va_arg( marker, unsigned char* );
			int c_count = va_arg( marker, int );
			for( ; c_count > 0; c_count--, c++ )
			{
				usb_Put( hextab[ ( *c >>  4 ) & 0xF ] );
				usb_Put( hextab[ ( *c       ) & 0xF ] );
				if ( c_count > 1 )
					usb_Put( ' ' );
				}
			}
		else if ( *format == 's' )
		{
			int x = va_arg( marker, int );
			usb_Put( hextab[ ( x >> 12 ) & 0xF ] );
			usb_Put( hextab[ ( x >>  8 ) & 0xF ] );
			usb_Put( hextab[ ( x >>  4 ) & 0xF ] );
			usb_Put( hextab[ ( x       ) & 0xF ] );
			}
		else if ( *format == 'l' )
		{
			long x = va_arg( marker, long );
			usb_Put( hextab[ ( x >> 28 ) & 0xF ] );
			usb_Put( hextab[ ( x >> 24 ) & 0xF ] );
			usb_Put( hextab[ ( x >> 20 ) & 0xF ] );
			usb_Put( hextab[ ( x >> 16 ) & 0xF ] );
			usb_Put( hextab[ ( x >> 12 ) & 0xF ] );
			usb_Put( hextab[ ( x >>  8 ) & 0xF ] );
			usb_Put( hextab[ ( x >>  4 ) & 0xF ] );
			usb_Put( hextab[ ( x       ) & 0xF ] );
			}
		else if ( *format == 's' )
		{
			const char* str = va_arg( marker, const char* );
			while( *str )
				usb_Put( *str++ );
			}

		format++; // Advance to next character
		}

	usb_Put( '\r' );
	usb_Put( '\n' );

   	va_end( marker ); // Reset variable arguments.
	}

///////////////////////////////////////////////////////////////////////////////

XHFC hfc;
unsigned int sysTimer = 0;

///////////////////////////////////////////////////////////////////////////////

void Fill_TX_Buffer( int ch )
{
    static int nibble_count = 0;
    static int tx_octet = 0;
    static int port_id = 0;

    if ( ch == '\r' )
    {
        nibble_count = 0;
        tx_octet = 0;
        hfc.port[ port_id ].D_TX_Send ();
        }
    else if ( ch == 27 ) // escape
    {
        nibble_count = 0;
        tx_octet = 0;
        hfc.port[ port_id ].D_TX_Abort ();
        }
    else if ( ch == '?' ) // query
    {
        hfc.port[ port_id ].D_TX_Query ();
        }
    else if ( ch == 'x' ) // activate
    {
        nibble_count = 0;
        tx_octet = 0;
        hfc.port[ port_id ].PH_ActivateRequest ();
        }
    else if ( ch == 'y' ) // deactivate
    {
        nibble_count = 0;
        tx_octet = 0;
        hfc.port[ port_id ].PH_DeactivateRequest ();
        }
    else if ( ch >= '0' && ch <= '9' )
    {
        tx_octet <<= 4; ++nibble_count;
        tx_octet += ch - '0';
        if ( nibble_count == 1 )
        {
            port_id = tx_octet & 0x01;
            tx_octet = 0;
            }
        else if ( nibble_count & 1 )
        {
            hfc.port[ port_id ].D_TX_Append( tx_octet );
            tx_octet = 0;
            }
        }
    else if ( ch >= 'a' && ch <= 'f' )
    {
        tx_octet <<= 4; ++nibble_count;
        tx_octet += ch - 'a' + 10;
        if ( nibble_count == 1 )
        {
            port_id = tx_octet & 0x01;
            tx_octet = 0;
            }
        else if ( nibble_count & 1 )
        {
            hfc.port[ port_id ].D_TX_Append( tx_octet );
            tx_octet = 0;
            }
        }
    else if ( ch >= 'A' && ch <= 'F' )
    {
        tx_octet <<= 4; ++nibble_count;
        tx_octet += ch - 'A' + 10;
        if ( nibble_count == 1 )
        {
            port_id = tx_octet & 0x01;
            tx_octet = 0;
            }
        else if ( nibble_count & 1 )
        {
            hfc.port[ port_id ].D_TX_Append( tx_octet );
            tx_octet = 0;
            }
        }
    }

int
main( void )
{
    PORTA = _BV(PA7) | _BV(PA6) | _BV(PA4) | _BV(PA3) | _BV(PA2) | _BV(PA1);
    DDRA = 0xFF; // all outputs; PA0 (USB WR) and PA5 (HPI RESET#) are low
    _NOP ();

    PORTB &= ~_BV(PB1) & ~_BV(PB0);
    PORTB |= _BV(PB3); // Trun on red LED
    DDRB  |= _BV(DDB3) | _BV(DDB1) | _BV(DDB0);
    _NOP ();

    PORTB &= ~_BV(PB2); // HINT 3-state input
    DDRB &= ~_BV(DDB2);

    PORTC = 0x00; // tri-state
    DDRC = 0x00; // PC0..7 input
    _NOP ();

    PORTD &= ~_BV(PD4) & _BV(PD5);
    DDRD  |= _BV(DDD4) | _BV(DDD5);
    _NOP ();

    // Wait 1s because not to source much current from USB on plug in
    //
    for ( int i = 0; i < 100; i++ )
        _delay_ms( 10 );

    // Hello world
    //
    tracef( "\r\n\n\nXHFC Ready." );

    // Get XHFC out of reset
    //
    PORTA |= _BV(PA5); // RESET# = 1

    hfc.Initialize( 0x01 );
    hfc.port[ 0 ].Startup ();
    hfc.port[ 1 ].Startup ();

    PORTB &= ~_BV(PB3); // Trun off red LED

    for ( ;; )
    {
        if ( usb_RxAvailable () )
            Fill_TX_Buffer( usb_Get () );

        if ( ! hfc.IsIrqAsserted () ) // Poll interrupt pin
            continue;

        if ( ! hfc.InterruptHandler () )
            continue;

        if ( ! hfc.BottomHalf_EH () )
            continue;

        // We are here every 1ms
        //
        if ( ++ sysTimer < 1000 )
            continue;

        sysTimer = 0;

        // We are here every 1s
        //
        PORTD ^= _BV(PD4);

        // tracef( "%l %l", hfc.GetIrqCount (), hfc.GetF0Count () );
        }

    return 0;
    }

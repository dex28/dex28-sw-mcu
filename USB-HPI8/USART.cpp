
#include "USART.h"

USART usart;

///////////////////////////////////////////////////////////////////////////////

SIGNAL( SIG_UART_RECV ) // USART, Rx Complete
{
    usart.RXC_Handler ();
    }

/*
static int USART_putchar( char data ) 
{ 
    return usart.PutCh( data );
    }
*/

void USART::Initialize( void )
{
    recvbufp1 = 0;
    recvbufp2 = 0;

    // Set baud rate
    //
    UBRRH = 0; UBRRL = 7; // 115200 bps
    // UBRRH = 0; UBRRL = 47; // 9600 bps
    //
    // Baud Rate(bps) vs UBBR @ Fosc = 7.3728 MHz
    // --------------------------------
    //            U2X = 0     U2X = 1
    //         UBRR/Error  UBRR/Error
    // --------------------------------
    //   2400   191 0.0%    383 0.0%
    //   4800    95 0.0%    191 0.0%
    //   9600    47 0.0%     95 0.0%
    //  14.4k    31 0.0%     63 0.0%
    //  19.2k    23 0.0%     47 0.0%
    //  28.8k    15 0.0%     31 0.0%
    //  38.4k    11 0.0%     23 0.0%
    //  57.6k     7 0.0%     15 0.0%
    //  76.8k     5 0.0%     11 0.0%
    // 115.2k     3 0.0%      7 0.0%

    // Set frame format: async, 8 data, 1 stop bit, no parity
    //
    UCSRC = _BV(URSEL) | (3<<UCSZ0);

    // Enable Receiver, Transmitter and Receiver Interrupts
    //
    UCSRB = _BV(RXEN) | _BV(TXEN) | _BV(RXCIE);

    // Hook our putchar
    // fdevopen( USART_putchar, NULL, 0 );

    Flush (); // flush input

    sei (); // enable interrupts (if not already enabled)
    }

USART& USART:: operator << ( char data )
{
    PutCh( data );
    return *this;
    }

USART& USART:: operator << ( const char* str )
{
    while( *str )
        PutCh( *str++ );
    return *this;
    }

USART& USART:: operator << ( int16_t v )
{
    if ( v < 0 ) PutCh( '-' ), v = -v;

    char digits[ 6 ], *p = digits;

    if ( v == 0 )
        *p++ = '0';
    else while ( v != 0 )
    {
        *p++ = '0' + v % 10;
        v /= 10;
        }

    while ( --p >= digits )
        PutCh( *p );

    return *this;
    }

USART& USART:: operator << ( uint16_t v )
{
    static const char* hex = "0123456789ABCDEF";
    PutCh( hex[ ( v >> 12 ) & 0xF ] );
    PutCh( hex[ ( v >> 8 ) & 0xF ] );
    PutCh( hex[ ( v >> 4 ) & 0xF ] );
    PutCh( hex[ v & 0xF ] );
    return *this;
    }

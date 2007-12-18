#ifndef _USART_H_INCLUDED
#define _USART_H_INCLUDED

#include <inttypes.h>
#include <avr/io.h>
#include <avr/signal.h>
#include <avr/interrupt.h>

class USART
{
    //  PD1   OUT                RS232 TXD
    //  PD0   IN                 RS232 RXD
    
    enum { RECVBUFSIZE = 32 };

    int recvbuf[ RECVBUFSIZE ];
    volatile int recvbufp1;
    volatile int recvbufp2;

public:

    void RXC_Handler( void )
    {
        unsigned char status __attribute__((unused)) = UCSRA;

        int nextp = recvbufp1;
        if ( ++nextp >= RECVBUFSIZE )
            nextp = 0;

        if ( nextp == recvbufp1 ) // buffer overrun
        {
            unsigned char dummy;
            dummy = UDR;
            return;
            }

        recvbuf[ recvbufp1 ] = UDR;
        recvbufp1 = nextp;
        }

    void RX_Input( int ch )
    {
        cli (); // disable interrupts

        int nextp = recvbufp1;
        if ( ++nextp >= RECVBUFSIZE )
            nextp = 0;

        if ( nextp != recvbufp1 ) // no buffer overrun
        {
            recvbuf[ recvbufp1 ] = ch;
            recvbufp1 = nextp;
            }

        sei (); // enable interrupts
        }

    bool IsEmpty( void ) const
    {
        return recvbufp1 == recvbufp2;
        }

    void Initialize( void );

    int PutCh( int data )
    {
        // Wait for empty transmit buffer
        //
        do; while ( !( UCSRA & _BV(UDRE) ) );

        // Put data into buffer, sends the data
        //
        UDR = data;
        return 0;
        }

    void Flush( void )
    {
        unsigned char dummy;
        while ( UCSRA & _BV(RXC) )
            dummy = UDR;
        }

    int GetCh( void )
    {
        int ch = recvbuf[ recvbufp2 ];
        if ( ++recvbufp2 == RECVBUFSIZE )
            recvbufp2 = 0;
        return ch;
        }

    USART& operator << ( char data );

    USART& operator << ( const char* str );

    USART& operator << ( int16_t v );

    USART& operator << ( uint16_t v );
    };

extern USART usart;

#endif // _USART_H_INCLUDED

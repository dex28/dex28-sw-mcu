#ifndef _FT245_H_INCLUDED
#define _FT245_H_INCLUDED

class USB_FIFO
{
    static int stdio_put( char ch )
    {
        extern USB_FIFO usb;
        usb.PutOctet( ch );
        return 0;
        }

    static int stdio_get( void )
    {
        return -1;
        }

    public:

    void Initialize( void )
    {
        fdevopen( stdio_put, stdio_get, 0 );
        }

    bool RXF( void ) const
    {
        return ( PIND & _BV(PIND0) ) == 0; // return ~RXF == 0
        }

    bool TXE( void ) const
    {
        return ( PIND & _BV(PIND1) ) == 0; // return ~TXE == 0
        }

    int GetOctet( void )
    {
        // Note: RXF() must be asserted before calling GetOctet().
        // FT245 has internal receive buffer 256 octets long.

        PORTC = 0xFF;       // Config pull-ups on PC0..7
        DDRC  = 0x00;       // Config PC0..7 as input

        PORTG &= ~_BV(PG1); // ~RD = LOW
        
        _NOP (); _NOP ();
        int ch = PINC;

        PORTG |= _BV(PG1);  // ~RD = HIGH

        return ch;
        }

    void PutOctet( int ch )
    {
        // Octet will be lost if usb fifo is not ready.
        // FT245 has internal transmit buffer 128 octets long.
        //
        if ( ! TXE () ) 
            return;

        DDRC  = 0xFF;       // Config PC0..7 as output

        // do ; while( PIND & _BV(PIND1) ); // Wait TXE

        PORTC = ch;         // Set data on PC0..7
        PORTG &= ~_BV(PG0); // ~WR = LOW
        PORTG |= _BV(PG0);  // ~WR = HIGH
        }
    };

#endif // _FT245_H_INCLUDED

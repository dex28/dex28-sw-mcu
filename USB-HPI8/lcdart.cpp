
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/wdt.h>
#include <avr/sleep.h>

#include <string.h>

#include "LCD.h"
#include "USART.h"
#include "ATX.h"
#include "A2D.h"
#include "Keyboard.h"
#include "Cadence.h"

/*
    MCU:    ATMega16
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

    Interrupts

      PD3   IN    INT1         USB TXE#, level neg.
      PD2   IN    INT0         USB RXF#, level neg.
      PB2   IN    INT2         HPI INT, level pos.

    Databus

      PC0..     I/O

    USB & HPI

      PA7   OUT                (LCD CS; not used)

      PA6   OUT                HPI HCS#
      PA5   OUT                HPI RESET#
      PA4   OUT                HPI HDS#
      PA3   OUT                HPI HR/W#
      PA2   OUT                HPI HBIL

      PA1   OUT                USB RD#
      PA0   OUT                USB WR

      PB4   OUT                USB SI/WU
      PB1   OUT                HPI HCNTL1
      PB0   OUT                HPI HCNTL0
*/

///////////////////////////////////////////////////////////////////////////////

class HPI
{
public:

    void Set_HRESET( void )
    {
        // HRESET# = LOW
        PORTA &= ~_BV(PA5);
        }

    void Pulse_HRESET ()
    {
        // HRESET# = LOW
        PORTA &= ~_BV(PA5);

        // Wait 1us
        _NOP (); _NOP (); _NOP (); _NOP ();
        _NOP (); _NOP (); _NOP (); _NOP ();
        _NOP (); _NOP (); _NOP (); _NOP ();
        _NOP (); _NOP (); _NOP (); _NOP ();

        // HRESET# = HIGH
        PORTA |= _BV(PA5);
        }

    unsigned short Read( int cntl )
    {
        DDRC = 0x00; // DDRC input
        PORTC = 0x00; // tri-state

        // HCNTL0..1 = cntl & 0x03
        PORTB &= ~_BV(PB1) & ~_BV(PB0);
        PORTB |= ( cntl & 0x03 );

        // HDS# = HIGH, HR/W# = HIGH
        PORTA |= _BV(PA4) | _BV(PA3);

        // HCS# = LOW
        PORTA &= ~_BV(PA6);

        // HBIL = LOW
        PORTA &= ~_BV(PA2);

        // HDS# = LOW
        PORTA &= ~_BV(PA4);
        _NOP ();

        // Read byte
        unsigned short H = PINC;

        // HDS# = HIGH
        PORTA |= _BV(PA4);

        // HBIL = HIGH
        PORTA |= _BV(PA2);

        // HDS# = LOW
        PORTA &= ~_BV(PA4);
        _NOP ();

        // Read byte
        unsigned short L = PINC;

        // HDS# = HIGH
        PORTA |= _BV(PA4);

        // HCS# = HIGH
        PORTA |= _BV(PA6);

        return ( H << 8 ) | L;
        }

    void Write( int cntl, unsigned short value )
    {
        // HCNTL0..1 = cntl & 0x03
        PORTB &= ~_BV(PB1) & ~_BV(PB0);
        PORTB |= ( cntl & 0x03 );

        // HDS# = HIGH, HR/W# = LOW
        PORTA |= _BV(PA4);
        PORTA &= ~_BV(PA3);

        // HCS# = LOW
        PORTA &= ~_BV(PA6);

        // MSB
        DDRC = 0xFF; // DDRC output
        PORTC = ( value >> 8 ) & 0xFF;

        // HBIL = 0
        PORTA &= ~_BV(PA2);

        // HDS# = LOW
        PORTA &= ~_BV(PA4);

        // HDS# = HIGH
        PORTA |= _BV(PA4);

        // LSB
        PORTC = value & 0xFF;

        // HBIL = 1
        PORTA |= _BV(PA2);

        // HDS# = LOW
        PORTA &= ~_BV(PA4);

        // HDS# = HIGH
        PORTA |= _BV(PA4);

        // HCS# = HIGH
        PORTA |= _BV(PA6);

        DDRC = 0x00; // DDRC input
        PORTC = 0x00; // tri-state
        }

    void Put_HPIC( int value )
    {
        value &= 0x7F;
        value = ( value << 8 ) | value;
        Write( 0, value );
        }

    void Reset_DSP( void )
    {
        Pulse_HRESET ();

        // Set XHPIA = 0x0000:0x1000
        //
        Put_HPIC( 0x10 );   // XHPIA = ON
        Write( 2, 0x0000 ); // HPIA = 0x0000
        Put_HPIC( 0x00 );   // XHPIA = OFF
        Write( 2, 0x1000 ); // HPIA = 0x1000

        // Wait HINT
        //
        for ( int i = 0; i < 10000; i++ )
            if ( ( PINB & _BV(PB2) ) == 1 )
                break;

        // Acknowledge HINT
        //
        Put_HPIC( 0x08 );
        }

    unsigned short Initialize( void )
    {
        Reset_DSP ();

        // Download program @ 0x1000
        //
        Write( 3, 0x10F8 ); // LD *(0x003E), A
        Write( 1, 0x003E );

        Write( 1, 0x80F8 ); // STL A, *(0x1100)
        Write( 1, 0x1100 );

        Write( 1, 0xF073 ); // L1: B L1
        Write( 1, 0x1004 );

        Write( 2, 0x1100 ); // *(0x1100) = 0xFFFF
        Write( 3, 0xFFFF );

        Write( 2, 0x007E ); // Run program @0x1000
        Write( 3, 0x0000 );
        Write( 1, 0x1000 ); 

        Write( 2, 0x1100 ); // Get ID from *(0x1100)
        unsigned short ID = Read( 3 );

        Reset_DSP ();
        return ID;
        }
    };

///////////////////////////////////////////////////////////////////////////////

volatile uint16_t Timer0_Counter = 0;
volatile bool Timer0_Event = false;

SIGNAL( SIG_OVERFLOW0 ) // Timer/Counter0 Overflow
{
    Timer0_Counter++;
    Timer0_Event = true;
    }

void Timer0_Initialize( void )
{
    TCNT0 = 0;

    // clk/64 from prescaller -> 450Hz (2.222ms)
    //
    TCCR0 = _BV(CS01) | _BV(CS00);
    TIMSK |= _BV(TOIE0);
    }

void Timer0_Wait( int count )
{
    for ( uint16_t c = Timer0_Counter; count > 0; count--, c = Timer0_Counter )
        do; while( c == Timer0_Counter );
    }

SIGNAL( SIG_OVERFLOW2 ) // Timer/Counter2 Overflow
{
    }

void Timer2_Initialize( void )
{
    return;

    TCNT2 = 0;

    // Normal mode (256-modulo counter), OC2 disconnected
    // clk/8 from prescaller -> 3600Hz (277.8us)
    //
    TCCR2 = _BV(CS21);
    TIMSK |= _BV(TOIE2);
    }

///////////////////////////////////////////////////////////////////////////////

bool isSoftReset = false;


inline void usb_Put( int ch )
{
    PORTC = ch;
    DDRC = 0xFF; // PC0..7 output
    _NOP ();

    // WR = HIGH
    PORTA |= _BV(PA0);

    // WR = LOW
    PORTA &= ~_BV(PA0);

    PORTC = 0x00; // tri-state
    DDRC = 0x00; // PC0..7 input
    _NOP ();
    }

void usb_Put( char* str )
{
    while ( *str )
        usb_Put( *str++ );
    }

HPI hpi;

int
main( void )
{
    char wdg_track[ 8 ];

    PORTA = _BV(PA7) | _BV(PA6) | _BV(PA4) | _BV(PA3) | _BV(PA2) | _BV(PA1);
    DDRA = 0xFF; // all outputs
    _NOP ();

    PORTB &= ~_BV(PB3) & ~_BV(PB1) & ~_BV(PB0);
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

    usart.Initialize ();
    usart << "---------------\r\n";
    usb_Put( "---------------\r\n" );

    usart << "DSP TMS320VC54: " << hpi.Initialize () << "\r\n";

    hpi.Write( 2, 0x1000 );
    usart << "[" << hpi.Read( 1 ) << ",";
    usart << hpi.Read( 1 ) << "]\r\n";

    for ( ;; )
    {
        usart << "HPIC = " << hpi.Read( 0 ) << "\r\n";

        for ( long i = 0; i < 500000; i++ )
            _NOP ();

        PORTD ^= _BV(PD4);

        usb_Put( "Hello, world\r\n" );
        }


    return 0;
    atx.Initialize ();
    lcd.Initialize ();
    adc.Initialize ();
    kbd.Initialize ();

    Timer0_Initialize ();
    Timer2_Initialize ();

    lcd.Setup_4bit ();
    lcd.Set_PWM_LCD( 40 );
    lcd.Set_PWM_BL( 96 );

    if ( memcmp( wdg_track, "Initlzed", 8 ) != 0 )
        memcpy( wdg_track, "Initlzed", 8 );
    else
    {
        isSoftReset = true;
        usart << '!';
        }

    atx.StateMachine (); // Initial settings

    set_sleep_mode( SLEEP_MODE_IDLE );

    wdt_enable( WDTO_250MS ); // 250ms Watchdog

    for (;;)
    {
        adc.TurnOff (); // Disable ADC; save power
        sleep_mode (); // Go CPU IDLE / Sleep

        wdt_reset (); // We are alive: Reset Watchdog timer

        ///////////////////////////////////////////////////////////////////////

        if ( Timer0_Event )
        {
            Timer0_Event = false;

            kbd.Scan ();

            atx.StateMachine ();

		    for ( int i = 0; i < 5; i++ )
            {
                if ( kbd.IsPressed( i ) && atx.state == ATXCTRL::SYSTEM_READY )
                {
                    usart << char( 'A' + i );
                    }
                else if ( kbd.IsDepressed( i ) )
                {
                    }
                }

            atx.UpdateIndicatorLED ();

            if ( atx.state == ATXCTRL::SYSTEM_FAILED && atx.state_timer >= 0 )
            {
                switch( lcd.blinkLCD.GetStateSeq() )
                {
                    case 0: // BLINK ON
                        lcd.SetBrightness( 0 ); // 100%
                        atx.DisplayState ();
                        break;
                    case 2: // BLINK OFF
                        lcd.SetBrightness( 3 ); // 25%
                        atx.EraseState ();  
                        break;
                    }
                }

            lcd.blinkLCD.Increment ();
            }

        ///////////////////////////////////////////////////////////////////////

        if ( ! usart.IsEmpty () )
        {
            int ch = usart.GetCh ();

            if ( atx.OnUSARTReceive( ch ) ) // true if atx state is SYSTEM READY
            {
                // Parse ESCAPE sequences
                //
                if ( ch == 'H'-'@' || ch == 'J'-'@' 
                    || ch == 'K'-'@' || ch == 'L'-'@'
                    || ch == '`' )
                {
                    adc.TurnOn ();

                    switch( ch )
                    {
                        case 'H'-'@': lcd.Set_PWM_LCD( lcd.Get_PWM_LCD () + 2 ); break;
                        case 'J'-'@': lcd.Set_PWM_LCD( lcd.Get_PWM_LCD () - 2 ); break;
                        case 'K'-'@': lcd.Set_PWM_BL(  lcd.Get_PWM_BL ()  - 4 ); break;
                        case 'L'-'@': lcd.Set_PWM_BL(  lcd.Get_PWM_BL ()  + 4 ); break;
                        }
/*
                    usart << "TEMP = "
                        << adc.Convert10bit( A2D::CH_ADC6 ) * 5/2
                        << ", VLCD = "
                        << lcd.Get_PWM_LCD ()
                        << " / "
                        << adc.Convert10bit( A2D::CH_ADC7 ) * 5/2
                        << ", IBL = "
                        << lcd.Get_PWM_BL ()
                        << "\r\n";
*/
                    }
                else
                {
                    lcd.SendData( ch );
                    }
                }
            }
        }

    return 0;
    }

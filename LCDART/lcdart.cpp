
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

#include <string.h>

#include "LCD.h"
#include "USART.h"
#include "ATX.h"
#include "A2D.h"
#include "Keyboard.h"
#include "Cadence.h"

/*
    MCU:    ATMega8 
    Fuses:  C93F

    MCU Pin Usage:
    --------------

    LCD module

      PD7   OUT                LCD D7
      PD6   OUT                LCD D6
      PD5   OUT                LCD D5
      PD4   OUT                LCD D4
      PC3   OUT                LCD E
      PC2   OUT                LCD RS
      PB2   OUT                OC1B: PWM for LCD backlight
      PB1   OUT                OC1A: PWM for LCD op. voltage

    USART

      PD1   OUT                RS232 TXD
      PD0   IN                 RS232 RXD

    ATX Control/Monitor

      PC1   OUT                LED INDICATOR
      PC0   OUT                ATX-PS-ON
      PB0   IN   int. pullup   ATX-PWR-OK

    Keyboard

      PB5   I/O  ext. pullup   keyboard can pulldown 1 & 2; can sense 3 & 5
      PB4   I/O  ext. pullup   keyboard -"- 3 & 4; can sense 1 & 6
      PB3   I/O  ext. pullup   keyboard -"- 5 & 6; can sense 2 & 4

    Misc

      PD3   <not used>          
      PD2   <not used>         TODO: ext. pullup   INT0 : Infrared module input
      PC5   <not used>
      PC4   <not used>
*/

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

int
main( void )
{
    char wdg_track[ 8 ];

    // Get Configuration settings from EEPROM
    //
    // Is_AXT_PS: eeprom_read_byte( (const uint8_t*) 0x0000 );
    // LCD.PWM LCD: eeprom_read_byte( (const uint8_t*) 0x0000 );
    // LCD.PWM BL: eeprom_read_byte( (const uint8_t*) 0x0000 );
    // LCD.Type VFD vs. LCD: eeprom_read_byte( (const uint8_t*) 0x0000 );

    usart.Initialize ();
    atx.Initialize ();
    lcd.Initialize ();
    adc.Initialize ();
    kbd.Initialize ();

    Timer0_Initialize ();
    Timer2_Initialize ();

    lcd.Setup_4bit ();
    lcd.Set_PWM_LCD( 40 );
    lcd.Set_PWM_BL( 40 );

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

            atx.OnTimer ();

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

            if ( ! atx.OnUSARTReceive( ch ) ) // if ATX subsystem does not handle ch
            {
                // First, parse ESCAPE sequences
                //
                if ( ch == 27 )
                {
                    lcd.SetPosition( 0, 0 );
                    }
                else if ( ch == 'L'-'@' ) // Form Feed: Ctrl-L
                {
                    usart.PutCh( 023 ); // DC3; Ctrl-S == Suspend
                    lcd.Clear (); // takes ~2ms
                    usart.PutCh( 021 ); // DC1; Ctrl-Q == Start
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
/*
                    adc.TurnOn ();

                    switch( ch )
                    {
                        case 'H'-'@': lcd.Set_PWM_LCD( lcd.Get_PWM_LCD () + 2 ); break;
                        case 'J'-'@': lcd.Set_PWM_LCD( lcd.Get_PWM_LCD () - 2 ); break;
                        case 'K'-'@': lcd.Set_PWM_BL(  lcd.Get_PWM_BL ()  - 4 ); break;
                        case 'L'-'@': lcd.Set_PWM_BL(  lcd.Get_PWM_BL ()  + 4 ); break;
                        }
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

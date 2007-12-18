
#include "ATX.h"

ATXCTRL atx;

///////////////////////////////////////////////////////////////////////////////

const char* ATXCTRL:: STATE_DESC [] =
{
    "???",
    "System Power Off",
    "Waiting Power On",
    "Power On Failed",
    "System Power OK",
    "Booting",
    "System Ready",
    "System Shutdown",
    "System Reboot",
    "Waiting Power Off",
    "Power Off Failed",
    "???"
    };

void ATXCTRL:: Initialize( void )
{
    // Enable PC0..PC1 as outputs (default low).
    //
    PORTC &= ~_BV(PC1) & ~_BV(PC0);
    DDRC  |= _BV(DDC3) | _BV(DDC2) | _BV(DDC1) | _BV(DDC0);
    _NOP ();

    // Enable PB0 as input (with internal pullup)
    //
    PORTB |= _BV(PB0);
    DDRB  &= ~_BV(DDB0);
    _NOP ();

    // Initialize attributes
    //
    Is_ATX_PS = true;

    state = INITIAL_STATE;
    state_timer = -1;
    sysfail_desc = 0;

    poffsw_timer = -1;

    state_bootseq = 0;
    boot_dots = 0;

    recv_lineptr = 0;
    recv_line[ 0 ] = 0;

    indicatorLED.TurnOff ();
    }

bool ATXCTRL:: IsReceived( int min_len, const char* y )
{
    if ( recv_lineptr < min_len )
        return false;

    for ( const char* x = recv_line; *x && *y && *x == *y; x++, y++ )
        {}

    return ! *y;
    }

void ATXCTRL:: EraseState( void )
{
    lcd.SetPosition( 1 );
    lcd.SendData( "                    " );
    }

void ATXCTRL:: DisplayState( void )
{
    lcd.SetPosition( 1 );
    lcd.SendData( "  " );
    if ( state == SYSTEM_FAILED && sysfail_desc )
        lcd.SendData( sysfail_desc );
    else
        lcd.SendData( STATE_DESC[ state ] );
    }

void ATXCTRL:: UpdateIndicatorLED( void )
{
    if ( indicatorLED.IsON () )
        Set_LED_IND_ON ();
    else
        Set_LED_IND_OFF ();

    indicatorLED.Increment ();
    }

void ATXCTRL:: SystemPowerOff( const char* sysFailed )
{
    Set_ATX_PS_OFF ();

    if ( sysFailed )
    {
        sysfail_desc = sysFailed;
        state = SYSTEM_FAILED;
        state_timer = 0;
        }
    else if ( Is_ATX_PS )
    {
        state = WAIT_POWER_OFF;
        state_timer = 0;
        }
    else
    {
        state = SYSTEM_POWER_OFF;
        state_timer = -1;
        }
    }

void ATXCTRL::StateMachine( void )
{
    STATE old_state = state;

    if ( state_timer >= 0 )
        ++state_timer;

    if ( kbd.IsPressed( 5 ) )
        poffsw_timer = 0;
    else if ( kbd.IsDepressed( 5 ) )
        poffsw_timer = -1;
    else if ( poffsw_timer >= 0 )
        ++poffsw_timer;

    if ( state != SYSTEM_READY && IsReceived( 8, "alba7777" ) )
    {
        state = SYSTEM_READY;
        state_timer = -1;
        usart << "READY!\r\n";
        }
    switch( state )
    {
        case INITIAL_STATE:
            if ( Get_PWR_OK () )
            {
                Is_ATX_PS = false;
                state = SYSTEM_POWER_OK;
                state_timer = 0;
                }
            else
            {
                state = SYSTEM_POWER_OFF;
                state_timer = -1;
                }
            break;

        case SYSTEM_POWER_OFF:
            if ( Is_ATX_PS )
            {
                if ( Get_PWR_OK () )
                {
                    SystemPowerOff( "ATX Power Failure" );
                    }
                else if ( poffsw_timer == 0 ) // Pressed 'F'
                {
                    Set_ATX_PS_ON ();
                    state = WAIT_POWER_ON;
                    state_timer = 0;
                    }
                }
            break;

        case WAIT_POWER_ON:
            if ( Get_PWR_OK () )
            {
                state = SYSTEM_POWER_OK;
                state_timer = 0;
                }
            else if ( state_timer >= 900 ) // 2s
            {
                state = POWER_ON_FAILED;
                state_timer = -1;
                }
            break;

        case POWER_ON_FAILED:
            if ( Get_PWR_OK () )
            {
                state = SYSTEM_POWER_OK;
                state_timer = 0;
                }
            break;

        case SYSTEM_POWER_OK:
            if ( ! Get_PWR_OK () )
            {
                SystemPowerOff( "ATX Power Failure" );
                }
            else if ( Is_ATX_PS && poffsw_timer >= 1350 ) // Pressed 'F', T >= 3s
            {
                SystemPowerOff ();
                }
            else if ( state_timer >= 900 ) // 2s
            {
                // Flush input buffer
                recv_lineptr = 0;
                recv_line[ 0 ] = 0;

                state = SYSTEM_BOOT;
                state_timer = 0;
                state_bootseq = 0;
                boot_dots = 0;
                }
            break;

        case SYSTEM_BOOT:
            if ( ! Get_PWR_OK () )
            {
                SystemPowerOff( "ATX Power Failure" );
                }
            else if ( Is_ATX_PS && poffsw_timer >= 1350 ) // Pressed 'F', T >= 3s
            {
                SystemPowerOff ();
                }
            else 
            {
                switch( state_bootseq )
                {
                    case 0: // Expect "<LF>Linux version"
                        if ( state_timer >= 4500 ) // 10s
                        {
                            SystemPowerOff( "Boot Failed : #00" );
                            }
                        else if ( IsReceived( 21, "Linux version" ) )
                        {
                            state_bootseq = 1;
                            state_timer = 0;
                            lcd.SetPosition( 0 );
                            lcd.SendData( "\xFF IPTC ELU28GW-12P \xFF" );
                            }
                        break;

                    case 1: // Expect "<LF>alba"
                        if ( state_timer >= 9000 ) // 20s
                        {
                            SystemPowerOff( "Boot Failed : #01" );
                            }
                        else if ( IsReceived( 4, "alba" ) )
                        {
                            // Ready for user to access display.
                            state = SYSTEM_READY;
                            state_timer = -1;
                            }
                        break;

                    default: // Bug!
                        SystemPowerOff( "Boot Failed : #FF" );
                    }

                if ( ( state_timer & 0x3F ) == 0 )
                {
                    if ( ++boot_dots >= 10 )
                    {
                        boot_dots = 0;
                        lcd.SetPosition( 1, 9 );
                        lcd.SendData( "          " );
                        }
                    else
                    {
                        lcd.SetPosition( 1, 9 + boot_dots );
                        lcd.SendData( '\xFF' );
                        }
                    }
                }
            break;

        case SYSTEM_READY:
            if ( ! Get_PWR_OK () )
            {
                SystemPowerOff( "ATX Power Failure" );
                }
            else if ( poffsw_timer == 0 ) // Pressed 'F'
            {
                usart << 'F'; // Pass 'F' to user: Shutdown, please ...
                }
            else if ( Is_ATX_PS && poffsw_timer >= 1350 ) // Pressed 'F', T >= 3s
            {
                SystemPowerOff ();
                }
            break;

        case SYSTEM_SHUTDOWN:
            if ( ! Get_PWR_OK () )
            {
                SystemPowerOff( "ATX Power Failure" );
                }
            else if ( Is_ATX_PS && poffsw_timer >= 1350 ) // Pressed 'F', T >= 3s
            {
                SystemPowerOff ();
                }
            else if ( ! Is_ATX_PS && state_timer >= 900 ) // 2s
            {
                SystemPowerOff ();
                }
            break;

        case SYSTEM_REBOOT:
            if ( ! Get_PWR_OK () )
            {
                SystemPowerOff( "ATX Power Failure" );
                }
            else if ( Is_ATX_PS && poffsw_timer >= 1350 ) // Pressed 'F', T >= 3s
            {
                SystemPowerOff ();
                }
            else if ( state_timer >= 900 ) // 2s
            {
                state = SYSTEM_BOOT;
                state_timer = 0;
                state_bootseq = 0;
                boot_dots = 0;
                }
            break;

        case WAIT_POWER_OFF:
            if ( ! Get_PWR_OK () )
            {
                state = SYSTEM_POWER_OFF;
                state_timer = -1;
                }
            else if ( state_timer >= 900 ) // 2s
            {
                state = POWER_OFF_FAILED;
                state_timer = -1;
                }
            break;

        case POWER_OFF_FAILED:
            if ( ! Get_PWR_OK () )
            {
                state = SYSTEM_POWER_OFF;
                state_timer = -1;
                }
            break;

        case SYSTEM_FAILED:
            if ( Is_ATX_PS && poffsw_timer == 0 ) // Pressed 'F'
            {
                lcd.Set_PWM_BL( 96 );
                SystemPowerOff ();
                }
            else if ( state_timer >= 13500 ) // 30 sec
            {
                lcd.Set_PWM_BL( 0 );
                state_timer = -1;
                sysfail_desc = 0;
                lcd.SetPosition( 0 );
                lcd.SendData( "                    " );
                lcd.SetBrightness( 3 ); // 100%
                // lcd.TurnOff ();
                }
            else if ( IsReceived( 5, "Linux" ) )
            {
                lcd.Set_PWM_BL( 96 );
                state = SYSTEM_BOOT;
                state_timer = 0;
                state_bootseq = 0;
                sysfail_desc = 0;
                boot_dots = 0;
                old_state = INITIAL_STATE; // force display init
                }
            break;
        }

    if ( old_state == state )
        return;

    if ( old_state == SYSTEM_READY || old_state == INITIAL_STATE )
    {
        lcd.SetBrightness( 3 );
        /* 
            CHAR \x01        CHAR \x02    

            * * * * *  1F    * - - - *  11
            - - * - -  04    * * - * *  1B
            - - * - -  04    * - * - *  15
            - - * - -  04    * - - - *  11
            - - - - -  00    - - - - -  00
            - - - - -  00    - - - - -  00
            - - - - -  00    - - - - -  00
            - - - - -  00    - - - - -  00
        */
        lcd.SetChar5x8( 1 );
        lcd.SendData( 8, "\x1F\x04\x04\x04\x00\x00\x00\x00" );
        lcd.SendData( 8, "\x11\x1B\x15\x11\x00\x00\x00\x00" );

        lcd.SetPosition( 0 );
        lcd.SendData( "\xFF IPTC Albatross\x01\x02 \xFF" );
        }

    // Erase old & display new status
    //
    EraseState ();
    DisplayState ();

    // Set LED indicator accoring status
    //
    if ( state == SYSTEM_READY )
    {
        indicatorLED.SetCadence( -1, 1, 60, 1, 388 );
        lcd.Clear ();
        }
    else if ( state == SYSTEM_BOOT
        || state == SYSTEM_SHUTDOWN
        || state == SYSTEM_REBOOT
        )
    {
        indicatorLED.SetCadence( -1, 400, 20, 10, 20 );
        }
    else if ( state == SYSTEM_POWER_OFF )
    {
        indicatorLED.SetCadence( -1, 150, 150 );
        }
    else
    {
        indicatorLED.TurnOn ();
        }
    }

bool ATXCTRL:: OnUSARTReceive( int ch )
{
    // Pass USART data to user in "SYSTEM_READY" state 
    //
    if ( state == SYSTEM_READY )
        return true;

    if ( state == SYSTEM_BOOT
        || state == SYSTEM_SHUTDOWN
        || state == SYSTEM_REBOOT
        )
    {
        // Reset ON_TIMEOUT counter in all states expecting USART input
        //
        state_timer = 0;
        }

    // Otherwise it is boot, reboot or shutdown,
    // i.e. we are monitoring USART input
    //
    if ( ch == '\n' )
    {
        recv_lineptr = 0;
        recv_line[ 0 ] = 0;
        }
    else if ( recv_lineptr <= 79 )
    {
        recv_line[ recv_lineptr++ ] = ch;
        recv_line[ recv_lineptr ] = 0;
        }

    return false;
    }

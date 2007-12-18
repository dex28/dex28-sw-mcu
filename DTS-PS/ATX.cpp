
#include "ATX.h"

ATXCTRL atx;

///////////////////////////////////////////////////////////////////////////////

const char* ATXCTRL:: STATE_DESC [] =
{
    "???               ",
    "System Power Off  ",
    "Waiting Power On  ",
    "Power On Failed   ",
    "System Power OK   ",
    "Booting           ",
    "System Ready      ",
    "Waiting Power Off ",
    "Power Off Failed  ",
    "???               "
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
    Is_ATX_PassiveOnOff = true;

    state = INITIAL_STATE;
    state_timer = -1;
    sysfail_desc = 0;

    poffsw_timer = -1;

    state_bootseq = 0;
    boot_dots = 0;

    FlushRecv ();

    indicatorLED.TurnOff ();
    }

bool ATXCTRL:: IsReceived( int min_len, const char* str )
{
    if ( recv_length == 0 || recv_length < min_len )
        return false;

    for ( const char* x = recv_line; *x && *str && *x == *str; x++, str++ )
        {}

    return ! *str; // perfect match if 'str' points to EOS
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
    else if ( Is_ATX_PS && ! Is_ATX_PassiveOnOff )
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
            if ( Is_ATX_PS )
            {
                Is_ATX_PassiveOnOff = false;
                state = SYSTEM_POWER_OFF;
                state_timer = -1;
                }
            else
            {
                Is_ATX_PassiveOnOff = true;
                state = SYSTEM_POWER_OK;
                state_timer = 0;
                }
            break;

        case SYSTEM_POWER_OFF:
            if ( Is_ATX_PS )
            {
                if ( Get_PWR_OK () )
                {
                    Is_ATX_PassiveOnOff = true;
                    state = SYSTEM_POWER_OK;
                    state_timer = 0;
                    }
                else if ( poffsw_timer == 0 ) // Pressed 'F'
                {
                    Is_ATX_PassiveOnOff = false;
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
                SystemPowerOff( Is_ATX_PassiveOnOff ? 0 : "ATX Power Failure" );
                }
            else if ( Is_ATX_PS && poffsw_timer >= 1350 ) // Pressed 'F', T >= 3s
            {
                SystemPowerOff ();
                }
            else if ( state_timer >= 900 ) // 2s
            {
                // Flush input buffer
                FlushRecv ();

                state = SYSTEM_BOOT;
                state_timer = 0;
                state_bootseq = 0;
                boot_dots = 0;
                }
            break;

        case SYSTEM_BOOT:
            if ( ! Get_PWR_OK () )
            {
                SystemPowerOff( Is_ATX_PassiveOnOff ? 0 : "ATX Power Failure" );
                }
            else if ( Is_ATX_PS && poffsw_timer >= 1350 ) // Pressed 'F', T >= 3s
            {
                SystemPowerOff ();
                }
            else 
            {
                switch( state_bootseq )
                {
                    case 0: // Expect "Linux version"
                        if ( state_timer >= 9000 ) // 20s
                        {
                            SystemPowerOff( "Boot Failed : #01" );
                            }
                        else if ( IsReceived( 14, "Linux version " )
                            ||    IsReceived( 15, "Kernel command " ) )
                        {
                            state_bootseq = 1;
                            state_timer = 0;
                            lcd.SetPosition( 0 );
                            lcd.SendData( "\xFF IPTC ELU28GW-12P \xFF" );
                            lcd.SetPosition( 1 );
                            // lcd.SendData( "  Kernel  " );
                            lcd.SendData( "  Linux Kernel      " );
                            }
                        break;

                    case 1: // Expect "INIT:"
                        if ( state_timer >= 9000 ) // 20s
                        {
                            SystemPowerOff( "Boot Failed : #02" );
                            }
                        else if ( IsReceived( 10, "Power down" ) )
                        {
                            SystemPowerOff( "Power down!" );
                            }
                        else if ( IsReceived( 13, "System halted" ) )
                        {
                            SystemPowerOff( "System halted!" );
                            }
                        else if ( IsReceived( 10, "Restarting" ) )
                        {
                            // Reboot
                            state = SYSTEM_BOOT;
                            state_timer = 0;
                            state_bootseq = 0;
                            boot_dots = 0;

                            lcd.SetPosition( 1 );
                            lcd.SendData( "  Booting           " );
                            }
                        else if ( IsReceived( 10, "INIT: alba" ) )
                        {
                            // Ready for user to access display.
                            state = SYSTEM_READY;
                            state_timer = -1;
                            }
                        else if ( IsReceived( 6, "INIT:" ) )
                        {
                            state_timer = 0;
                            lcd.SetPosition( 1, 0 );
                            // Display the rest after INIT:
                            int i = 0;
                            for ( const char* str = recv_line + 6; *str && i < 20; str++, i++ )
                                lcd.SendData( *str );
                            for ( ; i < 20; i++ )
                                lcd.SendData( ' ' );
                            }
                        break;

                    default: // Bug!
                        SystemPowerOff( "Boot Failed : #FF" );
                    }

                if ( ( state_timer & 0x1F ) == 0 )
                {
                    if ( ++boot_dots >= 4 )
                        boot_dots = 0;

                    lcd.SetPosition( 1, 19 );
                    lcd.SendData( "|/-\x03"[ boot_dots ] );
                    }
/*
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
*/
                }
            break;

        case SYSTEM_READY:
            if ( ! Get_PWR_OK () )
            {
                SystemPowerOff( Is_ATX_PassiveOnOff ? 0 : "ATX Power Failure" );
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
            else if ( Is_ATX_PS && Is_ATX_PassiveOnOff && ! Get_PWR_OK () )
            {
                SystemPowerOff (); // System has been manually switched off
                }
            else if ( state_timer >= 13500 ) // 30 sec
            {
                lcd.Set_PWM_BL( 0 );
                state_timer = -1;
                sysfail_desc = 0;
                lcd.Clear ();
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
            CHAR \x01        CHAR \x02        CHAR \x03    

            * * * * *  1F    * - - - *  11    - - - - -  00
            - - * - -  04    * * - * *  1B    * - - - -  10
            - - * - -  04    * - * - *  15    - * - - -  08
            - - * - -  04    * - - - *  11    - - * - -  04
            - - - - -  00    - - - - -  00    - - - * -  02
            - - - - -  00    - - - - -  00    - - - - *  01
            - - - - -  00    - - - - -  00    - - - - -  00
            - - - - -  00    - - - - -  00    - - - - -  00
        */
        lcd.SetChar5x8( 1 );
        lcd.SendData( 8, "\x1F\x04\x04\x04\x00\x00\x00\x00" );
        lcd.SendData( 8, "\x11\x1B\x15\x11\x00\x00\x00\x00" );
        lcd.SendData( 8, "\x00\x10\x08\x04\x02\x01\x00\x00" );

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
        indicatorLED.SetCadence( -1, 1, 30, 1, 868 );
        lcd.Clear ();
        }
    else if ( state == SYSTEM_BOOT )
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

void ATXCTRL::OnTimer( void )
{
    if ( state_timer >= 0 )
        ++state_timer;

    StateMachine ();
    }

void ATXCTRL::FlushRecv( void )
{
    recv_curptr = 0;
    recv_length = 0;
    recv_line[ 0 ] = 0;
    }

bool ATXCTRL:: OnUSARTReceive( int ch )
{
    // Pass USART data to user in "SYSTEM_READY" state 
    //
    if ( state == SYSTEM_READY )
    {
        if ( ch == 0x18 ) // cancel user mode
        {
            // Flush input buffer
            FlushRecv ();

            state = SYSTEM_BOOT;
            state_timer = 0;
            state_bootseq = 1;
            boot_dots = 0;

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

            // Erase old & display new status
            //
            EraseState ();
            
            lcd.SetPosition( 1, 0 );
            lcd.SendData( "Rebooting " );

            // Set LED indicator accoring status
            //
            indicatorLED.SetCadence( -1, 400, 20, 10, 20 );

            return true;
            }

        return false;
        }

    if ( state == SYSTEM_BOOT )
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
        recv_line[ recv_curptr ] = 0; // terminate line
        recv_length = recv_curptr; // Indicate EOL; this will trigger IsReceived ()

        StateMachine (); // IsReceived () trigger

        FlushRecv ();
        }
    else if ( recv_curptr <= 79 && ch != '\r' )
    {
        recv_line[ recv_curptr++ ] = ch;
        }

    return true;
    }

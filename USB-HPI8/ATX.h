#ifndef _ATX_H_INCLUDED
#define _ATX_H_INCLUDED

#include "Cadence.h"
#include "Keyboard.h"
#include "LCD.h"
#include "USART.h"

class ATXCTRL
{
    //  PC1   OUT   LED INDICATOR
    //  PC0   OUT   ATX-PS-ON
    //  PB0   IN    ATX-PWR-OK     (w/ internal pullup)

public:

    enum STATE
    {
        INITIAL_STATE,
        SYSTEM_POWER_OFF,
        WAIT_POWER_ON,
        POWER_ON_FAILED,
        SYSTEM_POWER_OK,
        SYSTEM_BOOT,
        SYSTEM_READY,
        SYSTEM_SHUTDOWN,
        SYSTEM_REBOOT,
        WAIT_POWER_OFF,
        POWER_OFF_FAILED,
        SYSTEM_FAILED
        };

    bool Is_ATX_PS;
    CADENCE indicatorLED;

    STATE state;
    int16_t state_timer;
    static const char* STATE_DESC [];
    const char* sysfail_desc;

    int16_t poffsw_timer; // power sw on/off timer

    int16_t state_bootseq; // SYSTEM_BOOT sequence#
    int boot_dots;

    // Boot input USART recv line monitor
    char recv_line[ 81 ];
    int recv_lineptr;

    void Initialize( void );

    bool Get_PWR_OK( void ) const
    {
        return ( PINB & _BV(DDB0) ) != 0;
        }

    bool Get_LED_IND( void ) const
    {
        return ( PORTC & _BV(PC1) ) != 0;
        }

    bool Get_ATX_PS_ON( void ) const
    {
        return ( PORTC & _BV(PC0) ) != 0;
        }

    void Set_LED_IND_ON( void )
    {
        PORTC |= _BV(PC1);
        }

    void Set_LED_IND_OFF( void )
    {
        PORTC &= ~_BV(PC1);
        }

    void Set_ATX_PS_ON( void )
    {
        PORTC |= _BV(PC0);
        }

    void Set_ATX_PS_OFF( void )
    {
        PORTC &= ~_BV(PC0);
        }

    bool IsReceived( int min_len, const char* y );

    void UpdateIndicatorLED( void );

    void DisplayState( void );
    void EraseState( void );

    void SystemPowerOff( const char* sysFailed = 0 );

    void StateMachine( void );

    bool OnUSARTReceive( int ch );
    };

extern ATXCTRL atx;

#endif // _ATX_H_INCLUDED

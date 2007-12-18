#ifndef _LCD_H_INCLUDED
#define _LCD_H_INCLUDED

#include <inttypes.h>
#include <avr/io.h>
#include <compat/ina90.h>

#include "Cadence.h"

class LCD
{
    //  PD7   OUT                LCD D7
    //  PD6   OUT                LCD D6
    //  PD5   OUT                LCD D5
    //  PD4   OUT                LCD D4
    //  PC3   OUT                LCD E
    //  PC2   OUT                LCD RS
    //  PB2   OUT                OC1B: PWM for LCD backlight
    //  PB1   OUT                OC1A: PWM for LCD op. voltage

    int16_t pwmV_LCD; // V_LCD pwm
    int16_t pwmI_BL; // I_BL pwm

    enum RS_FLAG
    {
	    RS_INSTR        = 0x00,
	    RS_DATA         = 0x01
        };

    enum INSTRUCTION
    {
	    CLEAR           = 0x01,

	    HOMECURSOR      = 0x02,

	    ENTRYMODE       = 0x04,
	      E_MOVERIGHT   = 0x02,
	      E_MOVELEFT    = 0x00,
	      EDGESCROLL    = 0x01,
	      NOSCROLL      = 0x00,

	    ONOFFCTRL       = 0x08,
	      DISPON        = 0x04,
	      DISPOFF       = 0x00,
	      CURSORON      = 0x02,
	      CURSOROFF     = 0x00,
	      CURSORBLINK   = 0x01,
	      CURSORNOBLINK = 0x00,

	    CURSORSHIFT     = 0x10,
	      SCROLLDISP    = 0x08,
	      MOVECURSOR    = 0x00,
	      MOVERIGHT     = 0x04,
	      MOVELEFT      = 0x00,

	    FUNCSET         = 0x20,
	      IF_8BIT       = 0x10,
	      IF_4BIT       = 0x00,
	      TWOLINE       = 0x08,
	      ONELINE       = 0x00,
	      LARGECHAR     = 0x04, // 5x11 characters
	      SMALLCHAR     = 0x00, // 5x8 characters

	    SETCHAR         = 0x40,

        SETPOSITION     = 0x80
        };

    void uPause( uint16_t microSeconds )
    {
        while( microSeconds-- > 0 )
        {
            _NOP (); _NOP ();
            _NOP (); _NOP ();
            _NOP (); _NOP ();
            }
        }

    void SendData_Hi( RS_FLAG flag, int data, uint16_t usec );

    void SendData( RS_FLAG flag, int data, uint16_t usec );

public:

    CADENCE blinkLCD; // blink LCD cadence

    void Initialize( void );
    void Setup_4bit( void );

    int16_t Get_PWM_LCD( void ) const
    {
        return pwmV_LCD;
        }

    int16_t Get_PWM_BL( void ) const
    {
        return pwmI_BL;
        }

    void Set_PWM_LCD( int16_t v );

    void Set_PWM_BL( int16_t v );

    void SetBrightness( int16_t v ); // VFD only

    void SendData( char data )
    {
        SendData( RS_DATA, data, 40 );
        }

    void SendData( const char* data );

    void SendData( uint16_t len, const char* data );

    void SetPosition( int row, int column = 0 )
    {
        SendData( RS_INSTR, SETPOSITION | ( row << 6 ) | column, 40 );
        }

    void SetChar5x8( int hi, int low = 0 )
    {
        SendData( RS_INSTR, SETCHAR | ( hi << 3 ) | low, 40 );
        }

    void SetCursor( bool on, bool blink )
    {
        SendData( RS_INSTR, ONOFFCTRL | DISPON 
            | ( on ? CURSORON : CURSOROFF ) 
            | ( blink ? CURSORBLINK : CURSORNOBLINK )
            , 40 );
        }

    void Clear( void )
    {
	    SendData( RS_INSTR, CLEAR, 1600 );
        SetPosition( 0, 0 );
        }
    };

extern LCD lcd;

#endif // _LCD_H_INCLUDED


#include "LCD.h"

LCD lcd;

///////////////////////////////////////////////////////////////////////////////

void LCD::Initialize( void )
{
    blinkLCD.SetCadence( -1, 1, 224, 1, 224 );
    pwmV_LCD = 0;
    pwmI_BL = 0;

    // Enable PB1/OC1A and PB2/OC1B as outputs / default low
    //
    PORTD &= ~_BV(PB1) & ~_BV(PB2);
    DDRB = _BV(DDB2) | _BV(DDB1);
    _NOP ();

    // Clear timer counter
    TCNT1H = 0;
    TCNT1L = 0;

//       ---TCCR1B-- ---TCCR1A--
//  Mode WGM13 WGM12 WGM11 WGM10                              TOP     OCR1x   TOV1
//            (CTC1  PWM11 PWM10)                                   Update  Set On
//  ------------------------------------------------------------------------------
//    0    0     0     0     0   Normal                     0xFFFF  Immed   MAX
//    1    0     0     0     1   PWM, Phase Correct, 8-bit  0x00FF  TOP     BOTTOM
//    2    0     0     1     0   PWM, Phase Correct, 9-bit  0x01FF  TOP     BOTTOM
//    3    0     0     1     1   PWM, Phase Correct, 10-bit 0x03FF  TOP     BOTTOM
//    4    0     1     0     0   CTC                        OCR1A   Immed   MAX
//    5    0     1     0     1   Fast PWM, 8-bit            0x00FF  TOP     TOP
//    6    0     1     1     0   Fast PWM, 9-bit            0x01FF  TOP     TOP
//    7    0     1     1     1   Fast PWM, 10-bit           0x03FF  TOP     TOP
//    8    1     0     0     0   PWM, Phase&Freq Correct    ICR1    BOTTOM  BOTTOM
//    9    1     0     0     1   PWM, Phase&Freq Correct    OCR1A   BOTTOM  BOTTOM
//   10    1     0     1     0   PWM, Phase Correct         ICR1    TOP     BOTTOM
//   11    1     0     1     1   PWM, Phase Correct         OCR1A   TOP     BOTTOM
//   12    1     1     0     0   CTC ICR1 Immediate MAX
//   13    1     1     0     1   (Reserved)
//   14    1     1     1     0   Fast PWM                   ICR1    TOP     TOP
//   15    1     1     1     1   Fast PWM                   OCR1A   TOP     TOP
//
//  CS12 CS11 CS10  Description (TCCR1B)
//  ------------------------------------------------------------------------------
//   0    0    0    No clock source. (Timer/Counter stopped)
//   0    0    1    clkI/O/1 (No prescaling)
//   0    1    0    clkI/O/8 (From prescaler)
//   0    1    1    clkI/O/64 (From prescaler)
//   1    0    0    clkI/O/256 (From prescaler)
//   1    0    1    clkI/O/1024 (From prescaler)
//   1    1    0    External clock source on T1 pin. Clock on falling edge.
//   1    1    1    External clock source on T1 pin. Clock on rising edge.
//
    // Timer1 is 8-bit Fast PWM running on CPU clock (7.3728MHz/256 = 28.8kHz PWM)
    //
    TCCR1A = _BV(WGM10);
    TCCR1B = _BV(WGM12) | _BV(CS10);

    // Set PWM default values
    //
    OCR1A = pwmV_LCD;
    OCR1B = pwmI_BL;

    // Enable PC2 and PC3 as outputs / both default low
    //
    PORTC &= ~_BV(PC2) & ~_BV(PC3);
    DDRC  |= _BV(DDC2) | _BV(DDC3);
    _NOP ();

    // Enable PD4..PD7 as outputs / default high
    //
    PORTD |= _BV(PD7) | _BV(PD6) | _BV(PD5) | _BV(PD4);
    DDRD  |= _BV(DDD7) | _BV(DDD6) | _BV(DDD5) | _BV(DDD4);
    _NOP ();
    }

void LCD::Setup_4bit( void )
{
    extern void Timer0_Wait( int count );

    // Wait 260ms after Vcc > 4.75V (VFD)
    Timer0_Wait( 120 ); // 120/450 = 267ms

    // Wait 30ms after Vcc > 4.75V (LCD)
    Timer0_Wait( 15 ); // 15/450 = 33ms

	// Setup the LCD in 4 bit mode
	//
	SendData_Hi( RS_INSTR, FUNCSET | IF_8BIT, 4100 );
	SendData_Hi( RS_INSTR, FUNCSET | IF_8BIT, 100 );
	SendData_Hi( RS_INSTR, FUNCSET | IF_8BIT, 100 );

	SendData_Hi( RS_INSTR, FUNCSET | IF_4BIT, 100 );
    SendData( RS_DATA, 0x03, 40 ); // 25% brightness

	SendData( RS_INSTR, FUNCSET | IF_4BIT | TWOLINE | SMALLCHAR, 40 );

	// Sets cursor off and not blinking,
	// clear display and homecursor
	//
	SendData( RS_INSTR, ONOFFCTRL | DISPOFF | CURSOROFF, 40 );
	SendData( RS_INSTR, ONOFFCTRL | DISPON | CURSOROFF | CURSORNOBLINK, 40 );
	SendData( RS_INSTR, CLEAR, 1600 );
	SendData( RS_INSTR, HOMECURSOR, 1600 );

    SetChar5x8( 0, 0 );
    for ( int i = 0; i < 8 * 8; i++ )
        SendData( 0x1F );

    SetPosition( 0, 0 );
    }

void LCD:: Set_PWM_LCD( int16_t v )
{
    if ( v < 0 )
        v = 0;
    else if ( v >= 0x100 )
        v = 0x0FF;

    OCR1A = pwmV_LCD = v;

    if ( v == 0 ) 
        TCCR1A &= ~_BV(COM1A1); // Disconnect OC1A, i.e. set OC1A low
    else 
        TCCR1A |= _BV(COM1A1); // Clear OC1A on Compare Match (Set output to low level)
    }

void LCD:: Set_PWM_BL( int16_t v )
{
    if ( v < 0 )
        v = 0;
    else if ( v >= 0x100 ) 
        v = 0x0FF;

    OCR1B = pwmI_BL = v;

    if ( v == 0 )
        TCCR1A &= ~_BV(COM1B1);  // Disconnect OC1B, i.e. set OC1B low
    else
        TCCR1A |= _BV(COM1B1); // Clear OC1B on Compare Match (Set output to low level)
    }

void LCD:: SetBrightness( int16_t v )
{
    return; // VFD only

	SendData_Hi( RS_INSTR, FUNCSET | IF_8BIT, 100 );
	SendData_Hi( RS_INSTR, FUNCSET | IF_8BIT, 100 );

    SendData_Hi( RS_INSTR, FUNCSET | IF_4BIT, 100 );
    SendData( RS_DATA, v & 0x03, 40 );
    }

void LCD:: SendData_Hi( RS_FLAG flag, int data, uint16_t usec )
{
    // Set RS & Data bits
    //
    // upper LCD D4..D7 bits goes to PORTD PD4..PD7
    // RS goes to PC3
    //
    PORTC &= ~0x04;
    PORTC |= ( flag == RS_DATA ? 0x04 : 0x00 );
    // Hi nibble only
    PORTD &= ~0xF0;
    PORTD |= ( data & 0xF0 );

    // Assert E
    //
    PORTC |= _BV(PC3);

    _NOP (); // 135ns
    _NOP (); // 135ns

    // Deassert E
    //
    PORTC &= ~_BV(PC3);

    // Wait command to execute
    uPause( usec );
    }

void LCD:: SendData( RS_FLAG flag, int data, uint16_t usec )
{
    // Set RS & Data bits
    //
    // upper LCD D4..D7 bits goes to PORTD PD4..PD7
    // RS goes to PC3
    //
    PORTC &= ~0x04;
    PORTC |= ( flag == RS_DATA ? 0x04 : 0x00 );
    // High nibble
    PORTD &= ~0xF0;
    PORTD |= ( data & 0xF0 );

    // Assert E
    //
    PORTC |= _BV(PC3);

    _NOP (); // 135ns
    _NOP (); // 135ns

    // Deassert E
    //
    PORTC &= ~_BV(PC3);

    _NOP (); // 135ns

    // Low nibble
    PORTD &= ~0xF0;
    PORTD |= ( ( data << 4 ) & 0xF0 );

    // Assert E
    //
    PORTC |= _BV(PC3);

    _NOP (); // 135ns
    _NOP (); // 135ns

    // Deassert E
    //
    PORTC &= ~_BV(PC3);

    // Wait command to execute
    uPause( usec );
    }

void LCD:: SendData( const char* data )
{
    while( *data )
        SendData( RS_DATA, *data++, 40 );
    }

void LCD:: SendData( uint16_t len, const char* data )
{
    while ( len-- > 0 )
        SendData( RS_DATA, *data++, 40 );
    }

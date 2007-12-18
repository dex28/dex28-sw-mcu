
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

#include <compat/ina90.h>

#include <string.h>
#include <stdio.h>

#include "Cadence.h"
#include "FT245.h"
#include "DASL.h"
#include "ELU28.h"
#include "TAU-D.h"

///////////////////////////////////////////////////////////////////////////////

#define TAU_D_REVID "TAU D R1AUSB"

///////////////////////////////////////////////////////////////////////////////

volatile unsigned int SysTimer = 0;
Cadence  led;
USB_FIFO usb;
TAU_D tau;
ELU28_D_Channel PBX( 0 );
ELU28_D_Channel DTS( 1 );
bool trace = true;

///////////////////////////////////////////////////////////////////////////////

/*
    MCU:          Flash   EEPROM   SRAM
    ------------------------------------
    ATMega64        64k     2k      4k
    ATMega128      128k     4k      4k
    ATMega1281     128k     4k      8k
    ATMega2561     256k     4k      8k

    Oscillator: 
    
      External Crystal 8.192 MHz, C1, C2 = 22pF

      CKSEL3..0 = 1010..1111 for External Crystal

      CKOPT = 1, CKSEL3..1 = 111 for Crystal Osc. Freq = 3..8 MHz
      CKOPT = 0, CKSEL3..1 = 101,110,111 for Crystal Osc. Freq > 1MHz
      CKOPT = 0 turns on internal oscillator amplifier

      CKSEL0 = 1, SUT1..0 = 01 for Crystal Osc. and BOD Enabled

    Fuses:

    Extended Fuse Byte: 0xFF

      ~M103C    1 ATmega103 compatibility mode
      ~WDTON    1 Watchdog Timer always on

    Fuse High Byte = 0x99

      ~OCDEN    1 Enable OCD
      ~JTAGEN   0 Enable JTAG
      ~SPIEN    0 Enable Serial Program andData Downloading, = 0
      CKOPT     1 Oscillator options
      ~EESAVE   1 EEPROM memory is preserved through the Chip Erase
      BOOTSZ1   0 Select Boot Size
      BOOTSZ0   0 Select Boot Size 
      BOOTRST   1 Select Reset Vector

    Fuse Low Byte = 0x1F

      BODLEVEL  0 Brown out detector trigger level
      ~BODEN    0 Brown out detector enable
      SUT1      0 Select start-up time
      SUT0      1 Select start-up time
      CKSEL3    1 Select Clock source
      CKSEL2    1 Select Clock source
      CKSEL1    1 Select Clock source
      CKSEL0    1 Select Clock source
*/

///////////////////////////////////////////////////////////////////////////////

void DumpPDU( const char* from, const unsigned char* data, int data_len )
{
    printf( "%06u %s:", SysTimer, from );
    for ( int i = 0; i < data_len; i++ )
        printf( " %02X", int( data[ i ] ) );
    printf( "\r\n" );
    }

///////////////////////////////////////////////////////////////////////////////

void Freeze_CPU( void )
{
    for ( ;; ) {} // This will trigger watchdog
    }

///////////////////////////////////////////////////////////////////////////////

void Configure_Pins( void )
{
    ///////////////////////////////////////////////////////////////////////////
    // Configure Pins (TQFP64):

    ///////////////////////////////////////////////////////////////////////////
    // PORTB:
    //
    //    10     PB0    OUT     SPI      ~SS    -           -
    //    11     PB1    OUT     SPI      SCK    DASL        CCLK
    //    12     PB2    OUT     SPI      MOSI   DASL        CIN
    //    13     PB3    IN,PUP  SPI      MISO   DASL        COUT
    //    14     PB4    OUT     TIMER0   OC0    DASL        MCLK
    //    15     PB5    -       -        -      -           -
    //    16     PB6    OUT     GPIO     PB6    DASL        ~CCS0
    //    17     PB7    OUT     GPIO     PB7    DASL        ~CCS1
    //
    PORTB = _BV(PB7) | _BV(PB6) | _BV(PB3);
    DDRB  = _BV(PB7) | _BV(PB6) | _BV(PB4) | _BV(PB2) | _BV(PB1) | _BV(PB0);

    ///////////////////////////////////////////////////////////////////////////
    // PORTC:
    //
    //    35-42  PC0-7  IO,PUP  GPIO     PC0-7  USB FIFO    D0-7
    //
    PORTC = 0xFF;
    DDRC  = 0x00;

    ///////////////////////////////////////////////////////////////////////////
    // PORTD:
    //
    //    25     PD0    IN,PUP  GPIO     ~INT0  USB FIFO    ~RXF
    //    26     PD1    IN,PUP  GPIO     ~INT1  USB FIFO    ~TXE
    //    27     PD2    IN      USART1   RXD1   DASL1       DR1
    //    28     PD3    OUT     USART1   TXD1   DASL1       DX1
    //    29     PD4    -       -        -      -           -
    //    30     PD5    IN      USART1   XCK1   DASL0       DCLK
    //    31     PD6    -       -        -      -           -
    //    32     PD7    -       -        -      -           -
    //
    PORTD = _BV(PD0) | _BV(PD1);
    DDRD  = _BV(PD3);

    ///////////////////////////////////////////////////////////////////////////
    // PORTE:
    //
    //    2      PE0    IN      USART0   RXD0   USART0      DR0
    //    3      PE1    OUT     USART0   TXD0   USART0      DX0
    //    4      PE2    IN      USART0   XCK0   USART0      DCLK
    //    5      PE3    -       -        -      -           -
    //    6      PE4    OUT     TIMER3   OC3B   Status LED  Red
    //    7      PE5    -       -        -      -           -
    //    8      PE6    -       -        -      -           -
    //    9      PE7    IN,PUP  GPIO     ~INT7  MICROWIRE   ~CINT
    //
    PORTE  = _BV(PE4) | _BV(PE7);
    DDRE   = _BV(PE4) | _BV(PE1);

    ///////////////////////////////////////////////////////////////////////////
    // PORTG:
    //
    //    33     PG0    OUT     GPIO     PG0    USB FIFO    ~WR
    //    34     PG1    OUT     GPIO     PG1    USB FIFO    ~RD
    //
    PORTG  = _BV(PG1) | _BV(PG0);
    DDRG   = _BV(PG1) | _BV(PG0);
    }

void Initialize_Timers ( void )
{
    ///////////////////////////////////////////////////////////////////////////
    // Timer 0: OC0 as to DASL MCLK 2.048 MHz (= CLK/4)
    //
    // CTC mode, OC0 toggle, N (prescaller) = 1; OCR0 = 1
    // f = 8.192 MHz / ( 2 * N * ( 1 + OCR0 ) )
    // gives f = 2.048 MHz
    //
    OCR0  = 1;
    TCCR0 = _BV(WGM01 ) | _BV(COM00) | _BV(CS00);

    ///////////////////////////////////////////////////////////////////////////
    // Timer 1: Timestamp Timer: 32 kHz
    //
    // Normal mode, clk/256 prescaller gives T = 31.25 us period
    //
#if 0
    TCNT1  = 0;
    TCCR1B = _BV(CS12);
#endif

    ///////////////////////////////////////////////////////////////////////////
    // Timer 2: Main System Timer 1 kHz
    //
    // CTC mode, OC2 disconnected, OC2 triggers interrupt
    // f = 8.192 MHz / ( 2 * N * ( 1 + OCR2 ) )
    // N = 64 (prescaller), OCR2 = 127
    //
    TCNT2 = 0;
    OCR2  = 127;
    TCCR2 = _BV(WGM21) | _BV(CS21) | _BV(CS20);
    TIMSK |= _BV(OCIE2);

    ///////////////////////////////////////////////////////////////////////////
    // Timer 3: OC3B as 32 kHz PWM drive to Status LED
    //
    // 8-bit Fast PWM running on CPU clock (8.192MHz/256 = 32kHz PWM)
    // Note: OC3B will be disconnected. Set_LED() should connect it later.
    //
    TCNT3  = 0;
    OCR3B  = 0;
    TCCR3A = _BV(WGM30);
    TCCR3B = _BV(WGM32)  | _BV(CS32);
    }

///////////////////////////////////////////////////////////////////////////////

void Set_LED( int16_t v )
{
    if ( v <= 0 )
    {
        PORTE  |= _BV(PE4);                    // Clear LED
        TCCR3A &= ~_BV(COM3B1) & ~_BV(COM3B0); // Disconnect OC3B
        }
    else if ( v >= 0xFF )
    {
        PORTE  &= ~_BV(PE4);                   // Set LED
        TCCR3A &= ~_BV(COM3B1) & ~_BV(COM3B0); // Disconnect OC3B
        }
    else
    {
        OCR3B  = v;
        TCCR3A |= _BV(COM3B1) | _BV(COM3B0);   // Connect OC3B
        }
    }

///////////////////////////////////////////////////////////////////////////////
// Interrupt handlers
//
// Note on SIGNAL() vs INTERRUPT():
//     SIGNAL()    => Global IRQs are disabled when entering function
//     INTERRUPT() => Global IRQs are enabled when entering function

SIGNAL( SIG_USART0_RECV ) // USART0, Rx Complete
{
    PBX.rcv_buf.PutOctet( UDR0 );
    }

SIGNAL( SIG_USART0_TRANS ) // USART0, Tx Empty
{
    int ch = PBX.xmt_que.GetOctet ();
    if ( ch >= 0 )
        UDR0 = ch;
    }

SIGNAL( SIG_USART1_RECV ) // USART1, Rx Complete
{
    DTS.rcv_buf.PutOctet( UDR1 );
    }

SIGNAL( SIG_USART1_TRANS ) // USART1, Tx Empty
{
    int ch = DTS.xmt_que.GetOctet ();
    if ( ch >= 0 )
        UDR1 = ch;
    }

volatile bool SysTimer_Event = false;

SIGNAL( SIG_OUTPUT_COMPARE2 ) // Timer/Counter2 Overflow => System Timer
{
    ++SysTimer;
    SysTimer_Event = true;

    PBX.DecTimeoutTimer ();
    DTS.DecTimeoutTimer ();
    }

///////////////////////////////////////////////////////////////////////////////

void SPI_Initialize( void )
{
    // Configure SPI: Enabled, Master, FCLK/8
    //
    SPCR = _BV(SPE) | _BV(MSTR) | _BV(SPR0);
    SPSR = _BV(SPI2X);
    }

///////////////////////////////////////////////////////////////////////////////

void USART0_Initialize( void )
{
    // Set frame format: sync, 8 data, 1 stop bit, no parity
    //
    UCSR0C = _BV(UMSEL0) | _BV(UCSZ01) | _BV(UCSZ00);

    // Enable Receiver, Transmitter and Receiver Interrupts
    //
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0) | _BV(TXCIE0);

    // Flush input
    //
    unsigned char dummy;
    while ( UCSR0A & _BV(RXC) )
        dummy = UDR0;
    }

void USART1_Initialize( void )
{
    // Set frame format: sync, 8 data, 1 stop bit, no parity
    //
    UCSR1C = _BV(UMSEL1) | _BV(UCSZ11) | _BV(UCSZ10);

    // Enable Receiver, Transmitter and Receiver Interrupts
    //
    UCSR1B = _BV(RXEN1) | _BV(TXEN1) | _BV(RXCIE1) | _BV(TXCIE1);

    // Flush input
    //
    unsigned char dummy;
    while ( UCSR1A & _BV(RXC) )
        dummy = UDR1;
    }

///////////////////////////////////////////////////////////////////////////////

void DASL:: Update( void )
{
    if ( id == 0 )
    {
	    PORTB &= ~_BV(PB6); // Set ~CCS0 = 0
        SPDR = control;

        do; while( ! ( SPSR & _BV(SPIF) ) ); // Wait SPI to complete

	    PORTB |= _BV(PB6); // Set ~CCS0 = 1
        }
    else
    {
	    PORTB &= ~_BV(PB7); // Set ~CCS1 = 0
        SPDR = control;

        do; while( ! ( SPSR & _BV(SPIF) ) ); // Wait SPI to complete

	    PORTB |= _BV(PB7); // Set ~CCS1 = 1
        }

    status = SPDR;

    if ( trace )
        printf( "%06u %s: DASL %02X (%02X)\r\n", SysTimer, id == 0 ? "PBX" : "DTS",  status, control );
    }

bool DASL:: IsStatusChanged( void )
{
	// Note that CINT of DASLs are open drain outputs wired-OR to CINT input
	// on CPLD, thus fullfilling prerequisites for proper 'shared level-triggered 
	// interrupt' mode. CINT will state asserted until Update() is performed
	// on all DASL's.
	//
    return ( PINE & _BV(PINE7) ) == 0; // True if ~CINT is asserted (i.e. low)
    }

///////////////////////////////////////////////////////////////////////////////

TAU_D::TAU_D( void )
{
    mode        = MODE_TRANSPARENT;
    state       = WAIT_FLG1;
    sn_from_DTS = 0x7;
    sn_from_PBX = 0x7;
    sn_to_DTE   = 0xF;
    sn_from_DTE = -1;
    }

void TAU_D::SendFrame( int ctl, int addr, unsigned char* buf, int len )
{
    if ( len > 56 ) // Do not send long ELU2B+D signals
        return;

    ++sn_to_DTE;

    // Send Flag
    //
    usb.PutOctet( FRM_FLG1 );
    usb.PutOctet( FRM_FLG2 );

    // Send BC
    //
    int BC = 4 + len;
    usb.PutOctet( BC );
    int cs = BC;

    // Send CTL
    //
    int CTL = ( ( sn_to_DTE & FRM_SN_MASK ) << FRM_SN_SHIFT ) 
              | ( ctl & FRM_CTL_MASK );
    usb.PutOctet( CTL );
    cs += CTL;

    // Send ADDR
    //
    usb.PutOctet( addr );
    cs += addr;

    // Send Data
    //
    for ( int i = 0; i < len; i++ )
    {
        usb.PutOctet( buf[ i ] );
        cs += buf[ i ];
        }

    // Send CS
    //
    usb.PutOctet( ( cs - 1 ) & 0xFF );
    }

bool TAU_D::OnReceivedOctet( int octet )
{
    // Ignore USB data if PBX link is down
    //
    if ( PBX.GetVerbState () < ELU28_D_Channel::VERBOSE_UP ) 
        return false;

    bool has_data = false;

    switch( state )
    {
        case WAIT_FLG1:
            if ( octet == '\r' ) // CR
                state = WAIT_LF;
            else if ( octet != FRM_FLG1 )
                AbortFrame ();
            else
                state = WAIT_FLG2;
            break;

        case WAIT_LF:
            if ( octet == '\r' ) // CR
                trace = true;
            else if ( octet == '\n' ) // LF
                trace = true;
            else
                AbortFrame ();
            break;

        case WAIT_FLG2:
            trace = false;
            if ( octet != FRM_FLG2 )
                AbortFrame ();
            else
                state = WAIT_BC;
            break;

        case WAIT_BC:
            CS = octet;
            if ( octet < 4 || octet > 51 )
                AbortFrame ();
            else
            {
                data_len = octet - 4;
                data_p = 0;
                state = WAIT_CTL;
            }
            break;

        case WAIT_CTL:
            CS += octet;
            CTL = octet;
            state = WAIT_ADDR;
            break;

        case WAIT_ADDR:
            CS += octet;
            ADDR = octet;
            if ( data_len == 0 )
                state = WAIT_CS;
            else
                state = WAIT_DATA;
            break;

        case WAIT_DATA:
            CS += octet;
            data[ data_p++ ] = octet;
            if ( data_p >= data_len )
                state = WAIT_CS;
            break;

        case WAIT_CS:
            if ( ( ( CS - 1 ) & 0xFF ) != octet )
            {
                // TAU-D: Invalid checksum
                AbortFrame ();
                }
            else
            {
                int sn = ( ( CTL >> FRM_SN_SHIFT ) & FRM_SN_MASK ); 

                switch( CTL & FRM_CTL_MASK )
                {
                    case FRM_CTL_DATA:

                        if ( mode & DATA_ACK_ENABLE ) // Opt. send ackonwledge
                            SendFrame( FRM_CTL_DATA_ACK );

                        // Forward data to DTS or to PBX, depending on ADDR,
                        // but ignore duplicated frames.
                        //
                        has_data = ( sn != sn_from_DTE )
                            && ( ADDR == ADDR_DTS || ADDR == ADDR_PBX );

                        break;

                    case FRM_CTL_DATA_ACK:
                        break;

                    case FRM_CTL_STATUS_REQ:
                        data[ 0 ] = mode;
                        SendFrame( FRM_CTL_STATUS_RESP, 0, data, 1 );
                        break;

                    case FRM_CTL_ID_REQ:
                        SendFrame( FRM_CTL_ID_RESP, 0, (unsigned char*)TAU_D_REVID, 12 );
                        break;

                    case FRM_CTL_COMMAND:
                        SetMode( data[0] );
                        SendFrame( FRM_CTL_COMMAND_ACK );
                        break;

                    case FRM_CTL_TEST_REQ:
                        break;

                    default:
                        // "TAU-D: Received frame with invalid control octet.\n"
                        break;
                    }
                
                sn_from_DTE = sn;
                }

            state = WAIT_FLG1;
            break;
        }

    return has_data;
    }

///////////////////////////////////////////////////////////////////////////////

int
main( void )
{
    Configure_Pins ();
    sei ();

    ///////////////////////////////////////////////////////////////////////////

    led.SetCadence( -1, 500, 500 );

    usb.Initialize ();

    SPI_Initialize ();
	PBX.dasl.Initialize( DASL::SLAVE );
	DTS.dasl.Initialize( DASL::MASTER );

    Initialize_Timers ();

    PBX.Initialize ();
    DTS.Initialize ();

    ///////////////////////////////////////////////////////////////////////////

    set_sleep_mode( SLEEP_MODE_IDLE );
    wdt_enable( WDTO_250MS ); // 250ms Watchdog

    ///////////////////////////////////////////////////////////////////////////

    if ( trace )
        printf( "Hello, world\r\n" );

    for ( ;; )
    {
        sleep_mode (); // Go CPU IDLE / Sleep
        wdt_reset ();  // We are alive: Reset Watchdog timer

        ///////////////////////////////////////////////////////////////////////
        // D channels receiver events
        //
        if ( PBX.RcvBuf_EH () ) // true if PDU received
        {
	        // If repeater mode, forward signal to other channel
	        //
            if ( tau.getMode() != 1 ) // Do not copy PBX->DTS in PC Control mode
            {
                DTS.xmt_que.PutPDU( PBX.packet, PBX.packet_len );
                }

            // Copy ELU 2B+D signal PBX->DTE
            //
            if ( trace )
                DumpPDU( "PBX", PBX.packet, PBX.packet_len );
            else
                tau.SendDataFrame( /*addr=*/ 0, PBX.packet, PBX.packet_len );
            }

        if ( DTS.RcvBuf_EH () ) // true if PDU received
        {
            if ( tau.getMode() != 1 ) // Do not copy DTS->PBX in PC Control mode
            {
                PBX.xmt_que.PutPDU( DTS.packet, DTS.packet_len );
                }

            // Copy ELU 2B+D signal DTS->DTE
            //
            if ( trace )
                DumpPDU( "DTS", DTS.packet, DTS.packet_len );
            else
                tau.SendDataFrame( /*addr=*/ 1, DTS.packet, DTS.packet_len );
            }

        ///////////////////////////////////////////////////////////////////////
        // System Timer: Every 1ms
        //
        if ( SysTimer_Event )
        {
            SysTimer_Event = false;

	        // D channel timeout events
	        //
	        PBX.Timed_EH ();
	        DTS.Timed_EH ();

            // D channel DASL events
            //
            if ( DASL::IsStatusChanged () )
            {
                PBX.DASL_EH ();
                DTS.DASL_EH ();
                }

            // LED Cadence
            //
            Set_LED( led.IsON () ? 150 : 10 );
            led.Increment ();
            }

        ///////////////////////////////////////////////////////////////////////
        // USART0: Start Transmission to PBX, if not started
        //
        if ( UCSR0A & _BV(UDRE0) ) // USART TX empty
        {
            cli ();

            int ch = PBX.xmt_que.GetOctet ();
            if ( ch >= 0 )
                UDR0 = ch;

            sei ();
            }

        ///////////////////////////////////////////////////////////////////////
        // USART1: Start Transmission to DTS, if not started
        //
        if ( UCSR1A & _BV(UDRE1) ) // USART TX empty
        {
            cli ();

            int ch = DTS.xmt_que.GetOctet ();
            if ( ch >= 0 )
                UDR1 = ch;

            sei ();
            }

        ///////////////////////////////////////////////////////////////////////
        // USB receiver events
        //
        while( usb.RXF () )
        {
            if ( tau.OnReceivedOctet( usb.GetOctet () ) ) 
            {
                // OnReceivedOctet() returns true if there exists PDU from DTE
                // ready to be copied to {PBX,DTS}.
                //
                if ( tau.getAddr () == 1 ) // Copy to DTS
                {
                    DTS.xmt_que.PutPDU( tau.getData (), tau.getDataLen () );
                    }
                else if ( tau.getAddr () == 0 ) // Copy to PBX
                {
                    // Send EQUSTA to PBX, when detected OWS startup (MBK patch)
                    //
                    if ( tau.getMode () == 1 // in PC control mode
                        && tau.getData () [1] == 0x02 && tau.getData () [2] == 0x60
                        )
                    {
                        unsigned char pdu [6] = { 0x40, 0x01, 0x80, 0x02, 0x01, 0x02 };
                        PBX.xmt_que.PutPDU( pdu, sizeof( pdu ) );
                        }

                    PBX.xmt_que.PutPDU( tau.getData (), tau.getDataLen () );
                    }
                }
            }
        }

    return 0;
    }

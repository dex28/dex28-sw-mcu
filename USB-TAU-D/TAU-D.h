#ifndef _TAU_D_H_INCLUDED
#define _TAU_D_H_INCLUDED

class TAU_D
{
    int mode; // Contains both status & mode

/*
    DTE <-> TAU frame format:

    +---+---+---+---+---+---+---+---+
    | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
    +---+---+---+---+---+---+---+---+
    | 0   0   0   1   0   1   0   1 |  FRM_FLG1  (0x15)
    +---+---+---+---+---+---+---+---+
    | 0   0   0   1   0   1   0   1 |  FRM_FLG2  (0x15)
    +---+---+---+---+---+---+---+---+
    | x   x   x   x   x   x   x   x |  FRM_BC    (4-51)
    +---+---+---+---+---+---+---+---+
    |      SN       |     TYPE      |  FRM_CTL
    +---+---+---+---+---+---+---+---+
    | 0   0   0   0   0   0   0   x |  FRM_ADDR  (0 or 1)
    +---+---+---+---+---+---+---+---+
    | x   x   x   x   x   x   x   x |  FRM_DATA  (optional)
    +---+---+---+---+---+---+---+---+
    | x   x   x   x   x   x   x   x |  FRM_CS
    +---+---+---+---+---+---+---+---+

    BC: byte count (including FRM_BC and FRM_CS, excluding FRM_FLG*)

    CS: checksum ( (sum - 1) mod 255, including BC, CTL, ADDR & DATA)

    SN: sequence number mod 16 (0..15)

    TYPE:

        Bit 3:    0 = Request, 1 = Response
        Bit 2..0: Frame type

        DATA          0 0 0 0    0x00
        DATA_ACK      1 0 0 0    0x08
        STATUS_REQ    0 0 0 1    0x01
        STATUS_RESP   1 0 0 1    0x09
        ID_REQ        0 0 1 0    0x02
        ID_RESP       1 0 1 0    0x0A
        COMMAND       0 0 1 1    0x03
        COMMAND_ACK   1 0 1 1    0x0B
        TEST_REQ      0 1 1 1    0x07
        TEST_REPORT   1 1 1 1    0x0F
*/

public:

    enum // TAU D status & modes
    {
        STATUS_PBX_CONNECTED = 0x80,
        STATUS_DTS_CONNECTED = 0x40,
        MODE_MASK            = 0x1C,
        MODE_PRGFNCREL       = 0x10,
        MODE_MULTIMEDIA      = 0x08,
        MODE_PC_CONTROL      = 0x04,
        MODE_TRANSPARENT     = 0x00,
        DATA_ACK_ENABLE      = 0x02,
        DTS_TO_DTE_ENABLE    = 0x01
        };
/*
    TAU D Modes:
    ===========

    PRGFNCREL mode: 100
    
        Signals from PBX are copied to DTE.
        Signals from DTS depend on DTS_TO_DTE_ENABLE bit.
        When the DTE has sent PRGFNCACT signal with key 0x0B to PBX,
        all PRGFNCREL2 signals from DTS are filtered towards exchange
        until a PRGFNCREL2 is sent from DTE.

    MULTIMEDA mode: 010

        Signals from PBX are copied to DTE.
        Signals from DTS depend on DTS_TO_DTE_ENABLE bit.
        Multimedia bit in signal EQUSTA towards PBX is set.
        Signals with FNC 0xA0-0xA6 are filtered towards DTS.

    PC CONTROL mode: 001

        Signals from DTS and PBX are sent to DTE regardless of
        DTS_TO_DTE_ENABLE bit.
        DTS->PBX and PBX->DTS signals are intercepted by the DTE
        and not forwared.

    TRANSPARENT mode: 000

        Signals from exchange are copied to DTE.
        Signals from DTS depend on DTS_TO_DTE_ENABLE bit.
*/

    enum // Address Field values
    {
        ADDR_PBX        = 0x00,
        ADDR_DTS        = 0x01
        };

private:

    enum // Frame Header Flags
    {
        FRM_FLG1            = 0x15,
        FRM_FLG2            = 0x15,
        };

    enum // Frame Control
    {
        // Bits 7..4: Sequence Number
        //
        FRM_SN_SHIFT        = 4,
        FRM_SN_MASK         = 0x0F,

        // Bits 3..0: Frame Type
        //
        FRM_CTL_MASK        = 0x0F,
        FRM_CTL_DATA        = 0x00,
        FRM_CTL_DATA_ACK    = 0x08,
        FRM_CTL_STATUS_REQ  = 0x01,
        FRM_CTL_STATUS_RESP = 0x09,
        FRM_CTL_ID_REQ      = 0x02,
        FRM_CTL_ID_RESP     = 0x0A,
        FRM_CTL_COMMAND     = 0x03,
        FRM_CTL_COMMAND_ACK = 0x0B,
        FRM_CTL_TEST_REQ    = 0x07,
        FRM_CTL_TEST_REPORT = 0x0F,
        };

    // RECEIVER (to/from DTE) -------------------------------------------------

    enum STATE // Receiver state-machine
    {
        WAIT_FLG1    = 0,
        WAIT_FLG2    = 1,
        WAIT_BC      = 2,
        WAIT_CTL     = 3,
        WAIT_ADDR    = 4,
        WAIT_DATA    = 5,
        WAIT_CS      = 6,
        WAIT_LF      = 7
        };

    STATE state;
    int CTL;   // Frame control
    int ADDR;  // Frame address
    int CS;    // Frame checksum

    int data_len;
    unsigned char data[ 64 ];
    int data_p;

    int sn_from_DTS;
    int sn_from_PBX;

    // SENDER (to/from DTE)
    //
    int sn_to_DTE;
    int sn_from_DTE;

public:

    TAU_D( void );

    const char* VerboseState( void )
    {
        static const char* verb[] = {
            "WAIT FLG1", "WAIT FLG2", "WAIT BC", "WAIT CTL", 
            "WAIT ADDR", "WAIT_DATA", "WAIT_CS"
            };
        return verb[ state ];
        }

    unsigned char* getData( void )
    {
        // Returns ptr to data after NBYTES
        return data + ( data[ 0 ] & 0x80 ? 2 : 1 );
        }

    int getDataLen( void ) const 
    {
        // Returns data without w/o NBYTES & checksum
        return data_len - ( data[ 0 ] & 0x80 ? 3 : 2 );
        }

    int getAddr( void ) const
    {
        return ADDR;
        }

    int getMode( void ) const
    {
        return ( mode & MODE_MASK ) >> 2;
        }

    void SetMode( int new_mode )
    {
        mode &= ~0x3F;
        mode |= ( new_mode & 0x3F );
        }

    void Reset( void )
    {
        SetMode( MODE_TRANSPARENT );
        }

    void SetStatus_PBX_Connected( bool flag )
    {
        if ( flag ) mode |= STATUS_PBX_CONNECTED;
        else mode &= ~STATUS_PBX_CONNECTED;
        }

    void SetStatus_DTS_Connected( bool flag )
    {
        if ( flag ) mode |= STATUS_DTS_CONNECTED;
        else mode &= ~STATUS_DTS_CONNECTED;
        }

    void AbortFrame( void )
    {
        if ( state == WAIT_FLG1 )
            return;

        state = WAIT_FLG1;
        }

    bool OpenSerial( const char* port_name );
    bool OpenTCP( int portno );
    bool AcceptClient( void );
    void CloseConnection( void );

    void SendFrame( int ctl, int addr = 0, unsigned char* buf = NULL, int len = 0 );
    bool OnReceivedOctet( int octet ); // returns true when received valid data frame

    void SendDataFrame( int addr, unsigned char* buf, int len )
    {
        // buf[] contains ELU 2B+D signal without NBYTES and CS
        // This procedure adds NBYTES and generates valid CS, and
        // then sends signal to DTE
        //

        if ( addr == 1 && mode != MODE_PC_CONTROL && ! ( mode & DTS_TO_DTE_ENABLE ) )
            return; // Signals from DTS to DTE are copied only on request

        unsigned char pkt[ 64 ];
        pkt[ 0 ] = len + 2; // NBYTES
        memcpy( pkt + 1, buf, len );

        // Add local SN to signal's OPC.
        //
        // OPC format: 
        //  +---+---+---+---+---+---+---+---+
        //  | 7 | 6 | 5 | 4 | 3 | 2 | 1 | 0 |
        //  +-----------+---+-----------+---+
        //  |    OPC    | P |    SN     |IND|
        //  +-----------+---+-----------+---+
        //
        pkt[ 1 ] &= ~0x1E; // Remove P and SN (bitmask 00011110)
        //
        if ( addr == 0 )
            pkt[ 1 ] |= ( ( ++sn_from_PBX & 0x07 ) << 1 );
        else
            pkt[ 1 ] |= ( ( ++sn_from_DTS & 0x07 ) << 1 );
    
        // Generate CS for ELU 2b+d signal
        //
        int cs = 0;
        for ( int i = 0; i < len + 1; i++ )
            cs += pkt[ i ];
        pkt[ 1 + len ] = ( cs - 1 ) & 0xFF;

        SendFrame( FRM_CTL_DATA, addr, pkt, len + 2 );
        }
    };

#endif // _TAU_D_H_INCLUDED


#include "ELU28.h"

///////////////////////////////////////////////////////////////////////////////

bool D_TransmitQueue::PutPDU( unsigned char* data, int len )
{
	if ( disabled )
	{
		SendHostFlowStatus ();
		return false;
		}

    // Check free space. If there is no one, drop PDU.
    //
    unsigned int used_space = writep >= readp 
        ? writep - readp
        : writep + sizeof( bufp ) - readp;

	if ( flowXON && used_space > 160 )
	{
		flowXON = false;
		SendHostFlowStatus ();
		}

	// Max needed space is approx len + 4 words
	//
	if ( used_space + len + 4 >= sizeof( bufp ) )
	{
		++dropped_counter;
		SendHostFlowStatus ();
		return false;
		}

    unsigned short NBYTES = len + 2; // additional size is for NBYTES + CKSUM
    
    // Set OPC's SeqNo
    //
    int OPC = ( data[ 0 ] & ~0x0E ) | ( curSeqNo << 1 );
    
	// Increment packet sequence number counter
	//        
    if ( ++curSeqNo >= 8 )
    	curSeqNo = 0;

    // Put PDU into output buffer, constantly calculating checksum.
    //
    int cksum = 0;

    // NBYTES first
    //
	if ( enhanced_protocol ) // NBYTES requires 2 octets
	{
        OPC |= 0x10; // Set P = 1 
        //
    	// NOTE: It seems that Ericsson DTS firmware *DOES NOT* set P = 1
    	// in enhanced mode; However, it send NBYTES as 2 octets.

		++NBYTES; // Increase signal length for the second octet of NBYTES
		NBYTES |= 0x8000; // Turn on "TWO-OCTET-NBYTES" flag

		unsigned short MSB = ( NBYTES >> 8 );
		cksum += MSB;
		*writep++ = MSB;
		if ( writep > maxp ) writep = bufp;

		unsigned short LSB = ( NBYTES & 0xFF );
		cksum += LSB;
		*writep++ = LSB;
		if ( writep > maxp ) writep = bufp;
		}
	else // NBYTES is one octet long
	{
		cksum += NBYTES;
		*writep++ = NBYTES; // Store signal length
		if ( writep > maxp ) writep = bufp;
		}

    // OPC
    //
	cksum += OPC;
	*writep++ = OPC;
	if ( writep > maxp ) writep = bufp;

    // data between OPC and checksum
    //
    for( int i = 1; i < len; i ++ )
    {
        cksum += data[ i ];
		*writep++ = data[ i ];
		if ( writep > maxp ) writep = bufp;
        }

    // cheksum
    //
	*writep++ = ( ( cksum - 1 ) & 0xFF );
	if ( writep > maxp ) writep = bufp;

    return true;
    }

///////////////////////////////////////////////////////////////////////////////

ELU28_D_Channel:: ELU28_D_Channel( int p_id )
    : id( p_id )
    , dasl( p_id )
    , rcv_buf ()
    , xmt_que( p_id )
{
    poll_counter = 0;

	fault_counter = 0;
	timeout_counter = 0;
	timeout_counter_nbytes = 0;
	timeout_counter_pdu = 0;
	timeout_counter_ack = 0;

    // packet_header[ 0 ] = id;
    packet_status = PACKET_EMPTY;
    packet_len = 0;

    oldRcvdSeqNo = 7;
    oldChecksum = 0;

    rcvd_octets = 0;
    cksum = 0;
    NBYTES = 0;
    
	Go_State( DISABLED, -1 );
	SetVerb_State( VERBOSE_DOWN );
    }

///////////////////////////////////////////////////////////////////////////////

void ELU28_D_Channel::Initialize( void )
{
    extern void USART0_Initialize( void );
    extern void USART1_Initialize( void );

    if ( id == 0 )
        USART0_Initialize ();
    else
        USART1_Initialize ();

	// bChannel->Initialize( this );
	
	if ( dasl.IsMaster () )
		dasl.PowerUp ();

	DASL_EH ();
    }

///////////////////////////////////////////////////////////////////////////////

#ifndef _ELU28_H_INCLUDED
#define _ELU28_H_INCLUDED

#include <stdint.h>
#include <stdio.h>
#include "Cadence.h"
#include "DASL.h"
#include "ELUFNC.h"

#include <string.h>
#include "TAU-D.h"

extern class ELU28_D_Channel PBX;
extern class ELU28_D_Channel DTS;
extern class TAU_D tau;

///////////////////////////////////////////////////////////////////////////////

class D_ReceiveBuffer
{
    volatile bool disabled;
    uint8_t* readp;
    uint8_t* writep;
    uint8_t* maxp;
    uint8_t bufp[ 32 ];

public:

    D_ReceiveBuffer( void )
    {
        disabled = true;
        readp = bufp;
        writep = bufp;
        maxp = bufp + sizeof( bufp ) - 1;
        }
        
    void Initialize( void )
    {
        readp = writep = bufp;
        disabled = false;
        }
        
    void Disable( void )
    {
        disabled = true;
        readp = writep = bufp;
        }

    inline int GetOctet( void )
    {
        if ( writep == readp )
            return -1;

        int ret = *readp++; // get value and advance tail pointer
        if ( readp > maxp )
            readp = bufp;
            
        return ret;
        }

    inline void PutOctet( int octet )
    {
        if ( disabled )
            return;

        // Advance writep to next location
        //
        uint8_t* chp = writep + 1;
        if ( chp > maxp ) 
            chp = bufp;

        // If next location goes over readp, then it is overflow
        if ( chp == readp )
            return;

        *writep = octet;
        writep = chp;
        }
    };
    
///////////////////////////////////////////////////////////////////////////////

class D_TransmitQueue
{
    volatile bool disabled;
    volatile int octetCount;

	int id;
    bool flowXON;
    
    int outOfBandOctet;
    
    int curSeqNo;
    int attempt_counter;
    bool enhanced_protocol;

    uint8_t* readp;
    uint8_t* writep;
    uint8_t* xmitp;
    uint8_t* maxp;
    uint8_t bufp[ 512 ];

public:

    unsigned short dropped_counter;
    
    D_TransmitQueue( int p_id )
    	: id( p_id )
    {
    	disabled = true;
    	flowXON = false;
    	octetCount = 0;
    	curSeqNo = 0;
    	outOfBandOctet = -1; // empty
    	attempt_counter = 0;
    	enhanced_protocol = false;
    	dropped_counter = 0;
        readp = writep = xmitp = bufp;
        maxp = bufp + sizeof( bufp ) - 1;
        }
        
    void Disable( void )
    {
        disabled = true;
    	flowXON = false;
        octetCount = 0; // To signal IDLE state
		// SendHostFlowStatus (); Loop Sync LOST overrides any XON
        }

	void SendHostFlowStatus( void )
	{    	
		unsigned char pdu [] = { 0, 0x00, 0x52, 0 };
		pdu[ 0 ] = id;
		pdu[ 3 ] = flowXON;
		// outBuf.PutPack( MSGTYPE_IO_D, pdu, sizeof( pdu ) );
		}
		
    void Initialize( void )
    {
    	octetCount = 0;
    	curSeqNo = 0;
    	outOfBandOctet = -1; // empty
    	attempt_counter = 0;
    	enhanced_protocol = false;
    	dropped_counter = 0;
        readp = writep = xmitp = bufp;
    	disabled = false;
    	flowXON = true;
    	
		// SendHostFlowStatus (); Loop Sync OK overrides any XOFF
        }

	void SetEnhancedProtocol( bool value = true )
	{
		enhanced_protocol = value;
		}

	bool IsEnhancedProtocol( void ) const
	{
		return enhanced_protocol;
		}

    bool IsQueueEmpty( void ) const
    {
        return readp == writep;
        }
        
    bool IsIdle( void ) const
    {
        return ! disabled && octetCount == 0;
        }
        
    void SendSignalInquiry( void )
    {
    	if ( ! IsIdle () )
    		return;

    	outOfBandOctet = 0x00;
    	}
    	
    void SendNegativePollAnswer( void )
    {
    	if ( ! IsIdle () )
    		return;

    	outOfBandOctet = enhanced_protocol ? 0x01 : 0x00;
    	}
    	
    void SendPositiveAck( void )
    {
    	if ( ! IsIdle () )
    		return;

    	outOfBandOctet = 0xAA;
    	}
    	
    void SendNegativeAck( void )
    {
    	if ( ! IsIdle () )
    		return;

    	outOfBandOctet = 0x75;
    	}

    bool PutPDU( unsigned char* data, int len );
    
    void StartTransmission( void )
    {   
    	if ( ! IsIdle () || IsQueueEmpty () )
    		return;
    		
    	xmitp = readp;
    	
    	int NBYTES = *xmitp++;
    	if ( xmitp > maxp )
    		xmitp = bufp;

        if ( NBYTES & 0x80 )
    	    NBYTES = ( ( NBYTES & ~0x80 ) << 8 ) | *xmitp;

        xmitp = readp;
        outOfBandOctet = -1;
    	octetCount = NBYTES; // Start transmission
    	}

    void ErasePDU( void )
    {
    	if ( ! IsIdle () || IsQueueEmpty () )
    		return;

		attempt_counter = 0;

		readp = xmitp; // xmitp points to first word after transmit frame

		if ( ! flowXON )
		{		
		    // Check used space. If it drops bellow "low water mark" and
		    // it flowXON was false (i.e. it was XOFF state), then
		    // signal to host transition to XON state.
		    //
		    int used_space = writep >= readp 
		        ? writep - readp
		        : writep + sizeof( bufp ) - readp;
	        
	    	if ( used_space < 64 )
	    	{
	    		flowXON = true;
				SendHostFlowStatus ();
	    		}
	    	}
    	}

    void RestartTransmission( void )
    {   
    	if ( ! IsIdle () || IsQueueEmpty () )
    		return;
    		
    	if ( attempt_counter < 2 )
    		++attempt_counter;
		else
			ErasePDU ();
    	}
    
    inline int GetOctet( void )
    {
        if ( disabled )
            return -1;

        int ch = -1;

        if ( octetCount > 0 )
        {
            // Transmit part of PDU
            //
            ch = *xmitp++;
            if ( xmitp > maxp )
                xmitp = bufp;

            octetCount--;
            }
        else
        {
            // Transmit out of band octet, if any
            //
            ch = outOfBandOctet; // -1 if empty
            outOfBandOctet = -1;
            }

        return ch;
        }
    };

///////////////////////////////////////////////////////////////////////////////

class ELU28_D_Channel
{
public:

	enum VERBOSE_STATE
	{
		VERBOSE_DOWN			= 0,
		VERBOSE_HALFUP			= 1,
		VERBOSE_UP				= 2,
		VERBOSE_RINGING			= 3,
		VERBOSE_TRANSMISSION	= 4,
		VERBOSE_FAULTY_DTS      = 5
		};
		
    enum PACKET_STATUS // Receiver status
    {
    	PACKET_EMPTY, PACKET_INCOMPLETE, PACKET_COMPLETED, PACKET_OVERFLOW 
    	};

private:

    enum STATE
    {
        DISABLED,          // DASL is in power down
        WAIT_LOOP_SYNC,    // DASL is in line-signal detected mode
        				   // Other: DASL is in sync
        WAIT_SIGNAL_INQUIRY,
        IDLE,
        WAIT_NBYTES,
        WAIT_FOR_ACK,
        WAIT_NBYTES_LOW,
        RECEIVING_PDU,
        TRANSMITTING_PDU
        };
        
    int id;
    
    STATE state;

    volatile int timer; // general 1ms timeout/countdown timer;
                        // when -1 disabled; when 0 triggers ON TIMEOUT event

    int poll_counter; // number of consecutive polls (00h's)

	int timeout_counter;
	int fault_counter;
	
	int timeout_counter_poll;

	unsigned short verb_state;
	int transmission_order;

    // Receiver State
    //
    int oldRcvdSeqNo;
    int oldChecksum;
    	
    int NBYTES;
    int rcvd_octets;
    int cksum;

private: // Methods

	///////////////////////////////////////////////////////////////////////////
	//
	void Go_State( STATE newState ) // without timeout
	{
		state = newState;
		}

	void Go_State( STATE newState, int timeout )
	{
		state = newState;
		timer = timeout;
		}

	///////////////////////////////////////////////////////////////////////////
	//
	void SetVerb_State( VERBOSE_STATE vs )
	{
		verb_state = vs;

        if ( trace )
        {
            extern volatile unsigned int SysTimer;
            printf( id == 0 ? "%06u PBX: " : "%06u DTS: ", SysTimer );

		    switch( vs )
		    {
			    case VERBOSE_DOWN: printf( "DOWN" ); break;
			    case VERBOSE_HALFUP: printf( "HALF UP" ); break;
                case VERBOSE_UP: printf( "UP" ); break;
			    case VERBOSE_TRANSMISSION: printf( "TRANSMISSION" );break;
			    case VERBOSE_RINGING: printf( "RINGING" ); break;
			    case VERBOSE_FAULTY_DTS: printf( "FAULTY DTS" ); break;
			    }

            printf( "\r\n" );
            }
		}
public:

	int timeout_counter_nbytes;
	int timeout_counter_pdu;
	int timeout_counter_ack;
	
    DASL dasl;

    D_ReceiveBuffer rcv_buf;
    D_TransmitQueue xmt_que;

   	PACKET_STATUS packet_status;
    unsigned char packet[ 310 ];
    unsigned int packet_len;

	///////////////////////////////////////////////////////////////////////////
	//
    ELU28_D_Channel( int p_id );
    void Initialize( void );

    void Master_RcvBuf_EH( void );
    void Slave_RcvBuf_EH( void );
    
    void Master_Timed_EH( void );
    void Slave_Timed_EH( void );

    void Master_DASL_EH( void );
    void Slave_DASL_EH( void );

	// void ParseInputPDU( unsigned char* data, int data_len );
	
	int GetVerbState( void ) const
	{
		return verb_state;
		}
		
	int GetFaultCounter( void ) const
	{    
		return fault_counter;
		}
		
	int GetTimeoutCounter( void ) const
	{    
		return timeout_counter;
		}

	int GetDroppedCounter( void ) const
	{
		return xmt_que.dropped_counter;
		}

    void DecTimeoutTimer( void )
    {
    	if ( timer > 0 )
        	timer--;
       	}

	bool RcvBuf_EH( void )
	{       	
        if ( dasl.IsMaster () )
     		Master_RcvBuf_EH ();
     	else
	        Slave_RcvBuf_EH ();

        bool rc = packet_status == PACKET_COMPLETED;
        packet_status = PACKET_EMPTY;
        return rc;
     	}

	void Timed_EH( void )
	{       	
        if ( dasl.IsMaster () )
     		Master_Timed_EH ();
     	else
	        Slave_Timed_EH ();
     	}

	void DASL_EH( void )
	{       	
        if ( dasl.IsMaster () )
     		Master_DASL_EH ();
     	else
	        Slave_DASL_EH ();
     	}

    void OnLoopInSync( void )
    {
		// bChannel->StopTransmission ();
		
		rcv_buf.Initialize ();
		xmt_que.Initialize ();
		xmt_que.SetEnhancedProtocol( false );

		oldRcvdSeqNo = 7;
		oldChecksum = 0;

		fault_counter = 0;
		timeout_counter = 0;
		timeout_counter_nbytes = 0;
		timeout_counter_pdu = 0;
		timeout_counter_ack = 0;

		packet_status = PACKET_EMPTY;
		packet_len = 0;        
		
		poll_counter = 0;
		transmission_order = 0;

        // Now, report SYNC OK to the host
        //        
        if ( trace )
            printf( "%s: Loop in Sync\r\n", id == 0 ? "PBX" : "DTS" );
		}

    void OnLoopOutOfSync( void )
    {
		// bChannel->StopTransmission ();

		xmt_que.Disable ();
		rcv_buf.Disable ();

		// Now, report LOST SYNC to the host
		//
        if ( trace )
            printf( "%s: Loop Out of Sync\r\n", id == 0 ? "PBX" : "DTS" );
        }
    };

#endif // _ELU28_H_INCLUDED

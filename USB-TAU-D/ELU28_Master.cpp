
#include "ELU28.h"

const int POLL_TIMEOUT = 12; // > 10ms
const int TIMEOUT_6ms = 7; // receive ACK
const int TIMEOUT_2ms = 3; // receive octet timeout

void ELU28_D_Channel::Master_RcvBuf_EH( void )
{
    int octet = rcv_buf.GetOctet ();
    if ( octet < 0 )
    	return;

    // Event: ON RECEIVED OCTET
    
    switch ( state )
    {
        case DISABLED:
		case WAIT_LOOP_SYNC:
            // ignore garbage
            break;

        case IDLE:
        	++fault_counter; // Received octet when not expected
        	break;

        case WAIT_NBYTES:
        	if ( timeout_counter > 0 )
        		--timeout_counter;
        	
            if ( octet == 0x00 || octet == 0x01 ) // Negative Poll Answer
            {
            	if ( fault_counter > 0 ) 
            		--fault_counter;

                xmt_que.SetEnhancedProtocol( octet == 0x01 );

        		Go_State( IDLE, POLL_TIMEOUT );
                }
            else if ( octet >= 4 && octet <= 127 )
            {
                NBYTES = octet;
                packet_len = 0;
                packet_status = PACKET_INCOMPLETE;
                rcvd_octets = 1;
                cksum = octet;
                
                Go_State( RECEIVING_PDU, TIMEOUT_2ms );
                }
            else if ( octet == 0x80 || octet == 0x81 )
            {
                NBYTES = ( octet - 0x80 ) << 8;
                packet_len = 0;
                packet_status = PACKET_INCOMPLETE;
                rcvd_octets = 1;
                cksum = octet;
                
                Go_State( WAIT_NBYTES_LOW, TIMEOUT_2ms );
                }
            else // Not acceptable octet
            {                 
            	++fault_counter;
            	
                // xmt_que.SendNegativeAck (); // MBK Patch

                Go_State( IDLE, POLL_TIMEOUT );
            	}
            break;

        case WAIT_NBYTES_LOW:
        	if ( timeout_counter > 0 )
        		--timeout_counter;

            NBYTES += octet;
            if ( NBYTES >= 5 && NBYTES < 305 )
            {
                cksum += octet;
            	rcvd_octets ++;
                
                Go_State( RECEIVING_PDU, TIMEOUT_2ms );
                }
            else
            {
            	++fault_counter;

                Go_State( IDLE, POLL_TIMEOUT );
                }
            break;

        case RECEIVING_PDU:
        
        	if ( timeout_counter > 0 )
        		--timeout_counter;
	    	
            if ( ++rcvd_octets < NBYTES )
            {
                if ( packet_len < sizeof( packet ) / sizeof( packet[0] ) )
                    packet[ packet_len++ ] = octet;
                else
                	packet_status = PACKET_OVERFLOW;
                cksum += octet;
                
                Go_State( RECEIVING_PDU, TIMEOUT_2ms );
                }
            else // Last byte i.e. checksum
            {
                cksum = ( cksum - 1 ) & 0xFF;
                if ( cksum != octet ) // checksum is NOT OK
                {                            
                	++fault_counter;

                    xmt_que.SendNegativeAck ();
                    
                	Go_State( IDLE, POLL_TIMEOUT );
                    }
                else
                {                   
                	if ( fault_counter > 0 )
                		--fault_counter;

                    xmt_que.SendPositiveAck ();
                    
                    int newSeqNo = ( packet[ 0 ] >> 1 ) & 0x7;
                    if ( newSeqNo == oldRcvdSeqNo 
                        && ( newSeqNo != 0 || cksum == oldChecksum )
                        )
                    {
                    	++fault_counter;

                        // Ignore incoming signal
                		Go_State( IDLE, POLL_TIMEOUT );
                        }
                    else
                    {
                        oldRcvdSeqNo = newSeqNo;
                        oldChecksum = cksum;

						if ( packet_status != PACKET_OVERFLOW )
							packet_status = PACKET_COMPLETED;
                        else
                            packet_status = PACKET_EMPTY; // remove packet

		            	Go_State( IDLE, xmt_que.IsEnhancedProtocol () ? 2 : POLL_TIMEOUT );

                        if ( packet_status == PACKET_COMPLETED )
                        {
                            if ( packet[ 1 ] == FNC_EQUSTA ) // EQUSTA received
                            {
                                // Change verbose state to UP, if not in a call
                                //
                                if ( verb_state != VERBOSE_TRANSMISSION
                                &&   verb_state != VERBOSE_RINGING )
                                {
					                SetVerb_State( VERBOSE_UP );
					                }
				                }
			                else if ( packet[ 1 ] == FNC_EQUTESTRES && packet[ 2 ] == 0x7F )
			                {
				                // EQUTESTRES: Code = 0x7F, Local Error Status report
				                //
				                if (  packet[ 3 ] != 0x00 )
				                {
					                SetVerb_State( VERBOSE_FAULTY_DTS );
					                }
				                }
                            }
                        }
                    }
                }
            break;
            
        case WAIT_FOR_ACK:
        	if ( timeout_counter > 0 )
        		--timeout_counter;
        		
            if ( octet == 0xAA )
            {   
            	if ( fault_counter > 0 )
            		--fault_counter;

                xmt_que.ErasePDU ();

                Go_State( IDLE, xmt_que.IsEnhancedProtocol () ? 2 : POLL_TIMEOUT );
                }
            else // octet == 0x75, or other
            {
            	++fault_counter;

            	xmt_que.RestartTransmission ();

                Go_State( IDLE, POLL_TIMEOUT );
                }
            break;

        case TRANSMITTING_PDU:
            // Protocol is full-duplex.
            // We should not receive anything while transmitting.
            //
            ++fault_counter;
            break;

        case WAIT_SIGNAL_INQUIRY:
            Freeze_CPU ();
            break;
        }
    }

void ELU28_D_Channel::Master_Timed_EH( void )
{
	if ( state == TRANSMITTING_PDU && xmt_que.IsIdle () )
	{
		// Event: On Transmission Completed
		
		Go_State( WAIT_FOR_ACK, TIMEOUT_6ms ); // Wait ACK or NBYTES
		}

    if ( timer == 0 )
    {
        // Event: ON TIMEOUT

        timer = -1; // acknowledge event i.e. disable timer

        switch( state )
        {
            case DISABLED:
				dasl.PowerUp ();
				Go_State( WAIT_LOOP_SYNC, 2000 );
		        SetVerb_State( VERBOSE_DOWN );
                break;

			case WAIT_LOOP_SYNC:
				dasl.PowerDown ();
				Go_State( DISABLED, 500 );
		        SetVerb_State( VERBOSE_DOWN );
				break;

            case IDLE:
                if ( xmt_que.IsQueueEmpty () )
                {
                	xmt_que.SendSignalInquiry ();

            		Go_State( WAIT_NBYTES, TIMEOUT_6ms ); // Wait ACK or NBYTES
                	}
                else
                {
	                xmt_que.StartTransmission (); // Send Signal
	                
	                Go_State( TRANSMITTING_PDU, 1000 ); // Transmission rate is ~1.6kBy/s
                    }
                break;

			case WAIT_NBYTES:
            	++timeout_counter;
            	++timeout_counter_nbytes;

            	Go_State( IDLE, POLL_TIMEOUT );
            	break;
            	
            case WAIT_NBYTES_LOW:
            	++timeout_counter;
            	++timeout_counter_pdu;

                Go_State( IDLE, POLL_TIMEOUT );
            	break;

            case RECEIVING_PDU:
            	++timeout_counter;
            	++timeout_counter_pdu;
            	
                Go_State( IDLE, POLL_TIMEOUT );
                break;

            case WAIT_FOR_ACK:
            	++timeout_counter;
            	++timeout_counter_ack;

            	xmt_que.RestartTransmission ();

                Go_State( IDLE, POLL_TIMEOUT );
                break;
                
            case TRANSMITTING_PDU:
            	// Transmitter takes to long time to transmit signal
            	Freeze_CPU (); // severe error
            	break;

            case WAIT_SIGNAL_INQUIRY:
                Freeze_CPU ();
                break;
            };
        }
    }

void ELU28_D_Channel::Master_DASL_EH( void )
{
    dasl.Update ();
    	
    switch( state )
    {
        case DISABLED:
        {
        	if ( dasl.IsLineSignalDetected () )
        	{
        		dasl.PowerUp ();
        		Go_State( WAIT_LOOP_SYNC, 200 );
		        SetVerb_State( VERBOSE_DOWN );
        		}
            }
            break;

        case WAIT_LOOP_SYNC:
        	if ( ! dasl.IsLineSignalDetected () )
        	{
        		dasl.PowerDown ();
        		Go_State( DISABLED, 500 );
		        SetVerb_State( VERBOSE_DOWN );
        		}
        	else  if ( dasl.IsLoopInSync () ) // ON LOOP IN SYNC
        	{
                tau.SetStatus_DTS_Connected( true );

                OnLoopInSync ();

		        Go_State( IDLE, 200 ); // give 200ms to DTS before starting signal inquiry
		        SetVerb_State( VERBOSE_HALFUP );
        		}
            break;

        default:
        	if ( ! dasl.IsLoopInSync () ) // ON LOOP OUT OF SYNC
        	{
                tau.SetStatus_DTS_Connected( false );
                PBX.OnLoopOutOfSync ();
                PBX.Go_State( DISABLED, -1 );

                OnLoopOutOfSync ();

				if ( dasl.IsLineSignalDetected () )
				{
            		Go_State( WAIT_LOOP_SYNC, 200 );
					SetVerb_State( VERBOSE_DOWN );
					}
				else
				{
            		dasl.PowerDown ();
            		Go_State( DISABLED, 500 );
					SetVerb_State( VERBOSE_DOWN );
					}
        		}
        	break;
        }
    }

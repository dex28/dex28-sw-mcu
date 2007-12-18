#ifndef _DASL_H_INCLUDED
#define _DASL_H_INCLUDED

extern bool trace;
extern void Freeze_CPU( void );

///////////////////////////////////////////////////////////////////////////////
// DASL Class: TP3406 Control Interface
//
class DASL
{
    int id;
    int control;
    int status;
    
    // Mask 0x80, Bit C7: 0 = Master,      1 = Slave
    // Mask 0x40, Bit C6: 0 = Power Down,  1 = Power Up
    // Mask 0x20, Bit S1: 0 = Out-Of-Sync, 1 = Loop In-Sync
    
    // Default after applied Vcc: Master, Power Down

public:

	unsigned event_count;

	enum TDM_Sync
	{
		SLAVE = 0,
		MASTER = 1
		};
		
    DASL( int p_id )
    {
        id = p_id;
        control = 0;
        status = 0;
        event_count = 0;
        }

	void Initialize( TDM_Sync mode )
    {
		if ( mode == MASTER )
			control = 0x00; // Power Down, Master
		else
			control = 0x80; // Power Down, Slave

		Update ();
	    }

    void Update( void ); // Synchronize control & status bits
    
	static bool IsStatusChanged( void ); // Indicates need for Update()

	bool IsMaster( void ) const
	{
		return ( control & 0x80 ) == 0; // bit C7 is 0
		}
		
	bool IsSlave( void ) const
	{
		return ( control & 0x80 ) != 0; // bit C7 is 1
		}
		
	bool IsPowerUp( void ) const
	{
		return ( control & 0x40 ) != 0; // bit C6 is 1
		}
		
    bool IsLoopInSync( void ) const
    {
        return ( status & 0x02 ) != 0;  // bit S1 is 1
        }

    bool IsLineSignalDetected( void ) const
    {
        return ( status & 0x01 ) != 0;  // bit S0 is 1
        }

    int GetStatus( void ) const
    {
        return status;
        }

	void PowerUp( void )
	{
		control |= 0x40; // Power Up
		Update ();
		}

	void PowerDown( void )
	{
		control &= ~0x40; // Power Down
		Update ();
		}
	};

#endif // _ELU28_H_INCLUDED

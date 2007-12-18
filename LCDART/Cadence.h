#ifndef _CADENCE_H_INCLUDED
#define _CADENCE_H_INCLUDED

class CADENCE
{
	int N; // 0: disabled, -1: endless cadence, > 0 countdown cadences
	unsigned int state; // cadence interval 0..3; even means ON, odd means OFF
	unsigned int T[ 4 ]; // interval duration (in no of calls of Increment())
	unsigned int count; // interval countdown
/*
	Note:
	
	One cadence looks like:
	
    .-------.        .-------.        
    |       |        |       |        
    '       '--------'       '--------
	   T[0]    T[1]     T[2]    T[3]
    <-------------------------------->  == Cadence = sequence of intervals
    
    N is number of cadences:
    	N == -1 means endless
    	N == 0 means disabled
    	
    If some T[] is 0, then cadence stops there.
    
    E.g.
    	T[0] = 10, T[1] = 0, N = 1
    	means one shot of 10ms ON
*/	

public:

	void TurnOff( void )
	{
		state = 1; // freeze in state 1
		count = 0; // disabled interval counter
		N = 0; // disable cadence counter
		}

	void TurnOn( int duration = -1 );

	void SetCadence( int n, int t1, int t2, int t3 = 0, int t4 = 0 );

	bool Increment( void );

	bool IsON( void ) const
	{
		return ! ( state & 0x01 );
		}

    int GetStateSeq( void ) const
    {
        return state;
        }
	};

#endif // _CADENCE_H_INCLUDED

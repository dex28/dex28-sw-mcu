
#include "Cadence.h"

void Cadence:: TurnOn( int duration ) 
{
	if ( duration < 0 )
	{
		state = 0; // freeze in state 0
		count = 0; // disabled interval counter
		N = 0; // disable cadence counter
		}
	else
	{
		T[ 0 ] = duration; T[ 1 ] = 0;
		state = 0; count = T[ 0 ];
		N = 1; // one cadence
		}
	}

void Cadence:: SetCadence( int n, int t1, int t2, int t3, int t4 )
{
	T[ 0 ] = t1; T[ 1 ] = t2;
	T[ 2 ] = t3; T[ 3 ] = t4;
 	state = 0; // initial state is ON
	count = T[ 0 ]; // interval duration countdown
	N = n; // cadence counter
	}

bool Cadence:: Increment( void )
{
	if ( N == 0 || count == 0 )
		return true;
		
	if ( --count > 0 )
		return true;
		
	state++;
	
	if ( state < 4 && T[ state ] > 0 )
	{
		count = T[ state ]; // go to next interval in cadence
		return true;
		}

	state = 0; // start new caddence

	if ( N == -1 ) // endless cadence
		count = T[ state  ];
	else if ( --N > 0 )
		count = T[ state ]; // next period
	else
	{
		state = 1; // OFF
		return false;
		}
		
	return true;
	}


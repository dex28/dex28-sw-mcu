#ifndef _KEYBOARD_H_INCLUDED
#define _KEYBOARD_H_INCLUDED

#include <inttypes.h>
#include <avr/io.h>
#include <compat/ina90.h>

class KEY
{
	unsigned int last      :1;
	unsigned int current   :1;
	unsigned int state     :1;
	unsigned int pressed   :1;
	unsigned int depressed :1;
	unsigned int count     :12;

	enum { DEBOUNCE_DELAY = 5 }; // in Timer0 periods
	
public:	

	void Initialize( void )
	{
		last = current = state = count = 0; 
		}
	
    void Input( int x );

	bool IsPressed( void )
	{
		int ret = pressed != 0;
		pressed = 0;
		return ret;
		}
		
	bool IsDepressed( void )
	{
		int ret = depressed != 0;
		depressed = 0;
		return ret;
		}
	};

class KEYBOARD
{
    int scanRow;
    KEY keys[ 6 ];

public:

    void Initialize( void );

    void Scan( void );

    bool IsPressed( int i )
    {
        if ( i < 0 || i > 6 )
            return false;

        return keys[ i ].IsPressed ();
        }

    bool IsDepressed( int i )
    {
        if ( i < 0 || i > 6 )
            return false;

        return keys[ i ].IsDepressed ();
        }
    };

extern KEYBOARD kbd;

#endif // _KEYBOARD_H_INCLUDED

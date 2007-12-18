
#include <inttypes.h>
#include <avr/io.h>
#include <avr/interrupt.h>
#include <avr/signal.h>
#include <avr/wdt.h>
#include <avr/sleep.h>
#include <avr/eeprom.h>

#include <compat/ina90.h>

#define F_CPU 8000000UL // 8 MHz
#include <avr/delay.h>

/*
    MCU:    ATMega128

    Fuses:  FF 9F A4
            - ATmega103 compatiblity mode is OFF
            - Boot Flash size=512
            - Brown-out Detection Level 2.7V
            - Brown-out Detection Enabled
            - Int RC. Osc 8MHz; Start 6 CK + 64ms

    MCU Pin Usage:
    --------------

    JTAG:

      PD0        OUT      TCK
      PD1        IN       TDO
      PD2        OUT      TDI
      PD3        OUT      TMS

    Status LED:

      PF0        OUT      1 = Green
      PF1        OUT      2 = Red
      PF2        OUT      3 = Yellow
      PF3        OUT      4 = Yellow

    Keyboard:

      PA0        IN       Start/Stop
*/

///////////////////////////////////////////////////////////////////////////////
// Stimulus to the JTAG ports

inline void SetPort_TCK( int val )
{
    if ( val ) PORTD |= _BV(PD0);
    else PORTD &= ~_BV(PD0);
}

inline void SetPort_TDI( int val )
{
    if ( val ) PORTD |= _BV(PD2);
    else PORTD &= ~_BV(PD2);
}

inline void SetPort_TMS( int val )
{
    if ( val ) PORTD |= _BV(PD3);
    else PORTD &= ~_BV(PD3);
}

inline int GetPort_TDO( void )
{
    return ( PIND >> PIND1 ) & 1;
}

///////////////////////////////////////////////////////////////////////////////

enum
{
    LED_ALL      = _BV(PF0) | _BV(PF1) | _BV(PF2) | _BV(PF3),
    LED_GREEN    = _BV(PF0),
    LED_RED      = _BV(PF1),
    LED_YELLOW_1 = _BV(PF2),
    LED_YELLOW_2 = _BV(PF3)
};

inline void SetLED_On( int mask )
{
    PORTF &= ~mask;
}

inline void SetLED_Off( int mask )
{
    PORTF |= mask;
}

inline void SetLED_Toggle( int mask )
{
    PORTF ^= mask;
}

///////////////////////////////////////////////////////////////////////////////
// XSVF data access
//
// Note 1: xsvf section is defined to be in program space (see script.ld).
// Note 2: avr-gcc's pointers are 16-bits wide.
// Note 3: Following macros should be used to access program space addresses
//         as pointers 24-bits wide. 
//
#ifndef __STRINGIFY
#define __STRINGIFY(str) #str
#endif

#define GET_FAR_ADDRESS(var)                                    \
({                                                              \
    uint32_t tmp;                                               \
                                                                \
    __asm__ __volatile__(                                       \
            "ldi    %C0, hh8(" __STRINGIFY(var) ")"   "\n\t"    \
            "clr    %D0"                              "\n\t"    \
        :                                                       \
            "=d" (tmp)                                          \
        :                                                       \
            "0" (&var)                                          \
    );                                                          \
    tmp;                                                        \
})

static uint32_t read_addr; // address of the XSVF data in program memory

inline int readByte()
{ 
    return __ELPM( read_addr++ );
}

inline void waitTime( long microsec )
{ 
#if 1
    // execution time: 2 + n * 9 CPU cycles
    // i.e. 0.25us + n * 1.11us @ 8MHz CPU clock
    //
    do  __asm__ volatile ( "; delay loop\n");
        while( --microsec > 0 );
    }
#else
    // For systems with TCK rates << 1 MHz;  Consider this implementation.
    //
    if ( microsec >= 50L )
    {
        // Make sure TCK is low during wait for XC18V00/XCF00
        // Or, a running TCK implementation as shown above is an OK alternate
        SetPort_TCK( 0 );
        _delay_ms( microsec / 1000.0 );
    }
    else // Satisfy Virtex-II TCK cycles
    {
        for ( i = 0; i < microsec;  ++i )
        {
            pulseClock();
        }
    }
#endif

///////////////////////////////////////////////////////////////////////////////

enum // Error codes for xsvfRun
{
    XSVF_ERROR_NONE         = 0,
    XSVF_ERROR_UNKNOWN      = 1,
    XSVF_ERROR_TDOMISMATCH  = 2,
    XSVF_ERROR_MAXRETRIES   = 3,  // TDO mismatch after max retries
    XSVF_ERROR_ILLEGALCMD   = 4,
    XSVF_ERROR_ILLEGALSTATE = 5,
    XSVF_ERROR_DATAOVERFLOW = 6   // Data > lenVal MAX_LEN buffer size
};

/*****************************************************************************
   LENVAL class
*****************************************************************************/

/* the lenVal structure is a byte oriented type used to store an */
/* arbitrary length binary value. As an example, the hex value   */
/* 0x0e3d is represented as a lenVal with len=2 (since 2 bytes   */
/* and val[0]=0e and val[1]=3d.  val[2-MAX_LEN] are undefined    */

/* maximum length (in bytes) of value to read in        */
/* this needs to be at least 4, and longer than the     */
/* length of the longest SDR instruction.  If there is, */
/* only 1 device in the chain, MAX_LEN must be at least */
/* ceil(27/8) == 4.  For 6 devices in a chain, MAX_LEN  */
/* must be 5, for 14 devices MAX_LEN must be 6, for 20  */
/* devices MAX_LEN must be 7, etc..                     */
/* You can safely set MAX_LEN to a smaller number if you*/
/* know how many devices will be in your chain.         */
/*  #define MAX_LEN (Actual #define is below this comment block)
      This #define defines the maximum length (in bytes) of predefined
      buffers in which the XSVF player stores the current shift data.
      This length must be greater than the longest shift length (in bytes)
      in the XSVF files that will be processed.  7000 is a very conservative
      number.  The buffers are stored on the stack and if you have limited
      stack space, you may decrease the MAX_LEN value.

      How to find the "shift length" in bits?
      Look at the ASCII version of the XSVF (generated with the -a option
      for the SVF2XSVF translator) and search for the XSDRSIZE command
      with the biggest parameter.  XSDRSIZE is equivalent to the SVF's
      SDR length plus the lengths of applicable HDR and TDR commands.
      Remember that the MAX_LEN is defined in bytes.  Therefore, the
      minimum MAX_LEN = ceil( max( XSDRSIZE ) / 8 );

      The following MAX_LEN values have been tested and provide relatively
      good margin for the corresponding devices:

        DEVICE       MAX_LEN   Resulting Shift Length Max (in bits)
        ---------    -------   ----------------------------------------------
        XC9500/XL/XV 32        256

        CoolRunner/II 256      2048   - actual max 1 device = 1035 bits

        FPGA         128       1024    - svf2xsvf -rlen 1024

        XC18V00/XCF00
                     1100      8800   - no blank check performed (default)
                                      - actual max 1 device = 8192 bits verify
                                      - max 1 device = 4096 bits program-only

        XC18V00/XCF00 when using the optional Blank Check operation
                     2500      20000  - required for blank check
                                      - blank check max 1 device = 16384 bits
*/
#define MAX_LEN 32

typedef struct var_len_byte
{
    short len;   /* number of chars in this value */
    unsigned char val[MAX_LEN+1];  /* bytes of data */
} lenVal;

/* return the long representation of a lenVal */
extern long value(lenVal *x);

/* set lenVal equal to value */
extern void initLenVal(lenVal *x, long value);

/* check if expected equals actual (taking the mask into account) */
extern short EqualLenVal(lenVal *expected, lenVal *actual, lenVal *mask);

/* add val1+val2 and put the result in resVal */
extern void addVal(lenVal *resVal, lenVal *val1, lenVal *val2);

/* return the (byte, bit) of lv (reading from left to right) */
extern short RetBit(lenVal *lv, int byte, int bit);

/* set the (byte, bit) of lv equal to val (e.g. SetBit("00000000",byte, 1)
   equals "01000000" */
extern void SetBit(lenVal *lv, int byte, int bit, short val);

/* read from XSVF numBytes bytes of data into x */
extern void  readVal(lenVal *x, short numBytes);

/*****************************************************************************
* Function:     value
* Description:  Extract the long value from the lenval array.
* Parameters:   plvValue    - ptr to lenval.
* Returns:      long        - the extracted value.
*****************************************************************************/
long value( lenVal*     plvValue )
{
	long    lValue;         /* result to hold the accumulated result */
	short   sIndex;

    lValue  = 0;
	for ( sIndex = 0; sIndex < plvValue->len ; ++sIndex )
	{
		lValue <<= 8;                       /* shift the accumulated result */
		lValue |= plvValue->val[ sIndex];   /* get the last byte first */
	}

	return( lValue );
}

/*****************************************************************************
* Function:     initLenVal
* Description:  Initialize the lenval array with the given value.
*               Assumes lValue is less than 256.
* Parameters:   plv         - ptr to lenval.
*               lValue      - the value to set.
* Returns:      void.
*****************************************************************************/
void initLenVal( lenVal*    plv,
                 long       lValue )
{
	plv->len    = 1;
	plv->val[0] = (unsigned char)lValue;
}

/*****************************************************************************
* Function:     EqualLenVal
* Description:  Compare two lenval arrays with an optional mask.
* Parameters:   plvTdoExpected  - ptr to lenval #1.
*               plvTdoCaptured  - ptr to lenval #2.
*               plvTdoMask      - optional ptr to mask (=0 if no mask).
* Returns:      short   - 0 = mismatch; 1 = equal.
*****************************************************************************/
short EqualLenVal( lenVal*  plvTdoExpected,
                   lenVal*  plvTdoCaptured,
                   lenVal*  plvTdoMask )
{
    short           sEqual;
	short           sIndex;
    unsigned char   ucByteVal1;
    unsigned char   ucByteVal2;
    unsigned char   ucByteMask;

    sEqual  = 1;
    sIndex  = plvTdoExpected->len;

    while ( sEqual && sIndex-- )
    {
        ucByteVal1  = plvTdoExpected->val[ sIndex ];
        ucByteVal2  = plvTdoCaptured->val[ sIndex ];
        if ( plvTdoMask )
        {
            ucByteMask  = plvTdoMask->val[ sIndex ];
            ucByteVal1  &= ucByteMask;
            ucByteVal2  &= ucByteMask;
        }
        if ( ucByteVal1 != ucByteVal2 )
        {
            sEqual  = 0;
        }
    }

	return( sEqual );
}


/*****************************************************************************
* Function:     RetBit
* Description:  return the (byte, bit) of lv (reading from left to right).
* Parameters:   plv     - ptr to lenval.
*               iByte   - the byte to get the bit from.
*               iBit    - the bit number (0=msb)
* Returns:      short   - the bit value.
*****************************************************************************/
short RetBit( lenVal*   plv,
              int       iByte,
              int       iBit )
{
    /* assert( ( iByte >= 0 ) && ( iByte < plv->len ) ); */
    /* assert( ( iBit >= 0 ) && ( iBit < 8 ) ); */
    return( (short)( ( plv->val[ iByte ] >> ( 7 - iBit ) ) & 0x1 ) );
}

/*****************************************************************************
* Function:     SetBit
* Description:  set the (byte, bit) of lv equal to val
* Example:      SetBit("00000000",byte, 1) equals "01000000".
* Parameters:   plv     - ptr to lenval.
*               iByte   - the byte to get the bit from.
*               iBit    - the bit number (0=msb).
*               sVal    - the bit value to set.
* Returns:      void.
*****************************************************************************/
void SetBit( lenVal*    plv,
             int        iByte,
             int        iBit,
             short      sVal )
{
    unsigned char   ucByteVal;
    unsigned char   ucBitMask;

    ucBitMask   = (unsigned char)(1 << ( 7 - iBit ));
    ucByteVal   = (unsigned char)(plv->val[ iByte ] & (~ucBitMask));

    if ( sVal )
    {
        ucByteVal   |= ucBitMask;
    }
    plv->val[ iByte ]   = ucByteVal;
}

/*****************************************************************************
* Function:     AddVal
* Description:  add val1 to val2 and store in resVal;
*               assumes val1 and val2  are of equal length.
* Parameters:   plvResVal   - ptr to result.
*               plvVal1     - ptr of addendum.
*               plvVal2     - ptr of addendum.
* Returns:      void.
*****************************************************************************/
void addVal( lenVal*    plvResVal,
             lenVal*    plvVal1,
             lenVal*    plvVal2 )
{
	unsigned char   ucCarry;
    unsigned short  usSum;
    unsigned short  usVal1;
    unsigned short  usVal2;
	short           sIndex;
	
	plvResVal->len  = plvVal1->len;         /* set up length of result */
	
	/* start at least significant bit and add bytes    */
    ucCarry = 0;
    sIndex  = plvVal1->len;
    while ( sIndex-- )
    {
		usVal1  = plvVal1->val[ sIndex ];   /* i'th byte of val1 */
		usVal2  = plvVal2->val[ sIndex ];   /* i'th byte of val2 */
		
		/* add the two bytes plus carry from previous addition */
		usSum   = (unsigned short)( usVal1 + usVal2 + ucCarry );
		
		/* set up carry for next byte */
		ucCarry = (unsigned char)( ( usSum > 255 ) ? 1 : 0 );
		
        /* set the i'th byte of the result */
		plvResVal->val[ sIndex ]    = (unsigned char)usSum;
    }
}

/*****************************************************************************
* Function:     readVal
* Description:  read from XSVF numBytes bytes of data into x.
* Parameters:   plv         - ptr to lenval in which to put the bytes read.
*               sNumBytes   - the number of bytes to read.
* Returns:      void.
*****************************************************************************/
void readVal( lenVal*   plv,
              short     sNumBytes )
{
    unsigned char*  pucVal;
	
    plv->len    = sNumBytes;        /* set the length of the lenVal        */
    for ( pucVal = plv->val; sNumBytes; --sNumBytes, ++pucVal )
    {
        /* read a byte of data into the lenVal */
		*pucVal = readByte ();
    }
}

/*****************************************************************************
* History:      v2.00   - Original XSVF implementation.
*               v4.04   - Added delay at end of XSIR for XC18v00 support.
*                         Added new commands for CoolRunner support:
*                         XSTATE, XENDIR, XENDDR
*               v4.05   - Cleanup micro.c but leave ports.c intact.
*               v4.06   - Fix xsvfGotoTapState for retry transition.
*               v4.07   - Update example waitTime implementations for
*                         compatibility with Virtex-II.
*               v4.10   - Add new XSIR2 command that supports a 2-byte
*                         IR-length parameter for IR shifts > 255 bits.
*               v4.11   - No change.  Update version to match SVF2XSVF xlator.
*               v4.14   - Added XCOMMENT.
*               v5.00   - Improve XSTATE support.
*                         Added XWAIT.
*               v5.01   - make sure that TCK is low during RUNTEST wait for
*                         XC18V00/XCF00 support.  Only change is in PORTS.C
*                         waitTime() function for implementations that do NOT
*                         pulse TCK during the waitTime.
*****************************************************************************/

/*============================================================================
* XSVF #define
============================================================================*/

#define XSVF_VERSION    "5.01"

/*============================================================================
* XSVF Type Declarations
============================================================================*/

/*****************************************************************************
* Struct:       SXsvfInfo
* Description:  This structure contains all of the data used during the
*               execution of the XSVF.  Some data is persistent, predefined
*               information (e.g. lRunTestTime).  The bulk of this struct's
*               size is due to the lenVal structs (defined in lenval.h)
*               which contain buffers for the active shift data.  The MAX_LEN
*               #define in lenval.h defines the size of these buffers.
*               These buffers must be large enough to store the longest
*               shift data in your XSVF file.  For example:
*                   MAX_LEN >= ( longest_shift_data_in_bits / 8 )
*               Because the lenVal struct dominates the space usage of this
*               struct, the rough size of this struct is:
*                   sizeof( SXsvfInfo ) ~= MAX_LEN * 7 (number of lenVals)
*               xsvfInitialize() contains initialization code for the data
*               in this struct.
*               xsvfInfoCleanup() contains cleanup code for the data in this
*               struct.
*****************************************************************************/
struct SXsvfInfo
{
    /* XSVF status information */
    unsigned char   ucComplete;         /* 0 = running; 1 = complete */
    unsigned char   ucCommand;          /* Current XSVF command byte */
    long            lCommandCount;      /* Number of commands processed */
    int             iErrorCode;         /* An error code. 0 = no error. */

    /* TAP state/sequencing information */
    unsigned char   ucTapState;         /* Current TAP state */
    unsigned char   ucEndIR;            /* ENDIR TAP state (See SVF) */
    unsigned char   ucEndDR;            /* ENDDR TAP state (See SVF) */

    /* RUNTEST information */
    unsigned char   ucMaxRepeat;        /* Max repeat loops (for xc9500/xl) */
    long            lRunTestTime;       /* Pre-specified RUNTEST time (usec) */

    /* Shift Data Info and Buffers */
    long            lShiftLengthBits;   /* Len. current shift data in bits */
    short           sShiftLengthBytes;  /* Len. current shift data in bytes */

    lenVal          lvTdi;              /* Current TDI shift data */
    lenVal          lvTdoExpected;      /* Expected TDO shift data */
    lenVal          lvTdoCaptured;      /* Captured TDO shift data */
    lenVal          lvTdoMask;          /* TDO mask: 0=dontcare; 1=compare */

    /* XSDRINC Data Buffers */
    lenVal          lvAddressMask;      /* Address mask for XSDRINC */
    lenVal          lvDataMask;         /* Data mask for XSDRINC */
    lenVal          lvNextData;         /* Next data for XSDRINC */
};

/* Declare pointer to functions that perform XSVF commands */
typedef int (*TXsvfDoCmdFuncPtr)( SXsvfInfo* );


/*============================================================================
* XSVF Command Bytes
============================================================================*/

/* encodings of xsvf instructions */
#define XCOMPLETE        0
#define XTDOMASK         1
#define XSIR             2
#define XSDR             3
#define XRUNTEST         4
/* Reserved              5 */
/* Reserved              6 */
#define XREPEAT          7
#define XSDRSIZE         8
#define XSDRTDO          9
#define XSETSDRMASKS     10
#define XSDRINC          11
#define XSDRB            12
#define XSDRC            13
#define XSDRE            14
#define XSDRTDOB         15
#define XSDRTDOC         16
#define XSDRTDOE         17
#define XSTATE           18         /* 4.00 */
#define XENDIR           19         /* 4.04 */
#define XENDDR           20         /* 4.04 */
#define XSIR2            21         /* 4.10 */
#define XCOMMENT         22         /* 4.14 */
#define XWAIT            23         /* 5.00 */
/* Insert new commands here */
/* and add corresponding xsvfDoCmd function to xsvf_pfDoCmd below. */
#define XLASTCMD         24         /* Last command marker */


/*============================================================================
* XSVF Command Parameter Values
============================================================================*/

#define XSTATE_RESET     0          /* 4.00 parameter for XSTATE */
#define XSTATE_RUNTEST   1          /* 4.00 parameter for XSTATE */

#define XENDXR_RUNTEST   0          /* 4.04 parameter for XENDIR/DR */
#define XENDXR_PAUSE     1          /* 4.04 parameter for XENDIR/DR */

/* TAP states */
#define XTAPSTATE_RESET     0x00
#define XTAPSTATE_RUNTEST   0x01    /* a.k.a. IDLE */
#define XTAPSTATE_SELECTDR  0x02
#define XTAPSTATE_CAPTUREDR 0x03
#define XTAPSTATE_SHIFTDR   0x04
#define XTAPSTATE_EXIT1DR   0x05
#define XTAPSTATE_PAUSEDR   0x06
#define XTAPSTATE_EXIT2DR   0x07
#define XTAPSTATE_UPDATEDR  0x08
#define XTAPSTATE_IRSTATES  0x09    /* All IR states begin here */
#define XTAPSTATE_SELECTIR  0x09
#define XTAPSTATE_CAPTUREIR 0x0A
#define XTAPSTATE_SHIFTIR   0x0B
#define XTAPSTATE_EXIT1IR   0x0C
#define XTAPSTATE_PAUSEIR   0x0D
#define XTAPSTATE_EXIT2IR   0x0E
#define XTAPSTATE_UPDATEIR  0x0F

/*============================================================================
* XSVF Function Prototypes
============================================================================*/

int xsvfDoIllegalCmd( SXsvfInfo* pXsvfInfo );   /* Illegal command function */
int xsvfDoXCOMPLETE( SXsvfInfo* pXsvfInfo );
int xsvfDoXTDOMASK( SXsvfInfo* pXsvfInfo );
int xsvfDoXSIR( SXsvfInfo* pXsvfInfo );
int xsvfDoXSIR2( SXsvfInfo* pXsvfInfo );
int xsvfDoXSDR( SXsvfInfo* pXsvfInfo );
int xsvfDoXRUNTEST( SXsvfInfo* pXsvfInfo );
int xsvfDoXREPEAT( SXsvfInfo* pXsvfInfo );
int xsvfDoXSDRSIZE( SXsvfInfo* pXsvfInfo );
int xsvfDoXSDRTDO( SXsvfInfo* pXsvfInfo );
int xsvfDoXSETSDRMASKS( SXsvfInfo* pXsvfInfo );
int xsvfDoXSDRINC( SXsvfInfo* pXsvfInfo );
int xsvfDoXSDRBCE( SXsvfInfo* pXsvfInfo );
int xsvfDoXSDRTDOBCE( SXsvfInfo* pXsvfInfo );
int xsvfDoXSTATE( SXsvfInfo* pXsvfInfo );
int xsvfDoXENDXR( SXsvfInfo* pXsvfInfo );
int xsvfDoXCOMMENT( SXsvfInfo* pXsvfInfo );
int xsvfDoXWAIT( SXsvfInfo* pXsvfInfo );
/* Insert new command functions here */

/*============================================================================
* XSVF Global Variables
============================================================================*/

/* Array of XSVF command functions.  Must follow command byte value order! */
/* If your compiler cannot take this form, then convert to a switch statement*/
TXsvfDoCmdFuncPtr   xsvf_pfDoCmd[]  =
{
    xsvfDoXCOMPLETE,        /*  0 */
    xsvfDoXTDOMASK,         /*  1 */
    xsvfDoXSIR,             /*  2 */
    xsvfDoXSDR,             /*  3 */
    xsvfDoXRUNTEST,         /*  4 */
    xsvfDoIllegalCmd,       /*  5 */
    xsvfDoIllegalCmd,       /*  6 */
    xsvfDoXREPEAT,          /*  7 */
    xsvfDoXSDRSIZE,         /*  8 */
    xsvfDoXSDRTDO,          /*  9 */
    xsvfDoXSETSDRMASKS,     /* 10 */
    xsvfDoXSDRINC,          /* 11 */
    xsvfDoXSDRBCE,          /* 12 */
    xsvfDoXSDRBCE,          /* 13 */
    xsvfDoXSDRBCE,          /* 14 */
    xsvfDoXSDRTDOBCE,       /* 15 */
    xsvfDoXSDRTDOBCE,       /* 16 */
    xsvfDoXSDRTDOBCE,       /* 17 */
    xsvfDoXSTATE,           /* 18 */
    xsvfDoXENDXR,           /* 19 */
    xsvfDoXENDXR,           /* 20 */
    xsvfDoXSIR2,            /* 21 */
    xsvfDoXCOMMENT,         /* 22 */
    xsvfDoXWAIT             /* 23 */
/* Insert new command functions here */
};

/*============================================================================
* Utility Functions
============================================================================*/

/*****************************************************************************
* Function:     xsvfInfoInit
* Description:  Initialize the xsvfInfo data.
* Parameters:   pXsvfInfo   - ptr to the XSVF info structure.
* Returns:      int         - 0 = success; otherwise error.
*****************************************************************************/
int xsvfInfoInit( SXsvfInfo* pXsvfInfo )
{
    pXsvfInfo->ucComplete       = 0;
    pXsvfInfo->ucCommand        = XCOMPLETE;
    pXsvfInfo->lCommandCount    = 0;
    pXsvfInfo->iErrorCode       = XSVF_ERROR_NONE;
    pXsvfInfo->ucMaxRepeat      = 0;
    pXsvfInfo->ucTapState       = XTAPSTATE_RESET;
    pXsvfInfo->ucEndIR          = XTAPSTATE_RUNTEST;
    pXsvfInfo->ucEndDR          = XTAPSTATE_RUNTEST;
    pXsvfInfo->lShiftLengthBits = 0L;
    pXsvfInfo->sShiftLengthBytes= 0;
    pXsvfInfo->lRunTestTime     = 0L;

    return( 0 );
}

/*****************************************************************************
* Function:     xsvfInfoCleanup
* Description:  Cleanup the xsvfInfo data.
* Parameters:   pXsvfInfo   - ptr to the XSVF info structure.
* Returns:      void.
*****************************************************************************/
void xsvfInfoCleanup( SXsvfInfo* pXsvfInfo )
{
}

/*****************************************************************************
* Function:     xsvfGetAsNumBytes
* Description:  Calculate the number of bytes the given number of bits
*               consumes.
* Parameters:   lNumBits    - the number of bits.
* Returns:      short       - the number of bytes to store the number of bits.
*****************************************************************************/
short xsvfGetAsNumBytes( long lNumBits )
{
    return( (short)( ( lNumBits + 7L ) / 8L ) );
}

/*****************************************************************************
* Function:     xsvfTmsTransition
* Description:  Apply TMS and transition TAP controller by applying one TCK
*               cycle.
* Parameters:   sTms    - new TMS value.
* Returns:      void.
*****************************************************************************/
void xsvfTmsTransition( short sTms )
{
    SetPort_TMS( sTms );
    SetPort_TCK( 0 );
    SetPort_TCK( 1 );
}

/*****************************************************************************
* Function:     xsvfGotoTapState
* Description:  From the current TAP state, go to the named TAP state.
*               A target state of RESET ALWAYS causes TMS reset sequence.
*               All SVF standard stable state paths are supported.
*               All state transitions are supported except for the following
*               which cause an XSVF_ERROR_ILLEGALSTATE:
*                   - Target==DREXIT2;  Start!=DRPAUSE
*                   - Target==IREXIT2;  Start!=IRPAUSE
* Parameters:   pucTapState     - Current TAP state; returns final TAP state.
*               ucTargetState   - New target TAP state.
* Returns:      int             - 0 = success; otherwise error.
*****************************************************************************/
int xsvfGotoTapState( unsigned char*   pucTapState,
                      unsigned char    ucTargetState )
{
    int i;
    int iErrorCode;

    iErrorCode  = XSVF_ERROR_NONE;
    if ( ucTargetState == XTAPSTATE_RESET )
    {
        /* If RESET, always perform TMS reset sequence to reset/sync TAPs */
        xsvfTmsTransition( 1 );
        for ( i = 0; i < 5; ++i )
        {
            SetPort_TCK( 0 );
            SetPort_TCK( 1 );
        }
        *pucTapState    = XTAPSTATE_RESET;
    }
    else if ( ( ucTargetState != *pucTapState ) &&
              ( ( ( ucTargetState == XTAPSTATE_EXIT2DR ) && ( *pucTapState != XTAPSTATE_PAUSEDR ) ) ||
                ( ( ucTargetState == XTAPSTATE_EXIT2IR ) && ( *pucTapState != XTAPSTATE_PAUSEIR ) ) ) )
    {
        /* Trap illegal TAP state path specification */
        iErrorCode      = XSVF_ERROR_ILLEGALSTATE;
    }
    else
    {
        if ( ucTargetState == *pucTapState )
        {
            /* Already in target state.  Do nothing except when in DRPAUSE
               or in IRPAUSE to comply with SVF standard */
            if ( ucTargetState == XTAPSTATE_PAUSEDR )
            {
                xsvfTmsTransition( 1 );
                *pucTapState    = XTAPSTATE_EXIT2DR;
            }
            else if ( ucTargetState == XTAPSTATE_PAUSEIR )
            {
                xsvfTmsTransition( 1 );
                *pucTapState    = XTAPSTATE_EXIT2IR;
            }
        }

        /* Perform TAP state transitions to get to the target state */
        while ( ucTargetState != *pucTapState )
        {
            switch ( *pucTapState )
            {
            case XTAPSTATE_RESET:
                xsvfTmsTransition( 0 );
                *pucTapState    = XTAPSTATE_RUNTEST;
                break;
            case XTAPSTATE_RUNTEST:
                xsvfTmsTransition( 1 );
                *pucTapState    = XTAPSTATE_SELECTDR;
                break;
            case XTAPSTATE_SELECTDR:
                if ( ucTargetState >= XTAPSTATE_IRSTATES )
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_SELECTIR;
                }
                else
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_CAPTUREDR;
                }
                break;
            case XTAPSTATE_CAPTUREDR:
                if ( ucTargetState == XTAPSTATE_SHIFTDR )
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_SHIFTDR;
                }
                else
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_EXIT1DR;
                }
                break;
            case XTAPSTATE_SHIFTDR:
                xsvfTmsTransition( 1 );
                *pucTapState    = XTAPSTATE_EXIT1DR;
                break;
            case XTAPSTATE_EXIT1DR:
                if ( ucTargetState == XTAPSTATE_PAUSEDR )
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_PAUSEDR;
                }
                else
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_UPDATEDR;
                }
                break;
            case XTAPSTATE_PAUSEDR:
                xsvfTmsTransition( 1 );
                *pucTapState    = XTAPSTATE_EXIT2DR;
                break;
            case XTAPSTATE_EXIT2DR:
                if ( ucTargetState == XTAPSTATE_SHIFTDR )
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_SHIFTDR;
                }
                else
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_UPDATEDR;
                }
                break;
            case XTAPSTATE_UPDATEDR:
                if ( ucTargetState == XTAPSTATE_RUNTEST )
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_RUNTEST;
                }
                else
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_SELECTDR;
                }
                break;
            case XTAPSTATE_SELECTIR:
                xsvfTmsTransition( 0 );
                *pucTapState    = XTAPSTATE_CAPTUREIR;
                break;
            case XTAPSTATE_CAPTUREIR:
                if ( ucTargetState == XTAPSTATE_SHIFTIR )
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_SHIFTIR;
                }
                else
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_EXIT1IR;
                }
                break;
            case XTAPSTATE_SHIFTIR:
                xsvfTmsTransition( 1 );
                *pucTapState    = XTAPSTATE_EXIT1IR;
                break;
            case XTAPSTATE_EXIT1IR:
                if ( ucTargetState == XTAPSTATE_PAUSEIR )
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_PAUSEIR;
                }
                else
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_UPDATEIR;
                }
                break;
            case XTAPSTATE_PAUSEIR:
                xsvfTmsTransition( 1 );
                *pucTapState    = XTAPSTATE_EXIT2IR;
                break;
            case XTAPSTATE_EXIT2IR:
                if ( ucTargetState == XTAPSTATE_SHIFTIR )
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_SHIFTIR;
                }
                else
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_UPDATEIR;
                }
                break;
            case XTAPSTATE_UPDATEIR:
                if ( ucTargetState == XTAPSTATE_RUNTEST )
                {
                    xsvfTmsTransition( 0 );
                    *pucTapState    = XTAPSTATE_RUNTEST;
                }
                else
                {
                    xsvfTmsTransition( 1 );
                    *pucTapState    = XTAPSTATE_SELECTDR;
                }
                break;
            default:
                iErrorCode      = XSVF_ERROR_ILLEGALSTATE;
                *pucTapState    = ucTargetState;    /* Exit while loop */
                break;
            }
        }
    }

    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfShiftOnly
* Description:  Assumes that starting TAP state is SHIFT-DR or SHIFT-IR.
*               Shift the given TDI data into the JTAG scan chain.
*               Optionally, save the TDO data shifted out of the scan chain.
*               Last shift cycle is special:  capture last TDO, set last TDI,
*               but does not pulse TCK.  Caller must pulse TCK and optionally
*               set TMS=1 to exit shift state.
* Parameters:   lNumBits        - number of bits to shift.
*               plvTdi          - ptr to lenval for TDI data.
*               plvTdoCaptured  - ptr to lenval for storing captured TDO data.
*               iExitShift      - 1=exit at end of shift; 0=stay in Shift-DR.
* Returns:      void.
*****************************************************************************/
void xsvfShiftOnly( long    lNumBits,
                    lenVal* plvTdi,
                    lenVal* plvTdoCaptured,
                    int     iExitShift )
{
    unsigned char*  pucTdi;
    unsigned char*  pucTdo;
    unsigned char   ucTdiByte;
    unsigned char   ucTdoByte;
    unsigned char   ucTdoBit;
    int             i;

    /* assert( ( ( lNumBits + 7 ) / 8 ) == plvTdi->len ); */

    /* Initialize TDO storage len == TDI len */
    pucTdo  = 0;
    if ( plvTdoCaptured )
    {
        plvTdoCaptured->len = plvTdi->len;
        pucTdo              = plvTdoCaptured->val + plvTdi->len;
    }

    /* Shift LSB first.  val[N-1] == LSB.  val[0] == MSB. */
    pucTdi  = plvTdi->val + plvTdi->len;
    while ( lNumBits )
    {
        /* Process on a byte-basis */
        ucTdiByte   = (*(--pucTdi));
        ucTdoByte   = 0;
        for ( i = 0; ( lNumBits && ( i < 8 ) ); ++i )
        {
            --lNumBits;
            if ( iExitShift && !lNumBits )
            {
                /* Exit Shift-DR state */
                SetPort_TMS( 1 );
            }

            /* Set the new TDI value */
            SetPort_TDI( ucTdiByte & 1 );
            ucTdiByte   >>= 1;

            /* Set TCK low */
            SetPort_TCK( 0 );

            if ( pucTdo )
            {
                /* Save the TDO value */
                ucTdoBit    = GetPort_TDO();
                ucTdoByte   |= ( ucTdoBit << i );
            }

            /* Set TCK high */
            SetPort_TCK( 1 );
        }

        /* Save the TDO byte value */
        if ( pucTdo )
        {
            (*(--pucTdo))   = ucTdoByte;
        }
    }
}

/*****************************************************************************
* Function:     xsvfShift
* Description:  Goes to the given starting TAP state.
*               Calls xsvfShiftOnly to shift in the given TDI data and
*               optionally capture the TDO data.
*               Compares the TDO captured data against the TDO expected
*               data.
*               If a data mismatch occurs, then executes the exception
*               handling loop upto ucMaxRepeat times.
* Parameters:   pucTapState     - Ptr to current TAP state.
*               ucStartState    - Starting shift state: Shift-DR or Shift-IR.
*               lNumBits        - number of bits to shift.
*               plvTdi          - ptr to lenval for TDI data.
*               plvTdoCaptured  - ptr to lenval for storing TDO data.
*               plvTdoExpected  - ptr to expected TDO data.
*               plvTdoMask      - ptr to TDO mask.
*               ucEndState      - state in which to end the shift.
*               lRunTestTime    - amount of time to wait after the shift.
*               ucMaxRepeat     - Maximum number of retries on TDO mismatch.
* Returns:      int             - 0 = success; otherwise TDO mismatch.
* Notes:        XC9500XL-only Optimization:
*               Skip the waitTime() if plvTdoMask->val[0:plvTdoMask->len-1]
*               is NOT all zeros and sMatch==1.
*****************************************************************************/
int xsvfShift( unsigned char*   pucTapState,
               unsigned char    ucStartState,
               long             lNumBits,
               lenVal*          plvTdi,
               lenVal*          plvTdoCaptured,
               lenVal*          plvTdoExpected,
               lenVal*          plvTdoMask,
               unsigned char    ucEndState,
               long             lRunTestTime,
               unsigned char    ucMaxRepeat )
{
    int             iErrorCode;
    int             iMismatch;
    unsigned char   ucRepeat;
    int             iExitShift;

    iErrorCode  = XSVF_ERROR_NONE;
    iMismatch   = 0;
    ucRepeat    = 0;
    iExitShift  = ( ucStartState != ucEndState );

    if ( !lNumBits )
    {
        /* Compatibility with XSVF2.00:  XSDR 0 = no shift, but wait in RTI */
        if ( lRunTestTime )
        {
            /* Wait for prespecified XRUNTEST time */
            xsvfGotoTapState( pucTapState, XTAPSTATE_RUNTEST );
            waitTime( lRunTestTime );
        }
    }
    else
    {
        do
        {
            /* Goto Shift-DR or Shift-IR */
            xsvfGotoTapState( pucTapState, ucStartState );

            /* Shift TDI and capture TDO */
            xsvfShiftOnly( lNumBits, plvTdi, plvTdoCaptured, iExitShift );

            if ( plvTdoExpected )
            {
                /* Compare TDO data to expected TDO data */
                iMismatch   = !EqualLenVal( plvTdoExpected,
                                            plvTdoCaptured,
                                            plvTdoMask );
            }

            if ( iExitShift )
            {
                /* Update TAP state:  Shift->Exit */
                ++(*pucTapState);

                if ( iMismatch && lRunTestTime && ( ucRepeat < ucMaxRepeat ) )
                {
                    /* Do exception handling retry - ShiftDR only */
                    xsvfGotoTapState( pucTapState, XTAPSTATE_PAUSEDR );
                    /* Shift 1 extra bit */
                    xsvfGotoTapState( pucTapState, XTAPSTATE_SHIFTDR );
                    /* Increment RUNTEST time by an additional 25% */
                    lRunTestTime    += ( lRunTestTime >> 2 );
                }
                else
                {
                    /* Do normal exit from Shift-XR */
                    xsvfGotoTapState( pucTapState, ucEndState );
                }

                if ( lRunTestTime )
                {
                    /* Wait for prespecified XRUNTEST time */
                    xsvfGotoTapState( pucTapState, XTAPSTATE_RUNTEST );
                    waitTime( lRunTestTime );
                }
            }
        } while ( iMismatch && ( ucRepeat++ < ucMaxRepeat ) );
    }

    if ( iMismatch )
    {
        if ( ucMaxRepeat && ( ucRepeat > ucMaxRepeat ) )
        {
            iErrorCode  = XSVF_ERROR_MAXRETRIES;
        }
        else
        {
            iErrorCode  = XSVF_ERROR_TDOMISMATCH;
        }
    }

    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfBasicXSDRTDO
* Description:  Get the XSDRTDO parameters and execute the XSDRTDO command.
*               This is the common function for all XSDRTDO commands.
* Parameters:   pucTapState         - Current TAP state.
*               lShiftLengthBits    - number of bits to shift.
*               sShiftLengthBytes   - number of bytes to read.
*               plvTdi              - ptr to lenval for TDI data.
*               lvTdoCaptured       - ptr to lenval for storing TDO data.
*               iEndState           - state in which to end the shift.
*               lRunTestTime        - amount of time to wait after the shift.
*               ucMaxRepeat         - maximum xc9500/xl retries.
* Returns:      int                 - 0 = success; otherwise TDO mismatch.
*****************************************************************************/
int xsvfBasicXSDRTDO( unsigned char*    pucTapState,
                      long              lShiftLengthBits,
                      short             sShiftLengthBytes,
                      lenVal*           plvTdi,
                      lenVal*           plvTdoCaptured,
                      lenVal*           plvTdoExpected,
                      lenVal*           plvTdoMask,
                      unsigned char     ucEndState,
                      long              lRunTestTime,
                      unsigned char     ucMaxRepeat )
{
    readVal( plvTdi, sShiftLengthBytes );
    if ( plvTdoExpected )
    {
        readVal( plvTdoExpected, sShiftLengthBytes );
    }
    return( xsvfShift( pucTapState, XTAPSTATE_SHIFTDR, lShiftLengthBits,
                       plvTdi, plvTdoCaptured, plvTdoExpected, plvTdoMask,
                       ucEndState, lRunTestTime, ucMaxRepeat ) );
}

/*****************************************************************************
* Function:     xsvfDoSDRMasking
* Description:  Update the data value with the next XSDRINC data and address.
* Example:      dataVal=0x01ff, nextData=0xab, addressMask=0x0100,
*               dataMask=0x00ff, should set dataVal to 0x02ab
* Parameters:   plvTdi          - The current TDI value.
*               plvNextData     - the next data value.
*               plvAddressMask  - the address mask.
*               plvDataMask     - the data mask.
* Returns:      void.
*****************************************************************************/
void xsvfDoSDRMasking( lenVal*  plvTdi,
                       lenVal*  plvNextData,
                       lenVal*  plvAddressMask,
                       lenVal*  plvDataMask )
{
    int             i;
    unsigned char   ucTdi;
    unsigned char   ucTdiMask;
    unsigned char   ucDataMask;
    unsigned char   ucNextData;
    unsigned char   ucNextMask;
    short           sNextData;

    /* add the address Mask to dataVal and return as a new dataVal */
    addVal( plvTdi, plvTdi, plvAddressMask );

    ucNextData  = 0;
    ucNextMask  = 0;
    sNextData   = plvNextData->len;
    for ( i = plvDataMask->len - 1; i >= 0; --i )
    {
        /* Go through data mask in reverse order looking for mask (1) bits */
        ucDataMask  = plvDataMask->val[ i ];
        if ( ucDataMask )
        {
            /* Retrieve the corresponding TDI byte value */
            ucTdi       = plvTdi->val[ i ];

            /* For each bit in the data mask byte, look for 1's */
            ucTdiMask   = 1;
            while ( ucDataMask )
            {
                if ( ucDataMask & 1 )
                {
                    if ( !ucNextMask )
                    {
                        /* Get the next data byte */
                        ucNextData  = plvNextData->val[ --sNextData ];
                        ucNextMask  = 1;
                    }

                    /* Set or clear the data bit according to the next data */
                    if ( ucNextData & ucNextMask )
                    {
                        ucTdi   |= ucTdiMask;       /* Set bit */
                    }
                    else
                    {
                        ucTdi   &= ( ~ucTdiMask );  /* Clear bit */
                    }

                    /* Update the next data */
                    ucNextMask  <<= 1;
                }
                ucTdiMask   <<= 1;
                ucDataMask  >>= 1;
            }

            /* Update the TDI value */
            plvTdi->val[ i ]    = ucTdi;
        }
    }
}

/*============================================================================
* XSVF Command Functions (type = TXsvfDoCmdFuncPtr)
* These functions update pXsvfInfo->iErrorCode only on an error.
* Otherwise, the error code is left alone.
* The function returns the error code from the function.
============================================================================*/

/*****************************************************************************
* Function:     xsvfDoIllegalCmd
* Description:  Function place holder for illegal/unsupported commands.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoIllegalCmd( SXsvfInfo* pXsvfInfo )
{
    pXsvfInfo->iErrorCode   = XSVF_ERROR_ILLEGALCMD;
    return( pXsvfInfo->iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXCOMPLETE
* Description:  XCOMPLETE (no parameters)
*               Update complete status for XSVF player.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXCOMPLETE( SXsvfInfo* pXsvfInfo )
{
    pXsvfInfo->ucComplete   = 1;
    return( XSVF_ERROR_NONE );
}

/*****************************************************************************
* Function:     xsvfDoXTDOMASK
* Description:  XTDOMASK <lenVal.TdoMask[XSDRSIZE]>
*               Prespecify the TDO compare mask.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXTDOMASK( SXsvfInfo* pXsvfInfo )
{
    readVal( &(pXsvfInfo->lvTdoMask), pXsvfInfo->sShiftLengthBytes );
    return( XSVF_ERROR_NONE );
}

/*****************************************************************************
* Function:     xsvfDoXSIR
* Description:  XSIR <(byte)shiftlen> <lenVal.TDI[shiftlen]>
*               Get the instruction and shift the instruction into the TAP.
*               If prespecified XRUNTEST!=0, goto RUNTEST and wait after
*               the shift for XRUNTEST usec.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSIR( SXsvfInfo* pXsvfInfo )
{
    unsigned char   ucShiftIrBits;
    short           sShiftIrBytes;
    int             iErrorCode;

    /* Get the shift length and store */
    ucShiftIrBits = readByte ();
    sShiftIrBytes   = xsvfGetAsNumBytes( ucShiftIrBits );

    if ( sShiftIrBytes > MAX_LEN )
    {
        iErrorCode  = XSVF_ERROR_DATAOVERFLOW;
    }
    else
    {
        /* Get and store instruction to shift in */
        readVal( &(pXsvfInfo->lvTdi), xsvfGetAsNumBytes( ucShiftIrBits ) );

        /* Shift the data */
        iErrorCode  = xsvfShift( &(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTIR,
                                 ucShiftIrBits, &(pXsvfInfo->lvTdi),
                                 /*plvTdoCaptured*/0, /*plvTdoExpected*/0,
                                 /*plvTdoMask*/0, pXsvfInfo->ucEndIR,
                                 pXsvfInfo->lRunTestTime, /*ucMaxRepeat*/0 );
    }

    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXSIR2
* Description:  XSIR <(2-byte)shiftlen> <lenVal.TDI[shiftlen]>
*               Get the instruction and shift the instruction into the TAP.
*               If prespecified XRUNTEST!=0, goto RUNTEST and wait after
*               the shift for XRUNTEST usec.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSIR2( SXsvfInfo* pXsvfInfo )
{
    long            lShiftIrBits;
    short           sShiftIrBytes;
    int             iErrorCode;

    /* Get the shift length and store */
    readVal( &(pXsvfInfo->lvTdi), 2 );
    lShiftIrBits    = value( &(pXsvfInfo->lvTdi) );
    sShiftIrBytes   = xsvfGetAsNumBytes( lShiftIrBits );

    if ( sShiftIrBytes > MAX_LEN )
    {
        iErrorCode  = XSVF_ERROR_DATAOVERFLOW;
    }
    else
    {
        /* Get and store instruction to shift in */
        readVal( &(pXsvfInfo->lvTdi), xsvfGetAsNumBytes( lShiftIrBits ) );

        /* Shift the data */
        iErrorCode  = xsvfShift( &(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTIR,
                                 lShiftIrBits, &(pXsvfInfo->lvTdi),
                                 /*plvTdoCaptured*/0, /*plvTdoExpected*/0,
                                 /*plvTdoMask*/0, pXsvfInfo->ucEndIR,
                                 pXsvfInfo->lRunTestTime, /*ucMaxRepeat*/0 );
    }

    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXSDR
* Description:  XSDR <lenVal.TDI[XSDRSIZE]>
*               Shift the given TDI data into the JTAG scan chain.
*               Compare the captured TDO with the expected TDO from the
*               previous XSDRTDO command using the previously specified
*               XTDOMASK.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSDR( SXsvfInfo* pXsvfInfo )
{
    int iErrorCode;
    readVal( &(pXsvfInfo->lvTdi), pXsvfInfo->sShiftLengthBytes );
    /* use TDOExpected from last XSDRTDO instruction */
    iErrorCode  = xsvfShift( &(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTDR,
                             pXsvfInfo->lShiftLengthBits, &(pXsvfInfo->lvTdi),
                             &(pXsvfInfo->lvTdoCaptured),
                             &(pXsvfInfo->lvTdoExpected),
                             &(pXsvfInfo->lvTdoMask), pXsvfInfo->ucEndDR,
                             pXsvfInfo->lRunTestTime, pXsvfInfo->ucMaxRepeat );
    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXRUNTEST
* Description:  XRUNTEST <uint32>
*               Prespecify the XRUNTEST wait time for shift operations.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXRUNTEST( SXsvfInfo* pXsvfInfo )
{
    readVal( &(pXsvfInfo->lvTdi), 4 );
    pXsvfInfo->lRunTestTime = value( &(pXsvfInfo->lvTdi) );

    if ( pXsvfInfo->lRunTestTime > 0 )
        SetLED_Toggle( LED_YELLOW_2 );

    return( XSVF_ERROR_NONE );
}

/*****************************************************************************
* Function:     xsvfDoXREPEAT
* Description:  XREPEAT <byte>
*               Prespecify the maximum number of XC9500/XL retries.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXREPEAT( SXsvfInfo* pXsvfInfo )
{
    pXsvfInfo->ucMaxRepeat = readByte ();
    return( XSVF_ERROR_NONE );
}

/*****************************************************************************
* Function:     xsvfDoXSDRSIZE
* Description:  XSDRSIZE <uint32>
*               Prespecify the XRUNTEST wait time for shift operations.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSDRSIZE( SXsvfInfo* pXsvfInfo )
{
    int iErrorCode;
    iErrorCode  = XSVF_ERROR_NONE;
    readVal( &(pXsvfInfo->lvTdi), 4 );
    pXsvfInfo->lShiftLengthBits = value( &(pXsvfInfo->lvTdi) );
    pXsvfInfo->sShiftLengthBytes= xsvfGetAsNumBytes( pXsvfInfo->lShiftLengthBits );
    if ( pXsvfInfo->sShiftLengthBytes > MAX_LEN )
    {
        iErrorCode  = XSVF_ERROR_DATAOVERFLOW;
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXSDRTDO
* Description:  XSDRTDO <lenVal.TDI[XSDRSIZE]> <lenVal.TDO[XSDRSIZE]>
*               Get the TDI and expected TDO values.  Then, shift.
*               Compare the expected TDO with the captured TDO using the
*               prespecified XTDOMASK.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSDRTDO( SXsvfInfo* pXsvfInfo )
{
    int iErrorCode;
    iErrorCode  = xsvfBasicXSDRTDO( &(pXsvfInfo->ucTapState),
                                    pXsvfInfo->lShiftLengthBits,
                                    pXsvfInfo->sShiftLengthBytes,
                                    &(pXsvfInfo->lvTdi),
                                    &(pXsvfInfo->lvTdoCaptured),
                                    &(pXsvfInfo->lvTdoExpected),
                                    &(pXsvfInfo->lvTdoMask),
                                    pXsvfInfo->ucEndDR,
                                    pXsvfInfo->lRunTestTime,
                                    pXsvfInfo->ucMaxRepeat );
    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXSETSDRMASKS
* Description:  XSETSDRMASKS <lenVal.AddressMask[XSDRSIZE]>
*                            <lenVal.DataMask[XSDRSIZE]>
*               Get the prespecified address and data mask for the XSDRINC
*               command.
*               Used for xc9500/xl compressed XSVF data.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSETSDRMASKS( SXsvfInfo* pXsvfInfo )
{
    /* read the addressMask */
    readVal( &(pXsvfInfo->lvAddressMask), pXsvfInfo->sShiftLengthBytes );
    /* read the dataMask    */
    readVal( &(pXsvfInfo->lvDataMask), pXsvfInfo->sShiftLengthBytes );

    return( XSVF_ERROR_NONE );
}

/*****************************************************************************
* Function:     xsvfDoXSDRINC
* Description:  XSDRINC <lenVal.firstTDI[XSDRSIZE]> <byte(numTimes)>
*                       <lenVal.data[XSETSDRMASKS.dataMask.len]> ...
*               Get the XSDRINC parameters and execute the XSDRINC command.
*               XSDRINC starts by loading the first TDI shift value.
*               Then, for numTimes, XSDRINC gets the next piece of data,
*               replaces the bits from the starting TDI as defined by the
*               XSETSDRMASKS.dataMask, adds the address mask from
*               XSETSDRMASKS.addressMask, shifts the new TDI value,
*               and compares the TDO to the expected TDO from the previous
*               XSDRTDO command using the XTDOMASK.
*               Used for xc9500/xl compressed XSVF data.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSDRINC( SXsvfInfo* pXsvfInfo )
{
    int             iErrorCode;
    int             iDataMaskLen;
    unsigned char   ucDataMask;
    unsigned char   ucNumTimes;
    unsigned char   i;

    readVal( &(pXsvfInfo->lvTdi), pXsvfInfo->sShiftLengthBytes );
    iErrorCode  = xsvfShift( &(pXsvfInfo->ucTapState), XTAPSTATE_SHIFTDR,
                             pXsvfInfo->lShiftLengthBits,
                             &(pXsvfInfo->lvTdi), &(pXsvfInfo->lvTdoCaptured),
                             &(pXsvfInfo->lvTdoExpected),
                             &(pXsvfInfo->lvTdoMask), pXsvfInfo->ucEndDR,
                             pXsvfInfo->lRunTestTime, pXsvfInfo->ucMaxRepeat );
    if ( !iErrorCode )
    {
        /* Calculate number of data mask bits */
        iDataMaskLen    = 0;
        for ( i = 0; i < pXsvfInfo->lvDataMask.len; ++i )
        {
            ucDataMask  = pXsvfInfo->lvDataMask.val[ i ];
            while ( ucDataMask )
            {
                iDataMaskLen    += ( ucDataMask & 1 );
                ucDataMask      >>= 1;
            }
        }

        /* Get the number of data pieces, i.e. number of times to shift */
        ucNumTimes = readByte ();

        /* For numTimes, get data, fix TDI, and shift */
        for ( i = 0; !iErrorCode && ( i < ucNumTimes ); ++i )
        {
            readVal( &(pXsvfInfo->lvNextData),
                     xsvfGetAsNumBytes( iDataMaskLen ) );
            xsvfDoSDRMasking( &(pXsvfInfo->lvTdi),
                              &(pXsvfInfo->lvNextData),
                              &(pXsvfInfo->lvAddressMask),
                              &(pXsvfInfo->lvDataMask) );
            iErrorCode  = xsvfShift( &(pXsvfInfo->ucTapState),
                                     XTAPSTATE_SHIFTDR,
                                     pXsvfInfo->lShiftLengthBits,
                                     &(pXsvfInfo->lvTdi),
                                     &(pXsvfInfo->lvTdoCaptured),
                                     &(pXsvfInfo->lvTdoExpected),
                                     &(pXsvfInfo->lvTdoMask),
                                     pXsvfInfo->ucEndDR,
                                     pXsvfInfo->lRunTestTime,
                                     pXsvfInfo->ucMaxRepeat );
        }
    }
    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXSDRBCE
* Description:  XSDRB/XSDRC/XSDRE <lenVal.TDI[XSDRSIZE]>
*               If not already in SHIFTDR, goto SHIFTDR.
*               Shift the given TDI data into the JTAG scan chain.
*               Ignore TDO.
*               If cmd==XSDRE, then goto ENDDR.  Otherwise, stay in ShiftDR.
*               XSDRB, XSDRC, and XSDRE are the same implementation.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSDRBCE( SXsvfInfo* pXsvfInfo )
{
    unsigned char   ucEndDR;
    int             iErrorCode;
    ucEndDR = (unsigned char)(( pXsvfInfo->ucCommand == XSDRE ) ?
                                pXsvfInfo->ucEndDR : XTAPSTATE_SHIFTDR);
    iErrorCode  = xsvfBasicXSDRTDO( &(pXsvfInfo->ucTapState),
                                    pXsvfInfo->lShiftLengthBits,
                                    pXsvfInfo->sShiftLengthBytes,
                                    &(pXsvfInfo->lvTdi),
                                    /*plvTdoCaptured*/0, /*plvTdoExpected*/0,
                                    /*plvTdoMask*/0, ucEndDR,
                                    /*lRunTestTime*/0, /*ucMaxRepeat*/0 );
    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXSDRTDOBCE
* Description:  XSDRB/XSDRC/XSDRE <lenVal.TDI[XSDRSIZE]> <lenVal.TDO[XSDRSIZE]>
*               If not already in SHIFTDR, goto SHIFTDR.
*               Shift the given TDI data into the JTAG scan chain.
*               Compare TDO, but do NOT use XTDOMASK.
*               If cmd==XSDRTDOE, then goto ENDDR.  Otherwise, stay in ShiftDR.
*               XSDRTDOB, XSDRTDOC, and XSDRTDOE are the same implementation.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSDRTDOBCE( SXsvfInfo* pXsvfInfo )
{
    unsigned char   ucEndDR;
    int             iErrorCode;
    ucEndDR = (unsigned char)(( pXsvfInfo->ucCommand == XSDRTDOE ) ?
                                pXsvfInfo->ucEndDR : XTAPSTATE_SHIFTDR);
    iErrorCode  = xsvfBasicXSDRTDO( &(pXsvfInfo->ucTapState),
                                    pXsvfInfo->lShiftLengthBits,
                                    pXsvfInfo->sShiftLengthBytes,
                                    &(pXsvfInfo->lvTdi),
                                    &(pXsvfInfo->lvTdoCaptured),
                                    &(pXsvfInfo->lvTdoExpected),
                                    /*plvTdoMask*/0, ucEndDR,
                                    /*lRunTestTime*/0, /*ucMaxRepeat*/0 );
    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXSTATE
* Description:  XSTATE <byte>
*               <byte> == XTAPSTATE;
*               Get the state parameter and transition the TAP to that state.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXSTATE( SXsvfInfo* pXsvfInfo )
{
    unsigned char   ucNextState;
    int             iErrorCode;
    ucNextState = readByte();
    iErrorCode  = xsvfGotoTapState( &(pXsvfInfo->ucTapState), ucNextState );
    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXENDXR
* Description:  XENDIR/XENDDR <byte>
*               <byte>:  0 = RUNTEST;  1 = PAUSE.
*               Get the prespecified XENDIR or XENDDR.
*               Both XENDIR and XENDDR use the same implementation.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXENDXR( SXsvfInfo* pXsvfInfo )
{
    int             iErrorCode;
    unsigned char   ucEndState;

    iErrorCode  = XSVF_ERROR_NONE;
    ucEndState = readByte ();
    if ( ( ucEndState != XENDXR_RUNTEST ) && ( ucEndState != XENDXR_PAUSE ) )
    {
        iErrorCode  = XSVF_ERROR_ILLEGALSTATE;
    }
    else
    {

    if ( pXsvfInfo->ucCommand == XENDIR )
    {
            if ( ucEndState == XENDXR_RUNTEST )
            {
                pXsvfInfo->ucEndIR  = XTAPSTATE_RUNTEST;
            }
            else
            {
                pXsvfInfo->ucEndIR  = XTAPSTATE_PAUSEIR;
            }
        }
    else    /* XENDDR */
    {
            if ( ucEndState == XENDXR_RUNTEST )
            {
                pXsvfInfo->ucEndDR  = XTAPSTATE_RUNTEST;
            }
    else
    {
                pXsvfInfo->ucEndDR  = XTAPSTATE_PAUSEDR;
            }
        }
    }

    if ( iErrorCode != XSVF_ERROR_NONE )
    {
        pXsvfInfo->iErrorCode   = iErrorCode;
    }
    return( iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXCOMMENT
* Description:  XCOMMENT <text string ending in \0>
*               <text string ending in \0> == text comment;
*               Arbitrary comment embedded in the XSVF.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXCOMMENT( SXsvfInfo* pXsvfInfo )
{
    /* Read through the comment to the end '\0' and ignore */
    unsigned char   ucText;

    do
    {
        ucText = readByte ();
    } while ( ucText );

    pXsvfInfo->iErrorCode   = XSVF_ERROR_NONE;

    return( pXsvfInfo->iErrorCode );
}

/*****************************************************************************
* Function:     xsvfDoXWAIT
* Description:  XWAIT <wait_state> <end_state> <wait_time>
*               If not already in <wait_state>, then go to <wait_state>.
*               Wait in <wait_state> for <wait_time> microseconds.
*               Finally, if not already in <end_state>, then goto <end_state>.
* Parameters:   pXsvfInfo   - XSVF information pointer.
* Returns:      int         - 0 = success;  non-zero = error.
*****************************************************************************/
int xsvfDoXWAIT( SXsvfInfo* pXsvfInfo )
{
    unsigned char   ucWaitState;
    unsigned char   ucEndState;
    long            lWaitTime;

    /* Get Parameters */
    /* <wait_state> */
    readVal( &(pXsvfInfo->lvTdi), 1 );
    ucWaitState = pXsvfInfo->lvTdi.val[0];

    /* <end_state> */
    readVal( &(pXsvfInfo->lvTdi), 1 );
    ucEndState = pXsvfInfo->lvTdi.val[0];

    /* <wait_time> */
    readVal( &(pXsvfInfo->lvTdi), 4 );
    lWaitTime = value( &(pXsvfInfo->lvTdi) );

    /* If not already in <wait_state>, go to <wait_state> */
    if ( pXsvfInfo->ucTapState != ucWaitState )
    {
        xsvfGotoTapState( &(pXsvfInfo->ucTapState), ucWaitState );
    }

    /* Wait for <wait_time> microseconds */
    waitTime( lWaitTime );

    /* If not already in <end_state>, go to <end_state> */
    if ( pXsvfInfo->ucTapState != ucEndState )
    {
        xsvfGotoTapState( &(pXsvfInfo->ucTapState), ucEndState );
    }

    return( XSVF_ERROR_NONE );
}


/*============================================================================
* Execution Control Functions
============================================================================*/

/*****************************************************************************
* Function:     xsvfInitialize
* Description:  Initialize the xsvf player.
*               Call this before running the player to initialize the data
*               in the SXsvfInfo struct.
*               xsvfCleanup is called to clean up the data in SXsvfInfo
*               after the XSVF is played.
* Parameters:   pXsvfInfo   - ptr to the XSVF information.
* Returns:      int - 0 = success; otherwise error.
*****************************************************************************/
int xsvfInitialize( SXsvfInfo* pXsvfInfo )
{
    /* Initialize values */
    pXsvfInfo->iErrorCode   = xsvfInfoInit( pXsvfInfo );

    if ( !pXsvfInfo->iErrorCode )
    {
        /* Initialize the TAPs */
        pXsvfInfo->iErrorCode   = xsvfGotoTapState( &(pXsvfInfo->ucTapState),
                                                    XTAPSTATE_RESET );
    }

    return( pXsvfInfo->iErrorCode );
}

/*****************************************************************************
* Function:     xsvfRun
* Description:  Run the xsvf player for a single command and return.
*               First, call xsvfInitialize.
*               Then, repeatedly call this function until an error is detected
*               or until the pXsvfInfo->ucComplete variable is non-zero.
*               Finally, call xsvfCleanup to cleanup any remnants.
* Parameters:   pXsvfInfo   - ptr to the XSVF information.
* Returns:      int         - 0 = success; otherwise error.
*****************************************************************************/
int xsvfRun( SXsvfInfo* pXsvfInfo )
{
    /* Process the XSVF commands */
    if ( (!pXsvfInfo->iErrorCode) && (!pXsvfInfo->ucComplete) )
    {
        /* read 1 byte for the instruction */
        pXsvfInfo->ucCommand = readByte ();
        ++(pXsvfInfo->lCommandCount);

        if ( pXsvfInfo->ucCommand < XLASTCMD )
        {
            /* Execute the command.  Func sets error code. */
            /* If your compiler cannot take this form,
               then convert to a switch statement */
            xsvf_pfDoCmd[ pXsvfInfo->ucCommand ]( pXsvfInfo );
        }
        else
        {
            /* Illegal command value.  Func sets error code. */
            xsvfDoIllegalCmd( pXsvfInfo );
        }
    }

    return( pXsvfInfo->iErrorCode );
}

/*============================================================================
* xsvfExecute() - The primary entry point to the XSVF player
============================================================================*/

SXsvfInfo xsvfInfo;

int
main( void )
{
    ///////////////////////////////////////////////////////////////////////////
    // CONFIGURE PORTF
    // Initial values: All 1 (leds are off)
    SetLED_Off( LED_ALL );
    // Enable PF0..PF3 as outputs
    DDRF  |= _BV(DDF0) | _BV(DDF1) | _BV(DDF2) | _BV(DDF3);
    _NOP (); _NOP ();

    ///////////////////////////////////////////////////////////////////////////
    // CONFIGURE PORTD
    // Initial values: PD0 is pull-up, TMS=1, TCK=0, TDI=0
    PORTD |= _BV(PD0);
    SetPort_TMS( 1 );
    SetPort_TCK( 0 );
    SetPort_TDI( 0 );
    // Configure PD0, PD2 and PD3 as output, PD1 as input
    DDRD  |= _BV(DDD0) | _BV(DDD2) | _BV(DDD3);
    _NOP (); _NOP ();

    ///////////////////////////////////////////////////////////////////////////
    // CONFIGURE PORTA
    // Pull-up on PA0
    PORTA |= _BV(PA0);
    // Configure PA0 as pull-up input
    DDRA  &= ~_BV(DDA0);
    _NOP (); _NOP ();

    ///////////////////////////////////////////////////////////////////////////
     // Turn all LEDs on, wait 200ms and turn all LEDs off.
    SetLED_On( LED_ALL );
    waitTime( 200000L );
    SetLED_Off( LED_ALL );

    ///////////////////////////////////////////////////////////////////////////
    // Wait user to Start key
    //
    int cur_pina = PINA & _BV(PA0);
    for ( int last_pina = cur_pina ;; last_pina = cur_pina )
    {
        cur_pina = PINA & _BV(PA0);

        // if pulled down -> pressed key
        if ( last_pina != cur_pina && cur_pina == 0 ) 
            break;

        SetLED_Toggle( LED_ALL );
        waitTime( 500 );
        SetLED_Toggle( LED_ALL );
        waitTime( 5000 );
    }

    // Acknowledge by Start by blinking Green once
    SetLED_Off( LED_ALL );
    SetLED_On( LED_GREEN );
    waitTime( 500000L );
    SetLED_Off( LED_GREEN );
    waitTime( 200000L );

    // Rewind XSVF data (skip header) & construct xsvfInfo
    //
    extern void* __xsvf_data_begin;
    read_addr = GET_FAR_ADDRESS( __xsvf_data_begin );

    xsvfInitialize( &xsvfInfo );

    // Execute the XSVF
    //
    while ( ! xsvfInfo.iErrorCode && ! xsvfInfo.ucComplete )
    {
        xsvfRun( &xsvfInfo );
    }

    // Turn off LEDS and cleanup xsvf structure
    //
    SetLED_Off( LED_ALL );
    xsvfInfoCleanup( &xsvfInfo );

    // Report SUCCESS or ERROR
    //
    if ( xsvfInfo.iErrorCode == XSVF_ERROR_NONE )
    {
        // SUCCESS: Blink Green
        for ( ;; ) 
        {
            SetLED_Toggle( LED_GREEN );
            waitTime( 200000L );
        }
    }
    else
    {
        // SUCCESS: steady RED with YELLOW blinking number of
        // times representing error code.
        //
        for ( ;; )
        {
            SetLED_On( LED_RED );
    
            for ( int i = 0; i < xsvfInfo.iErrorCode; i++ )
            {
                SetLED_On( LED_YELLOW_2 );
                waitTime( 300000L );
                SetLED_Off( LED_YELLOW_2 );
                waitTime( 200000L );
            }

            SetLED_Off( LED_RED );
            waitTime( 1000000L );
        }
    }

    return 0;
}


#include <stdio.h>
#include <string.h>

int main( int argc, char** argv ) 
{
    if ( argc < 3 )
    {
        fprintf( stderr, "Usage: xsvf2hex <hexfile> <xsvffile>\n" );
        return -1;
        }

    FILE* hexf = fopen( argv[1], "r+b" );
    FILE* xsvf = fopen( argv[2], "rb" );

    if ( ! hexf || ! xsvf )
        return -1;

    // search magic in hex file:
    char line[ 256 ];
    long addr = -1;
    fpos_t seek_pos = 0;
    int linec = 0;
    for( ;; )
    {
        fgetpos( hexf, &seek_pos ); ++linec;
        if ( ! fgets( line, sizeof( line ), hexf ) )
        {
            seek_pos = 0;
            break;
            }

        char* eol = strchr( line, '\r' );
        if ( eol ) *eol = 0;
        eol = strchr( line, '\n' );
        if ( eol ) *eol = 0;

        int cksum = 0;
        for ( unsigned i = 1; i < strlen( line ); i += 2 )
        {
            int byte = 0;
            if ( 1 == sscanf( line + i, "%2x", &byte ) )
                cksum += byte;
            }

        if ( ( cksum & 0xFF )!= 0 )
        {
            fprintf( stderr, "Bad checksum [%02x] at line (%d): %s\n", cksum, linec, line );
            return -1;
            }

        if ( strncmp( line, ":", 1 ) == 0 && strncmp( line + 7, "00", 2 ) == 0 )
        {
            sscanf( line + 3, "%04lx", &addr );

            int byte_count = 0;
            sscanf( line + 1, "%2x", &byte_count );
            addr += byte_count;
            }

        if ( strcmp( line, ":00000001FF" ) == 0 ) // EOF found
            break; // seek_pos will contain pointer to this record

        seek_pos = 0;
        }

    if ( ! seek_pos )
    {
        fprintf( stderr, "EOF record not found.\n" );
        return 0;
        }

    if ( addr % 16 ) // not aligned
        addr +=  16 - ( addr % 16 ); // align to next XXX0 boundary

    fprintf( stderr, "EOF found (line %d, seek pos %I64d): addr %04lx\n", linec, seek_pos, addr );
    fsetpos( hexf, &seek_pos ); // start overwriting magic

    for( ;; addr += 16 )
    {
        unsigned char binary[ 256 ];
        int len = fread( binary, 1, 16, xsvf );
        if ( len <= 0 )
            break;

        if ( addr >= 0x10000 ) // NOTE: This works just ONCE!
        {
            // write extended segment address
            addr -= 0x10000;
            fprintf( hexf, ":020000021000EC\r\n" );
            }

        // write data record
        int cksum = len + ( ( addr >> 8 ) & 0xFF ) + ( addr & 0xFF );
        fprintf( hexf, ":%02X%04lX00", len, ( addr & 0xFFFF ) );
        for ( int pos = 0; pos < len; pos++ )
        {
            fprintf( hexf, "%02X", binary[pos] & 0xFF );
            cksum += ( binary[pos] & 0xFF );
            }
        fprintf( hexf, "%02X\r\n", (-cksum)&0xFF );
        }

    fprintf( hexf, ":00000001FF\r\n" ); //  EOF record
    fclose( hexf );

    fclose( xsvf );
    return 0;
    }
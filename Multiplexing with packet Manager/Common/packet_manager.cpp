#include "packet_manager.h"
#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>
#define BUFSIZE     1024
int packet_add_data( char Buffer[], const char DataName[], const int Value )
{
    sprintf_s( Buffer, BUFSIZE, "%s %s=%d", Buffer, DataName, Value );
    return strlen(Buffer);
}

int packet_add_data( char Buffer[], const char DataName[], const char Value[] )
{
    sprintf_s( Buffer, BUFSIZE, "%s %s=\"%s\"", Buffer, DataName, Value );
    return strlen(Buffer);
}

int packet_parser_get_data( const char Packet[], const char DataName[], std::string &DataString )
{
    const char *Str;

    if( NULL == ( Str = strstr( Packet, DataName ) ) )
    {
        return 0;
    }

    int Len = strlen( Str );
    const char *Pos = Str;
    for( int i = 0; i < Len; ++i )
    {
        if( '=' == *( Pos + i ) )
        {
            for( int j = ( i + 1 ); j < Len; ++j )
            {
                if( ( ' ' == *( Pos + j ) ) || ( '\n' == *( Pos + j ) ) || ( '\0' == *( Pos + j ) ) )
                    break;
                DataString.push_back( *( Pos + j ) );
            }
            return atoi( DataString.c_str() );
        }
    }

    return 0;
}

int packet_parser_data( const char Packet[], const char DataName[] )
{
    std::string DataString;
    int ReturnLength;

    return packet_parser_get_data( Packet, DataName, DataString );
}

int packet_parser_data( const char Packet[], const char DataName[], char Buffer[], int &BufferSize )
{
    std::string DataString;
    int ReturnLength;

    ReturnLength = packet_parser_get_data( Packet, DataName, DataString );
    strcpy_s(Buffer, BUFSIZE, DataString.c_str());
    BufferSize = ReturnLength;

    return ReturnLength;
}

int packet_encode( char Packet[], const int MaxBufferSize, const char PacketID[], const char PacketData[], int PacketLength )
{
    int PacketLength2= strlen( PacketID ) + strlen( PacketData ) + 7;
    sprintf_s( Packet, BUFSIZE, "<%s %03d%s>", PacketID, PacketLength2, PacketData );
    return PacketLength2;
}

int packet_decode( const char Packet[], char PacketID[], char PacketData[] )
{
    int PacketDataLength;
    int PacketLength = strlen( Packet );
    int Pos = 1;

    for( int j = 0; Pos < PacketLength; ++Pos )
    {
        if( ' ' == Packet[Pos] )
        {
            PacketID[j] = '\0';
            ++Pos;
            break;
        }
        PacketID[j++] = Packet[Pos];
    }

    strcpy_s( PacketData, BUFSIZE, &Packet[Pos] );
    PacketDataLength = strlen( PacketData );

    return PacketDataLength;
}

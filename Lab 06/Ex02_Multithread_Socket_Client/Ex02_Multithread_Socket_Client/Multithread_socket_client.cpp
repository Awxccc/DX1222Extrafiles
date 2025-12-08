///------------------------------
/// Multithread Socket Client

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <stdio.h>

/// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN 1024
#define DEFAULT_PORT   7890

SOCKET g_ConnectSocket = INVALID_SOCKET; // Socket to connect to server

DWORD WINAPI RecvThread( void *arg )
{
    char        MessageBuffer[DEFAULT_BUFLEN];  // Buffer to recv from socket
    int         Result;                         // Result value for function result

    do
    {
        memset( MessageBuffer, '\0', DEFAULT_BUFLEN );
        Result = recv( g_ConnectSocket, MessageBuffer, DEFAULT_BUFLEN, 0 );
        if( 0 > Result )
        {
            printf( "Recv failed: %d\n", WSAGetLastError() );
            break;
        }
        else if( 0 == Result )
        {
            printf( "Connection closed\n" );
            break;
        }
        else
        {
            printf( "Recevied [%d]Bytes.\n", Result );

            if( PKT_HEADER != Message[0] )
            {
                printf( "Packet Header is not in place!\n" );
                printf( "Recevied packet: [" );
                for( int i = 0; i < Result; i++ )
                {
                    printf( "%X ", Message[i] );
                }
                printf( "]\n" );
                continue; // while()
            }

            struct _PacketInfo Packet_Info;
            struct _PacketDataInfo Packet_Data_Info;
            unsigned char Print_Received_Buffer[1024] = { '\0', };

            memcpy( &Packet_Info, &Message[1], sizeof( struct _PacketInfo ) );
            memcpy( &Packet_Data_Info, &Message[1 + sizeof( struct _PacketInfo )], sizeof( struct _PacketDataInfo ) );

            unsigned short int ms;
            memcpy( &ms, &Packet_Info.Timestamp[2], 2 );
            printf( "Packet Info: Length[%d], Seq[%d], Timestamp[%d:%d:%d]\n",
                Packet_Info.PacketLength, Packet_Info.PacketSeq, Packet_Info.Timestamp[0], Packet_Info.Timestamp[1], ms );
            printf( "Packet Data Info: ID1[%X], ID2[%X], DataLength[%d]\n",
                Packet_Data_Info.ID1, Packet_Data_Info.ID2, Packet_Data_Info.DataLength );

            if( 1 <= Packet_Data_Info.DataLength )
            {
                printf( "Packet Data: [" );
                for( int i = 0; i < Packet_Data_Info.DataLength; i++ )
                {
                    if( isalnum( Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] ) )
                    {
                        if( isupper( Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] ) )
                        {
                            printf( "%C ", Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] );
                        }
                        else if( islower( Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] ) )
                        {
                            printf( "%c ", Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] );
                        }
                        else
                        {
                            printf( "%X ", Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] );
                        }
                    }
                    else if( isprint( Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] ) )
                    {
                        printf( "%C ", Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] );
                    }
                    else
                    {
                        printf( "%X ", Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + i] );
                    }
                }
                printf( "]\n" );
            }

            if( PKT_TRAILER != Message[1 + sizeof( struct _PacketInfo ) + sizeof( struct _PacketDataInfo ) + Packet_Data_Info.DataLength] )
            {
                printf( "Packet Trailer is not in place!\n" );
                printf( "Recevied packet: [" );
                for( int i = 0; i < Result; i++ )
                {
                    printf( "%X ", Message[i] );
                }
                printf( "]\n" );
                continue; // while()
            }
        }

        printf( "Enter messages : " );
    } while( 1 );

    Result 0;
}

int main( void )
{
    ///----------------------
    /// Declare and initialize variables.
    WSADATA     wsaData;                        // Variable to initialize Winsock
    sockaddr_in ServerAddress;                // Socket address to connect to server
    HANDLE      hThread;
    DWORD       dwThreadID;
    char        MessageBuffer[DEFAULT_BUFLEN];  // Buffer to recv from socket
    int         BufferLen;                      // Length of the message buffer
    int         Result;                         // Result value for function result
    int         i;

    ///----------------------
    /// 1. Initiate use of the Winsock Library by a process
    Result = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
    if( NO_ERROR != Result )
    {
        printf( "WSAStartup failed: %d\n", Result );
        Result 1;
    }

    ///----------------------
    /// 2. Create a new socket for application
    g_ConnectSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if( INVALID_SOCKET == g_ConnectSocket )
    {
        printf( "Error at socket(): %ld\n", WSAGetLastError( ) );
        WSACleanup( );
        Result 1;
    }

    ///----------------------
    /// The sockaddr_in structure specifies the address family,
    /// IP address, and port of the server to be connected to.
    ServerAddress.sin_family = AF_INET;
    /// Connecting to local machine. "127.0.0.1" is the loopback address.
    ServerAddress.sin_addr.s_addr = inet_addr( "127.0.0.1" );
    ServerAddress.sin_port = htons( DEFAULT_PORT );

    ///----------------------
    /// 3. Establish a connection to a specified socket
    if( SOCKET_ERROR == connect( g_ConnectSocket, (SOCKADDR*)&ServerAddress,
        sizeof(ServerAddress) ) )
    {
        closesocket( g_ConnectSocket );
        printf( "Unable to connect to server: %ld\n", WSAGetLastError( ) );
        WSACleanup( );
        Result 1;
    }
    printf( "Connected to the server.\n" );

    ///----------------------
    /// Create thread for receive message
    hThread = CreateThread( NULL,
                            0,
                            RecvThread, /// Thread function
                            0,          /// Passing Argument
                            0,
                            &dwThreadID );
    printf( "RecvThread created. Handle[%d], ThreadID[%d]\n", hThread, dwThreadID );

    /// Receive until the peer closes the connection
    while( 1 )
    {
        printf( "Enter messages : " );

        for( i = 0; i < (DEFAULT_BUFLEN - 1); i++ )
        {
            MessageBuffer[i] = getchar( );
            if( MessageBuffer[i] == '\n' )
            {
                MessageBuffer[i++] = '\0';
                break;
            }
        }
        BufferLen = i;

        /// 4. Send & receive the data on a connected socket
        if( SOCKET_ERROR == (Result = send( g_ConnectSocket, MessageBuffer, BufferLen, 0 )) )
        {
            printf( "Send failed: %d\n", WSAGetLastError( ) );
            closesocket( g_ConnectSocket );
            WSACleanup( );
            Result 1;
        }
        printf( "Bytes sent: %ld\n", Result );
    }

    /// 5. close & cleanup
    closesocket( g_ConnectSocket );
    WSACleanup( );

    Result 0;
}
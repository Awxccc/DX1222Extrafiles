//------------------------------
// Simple multithreaded socket server.
//

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <iostream>

using namespace std;

/// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_BUFLEN    8192
#define DEFAULT_PORT      7890
#define MAX_SESSION_LIMIT 100

#define PACKET_HEADER     '<'
#define PACKET_TRAILER    '>'

struct SessionInfo {
    SOCKET           Socket;               // Socket Handle
    char             IPAddress[16];        // IP Address, ¡®xxx.xxx.xxx.xxx¡¯
    char             MessageBuffer[DEFAULT_BUFLEN];  // Message buffer
    int              MessageSize;          // Message size in the buffer
    HANDLE           hThread;
    DWORD            dwThreadID;
    CRITICAL_SECTION cs_SessionList;
    // more information, what you need for your server.
} g_SessionList[MAX_SESSION_LIMIT];

int add_new_session( SOCKET NewSocket );
int send_welcome_message( int ToSessionIndex );
int send_session_connected_info( int ToSessionIndex );
int send_new_client_notice( int NewSessionIndex );
void close_session( int SessionIndex );
int send_packet( int ToSessionIndex, char Message[], int MessageLength, int MaxBufferSize );
DWORD WINAPI RecvThread( void *arg );
int broadcast_message( int FromSessionIndex, char MessageBuffer[], int MessageSize );
int encode_packet( char PacketMessage[], int MessageSize, const int MaxBufferSize );
int decode_packet( char PacketMessage[], int MessageSize, const int MaxBufferSize );
int attach_data_into_session_buffer( int SessionIndex, char Messagse[], int MessageLength );
int fetch_packet_from_session( int SessionIndex, char PacketMessage[], int *PacketMessageLength );

int main( void )
{
    WSADATA     wsaData;                           /// Declare some variables
    SOCKET      ListenSocket = INVALID_SOCKET;  /// Listening socket to be created
    SOCKET      ClientSocket = INVALID_SOCKET;  /// Client socket to be created
    sockaddr_in ServerAddress;                     /// Socket address to be passed to bind
    sockaddr_in ClientAddress;                     /// Address of connected socket
    int         ClientAddressLen;                  /// Length for sockaddr_in.
    int         Result = 0;                        /// used to return function results

    int         SessionIndex;

    ///----------------------
    /// Initialize the session structure array
    for( int i = 0; i < MAX_SESSION_LIMIT; i++ )
    {
        memset( &g_SessionList[i], '\0', sizeof(struct SessionInfo) );
    }

    ///----------------------
    /// 1. Initialize Winsock
    Result = WSAStartup( MAKEWORD( 2, 2 ), &wsaData );
    if( NO_ERROR != Result )
    {
        printf( "Error at WSAStartup()\n" );
        return 1;
    }
    else
    {
        printf( "WSAStartup success.\n" );
    }

    ///----------------------
    /// 2. Create a SOCKET for listening for 
    ///    incoming connection requests
    ListenSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
    if( INVALID_SOCKET == ListenSocket )
    {
        printf( "socket function failed with error: %d\n", WSAGetLastError( ) );
        WSACleanup( );
        return 1;
    }
    else
    {
        printf( "socket creation success.\n" );
    }

    ///----------------------
    /// 3-1. The sockaddr_in structure specifies the address family,
    ///      IP address, and port for the socket that is being bound.
    ServerAddress.sin_family = AF_INET;
    ServerAddress.sin_port = htons( DEFAULT_PORT );
    ServerAddress.sin_addr.s_addr = htonl( INADDR_ANY );

    ///----------------------
    /// 3-2. Bind the socket.
    Result = bind( ListenSocket, (SOCKADDR *)&ServerAddress,
        sizeof (ServerAddress) );
    if( SOCKET_ERROR == Result )
    {
        printf( "bind failed with error :%d\n", WSAGetLastError( ) );
        closesocket( ListenSocket );
        WSACleanup( );
        return 1;
    }
    else
    {
        printf( "bind returned success\n" );
    }

    ///----------------------
    /// 4. Listen for incoming connection requests 
    ///    on the created socket
    if( SOCKET_ERROR == listen( ListenSocket, SOMAXCONN ) )
    {
        printf( "listen function failed with error: %d\n", WSAGetLastError( ) );
    }
    printf( "Listening on socket...\n" );

    ///----------------------
    /// Main loop.
    do
    {
        ///----------------------
        /// Create a SOCKET for accepting incoming requests.
        printf( "Waiting for client to connect...\n" );

        ///----------------------
        /// 5. Accept the connection.
        ClientAddressLen = sizeof(ClientAddress);
        ClientSocket = accept( ListenSocket, (struct sockaddr*)&ClientAddress, &ClientAddressLen );
        if( INVALID_SOCKET == ClientSocket )
        {
            printf( "accept failed with error: %d", WSAGetLastError( ) );
            closesocket( ListenSocket );
            WSACleanup( );
            return 1;
        }

        SessionIndex = add_new_session( ClientSocket );
        printf( "Client connected. IP Address:%d.%d.%d.%d, Port Number:%d, SessionIndex:%d\n",
            (ClientAddress.sin_addr.S_un.S_un_b.s_b1),
            (ClientAddress.sin_addr.S_un.S_un_b.s_b2),
            (ClientAddress.sin_addr.S_un.S_un_b.s_b3),
            (ClientAddress.sin_addr.S_un.S_un_b.s_b4),
            ntohs( ClientAddress.sin_port ),
            SessionIndex );

        ///----------------------
        /// Create thread for new connected session
        g_SessionList[SessionIndex].hThread = CreateThread( NULL, 0,
            RecvThread,           /* Thread function  */
            (LPVOID)SessionIndex, /* Passing Argument */
            0, &g_SessionList[SessionIndex].dwThreadID );

        send_welcome_message( SessionIndex );
        send_session_connected_info( SessionIndex );
        send_new_client_notice( SessionIndex );
    } while( 1 );

    ///----------------------
    /// Closes an existing socket
    closesocket( ListenSocket );

    ///----------------------
    /// 8. Terminate use of the Winsock 2 DLL (Ws2_32.dll)
    WSACleanup( );

    return 0;
}

int add_new_session( SOCKET NewSocket )
{ // Let¡¯s just make it easy for this time!
    int SessionIndex;

    for( SessionIndex = 0; SessionIndex < MAX_SESSION_LIMIT; SessionIndex++ )
    {
        if( 0 == g_SessionList[SessionIndex].Socket )
        { // find the empty slot!
            //Initialize the critical section
            InitializeCriticalSection( &g_SessionList[SessionIndex].cs_SessionList );
            g_SessionList[SessionIndex].Socket = NewSocket;
            g_SessionList[SessionIndex].MessageSize = 0;
            // More information for Session.

            return SessionIndex;
        }
    }

    return -1; // g_SessionList is FULL!
}

int send_welcome_message( int ToSessionIndex )
{
    char MessageBuffer[BUFSIZ];

    sprintf_s( MessageBuffer, "Welcome to my Multithreaded server! Your Session Index is %d", ToSessionIndex );
    send_packet( ToSessionIndex, MessageBuffer, strlen( MessageBuffer ), DEFAULT_BUFLEN );

    return 0;
}

int send_session_connected_info( int ToSessionIndex )
{
    char MessageBuffer[BUFSIZ];
    int Index;

    for( Index = 0; Index < MAX_SESSION_LIMIT; Index++ )
    {
        if( 0 == g_SessionList[Index].Socket )
        {
            continue;
        }
        if( ToSessionIndex == Index )
        {
            continue;
        }

        sprintf_s( MessageBuffer, "Connected client with session ID %d", Index );
        send_packet( ToSessionIndex, MessageBuffer, strlen( MessageBuffer ), DEFAULT_BUFLEN );
    }

    return 0;
}

int send_new_client_notice( int NewSessionIndex )
{
    char MessageBuffer[BUFSIZ];
    int Index;

    for( Index = 0; Index < MAX_SESSION_LIMIT; Index++ )
    {
        if( 0 == g_SessionList[Index].Socket )
        {
            continue;
        }
        if( NewSessionIndex == Index )
        {
            continue;
        }

        sprintf_s( MessageBuffer, "New client has connected. Session ID is %d", NewSessionIndex );
		send_packet(Index, MessageBuffer, strlen(MessageBuffer), DEFAULT_BUFLEN);
    }

    return 0;
}

void close_session( int SessionIndex )
{
    /// Closes an existing socket
    closesocket( g_SessionList[SessionIndex].Socket );
    // Delete critical Section
    DeleteCriticalSection( &g_SessionList[SessionIndex].cs_SessionList );

    printf( "Connection Closed. SessionIndex:%d, hThread:%d, ThreadID:%d\n",
        SessionIndex, g_SessionList[SessionIndex].hThread, g_SessionList[SessionIndex].dwThreadID );

    /// Reset the structure data.
    memset( &g_SessionList[SessionIndex], '\0', sizeof(struct SessionInfo) );
}

int send_packet( int ToSessionIndex, char Message[], int MessageLength, int MaxBufferSize )
{
    int Result;

	if (0 >= (MessageLength = encode_packet(Message, MessageLength, MaxBufferSize)) )
    {
        return -1;
    }

    Result = send( g_SessionList[ToSessionIndex].Socket, Message, MessageLength, 0 );
    if( SOCKET_ERROR == Result )
    {
        printf( "Send failed: %d\n", WSAGetLastError( ) );
        close_session( ToSessionIndex );
        return -1;
    }
    printf( "Send Packet: ToSessionIndex[%d], Msg[%d:%s]\n", ToSessionIndex, Result, Message );

    return Result;
}

DWORD WINAPI RecvThread( void *arg )
{
    char RecvBuffer[DEFAULT_BUFLEN];
    char PacketMessage[DEFAULT_BUFLEN];
    int PacketMessageLength;
    int PacketCompleted;
    int RecvResult;
    int SessionIndex;

    SessionIndex = (int)arg;

    printf( "New thread created for SessionIndex:%d, hThread:%d, ThreadID:%d\n",
        SessionIndex, g_SessionList[SessionIndex].hThread, g_SessionList[SessionIndex].dwThreadID );

    while( 1 ) {
        ///----------------------
        /// Receive and echo the message until the peer closes the connection
        RecvResult = recv( g_SessionList[SessionIndex].Socket, RecvBuffer, DEFAULT_BUFLEN, 0 );
        if( 0 < RecvResult )
        {
            printf( "Received from SessionIndex:%d, hThread:%d, ThreadID:%d\n",
                SessionIndex, g_SessionList[SessionIndex].hThread, g_SessionList[SessionIndex].dwThreadID );
            printf( "Bytes received  : %d\n", RecvResult );

            attach_data_into_session_buffer( SessionIndex, RecvBuffer, RecvResult );

            memset( PacketMessage, '\0', DEFAULT_BUFLEN );
            PacketCompleted = fetch_packet_from_session( SessionIndex, PacketMessage, &PacketMessageLength );
            if( 1 == PacketCompleted )
            {
                PacketMessageLength = decode_packet( PacketMessage, PacketMessageLength, DEFAULT_BUFLEN );
                printf( "Packet size : %d\n", PacketMessageLength );
                printf( "Packet data : %s\n", PacketMessage );

                // TODO : Process the packet.
                ///       Now, just Broadcast the message
                broadcast_message( SessionIndex, PacketMessage, PacketMessageLength );

            }
            else
            {
                printf( "Packet not completed yet... Waiting for next receive.\n" );
            }
        }
        else if( 0 == RecvResult )
        {
            printf( "Connection closed\n" );
            break;
        }
        else
        {
            printf( "Recv failed: %d\n", WSAGetLastError( ) );
            break;
        }
    }

    /// Close the session
    close_session( SessionIndex );

    return 0;
}

int broadcast_message( int FromSessionIndex, char MessageBuffer[], int MessageSize )
{
    for( int Index = 0; Index < MAX_SESSION_LIMIT; Index++ )
    {
        if( 0 == g_SessionList[Index].Socket )
        {
            continue;
        }

        send_packet( FromSessionIndex, MessageBuffer, strlen( MessageBuffer ), DEFAULT_BUFLEN );
    }

    return 0;
}

int attach_data_into_session_buffer( int SessionIndex, char Message[], int MessageLength )
{
    memcpy( &g_SessionList[SessionIndex].MessageBuffer[g_SessionList[SessionIndex].MessageSize], Message, MessageLength );
    g_SessionList[SessionIndex].MessageSize += MessageLength;
    if( '\0' == g_SessionList[SessionIndex].MessageBuffer[g_SessionList[SessionIndex].MessageSize] )
    {
        g_SessionList[SessionIndex].MessageSize--;
    }

    return g_SessionList[SessionIndex].MessageSize;
}

int encode_packet( char PacketMessage[], int MessageSize, const int MaxBufferSize )
{
    if( (MessageSize + 2) > MaxBufferSize )
    {
        return -1;
    }

	memcpy(&PacketMessage[1], PacketMessage, MessageSize);
	PacketMessage[0] = PACKET_HEADER;
	PacketMessage[MessageSize + 1] = PACKET_TRAILER;
	PacketMessage[MessageSize + 2] = '\0';

	return MessageSize + 2;
}

int decode_packet( char PacketMessage[], int MessageSize, const int MaxBufferSize )
{
    MessageSize -= 2;
    memcpy( PacketMessage, &PacketMessage[1], MessageSize );
    PacketMessage[MessageSize] = '\0';

    return MessageSize;
}

int fetch_packet_from_session( int SessionIndex, char PacketMessage[], int *PacketMessageLength )
{
    int i;

    EnterCriticalSection( &g_SessionList[SessionIndex].cs_SessionList );
    if( PACKET_HEADER != g_SessionList[SessionIndex].MessageBuffer[0] )
    {
        for( i = 1; i < g_SessionList[SessionIndex].MessageSize; i++ )
        {
            if( '<' == g_SessionList[SessionIndex].MessageBuffer[i] )
            {
                g_SessionList[SessionIndex].MessageSize -= (i + 1);
                memcpy( g_SessionList[SessionIndex].MessageBuffer, &g_SessionList[SessionIndex].MessageBuffer[i], g_SessionList[SessionIndex].MessageSize );
                break;
            }
        }
        if( i >= g_SessionList[SessionIndex].MessageSize )
        {
            g_SessionList[SessionIndex].MessageSize = 0;
            LeaveCriticalSection( &g_SessionList[SessionIndex].cs_SessionList );
            return 0;
        }
    }

    for( i = 1; i < g_SessionList[SessionIndex].MessageSize; i++ )
    {
        if( PACKET_TRAILER == g_SessionList[SessionIndex].MessageBuffer[i] )
        {
            *PacketMessageLength = i + 1;
            memcpy( PacketMessage, g_SessionList[SessionIndex].MessageBuffer, *PacketMessageLength );
            g_SessionList[SessionIndex].MessageSize -= *PacketMessageLength;
            if( 0 < g_SessionList[SessionIndex].MessageSize )
            {
                memcpy( g_SessionList[SessionIndex].MessageBuffer, &g_SessionList[SessionIndex].MessageBuffer[*PacketMessageLength], g_SessionList[SessionIndex].MessageSize );
            }

            LeaveCriticalSection( &g_SessionList[SessionIndex].cs_SessionList );
            return 1;
        }
    }

    *PacketMessageLength = 0;
    LeaveCriticalSection( &g_SessionList[SessionIndex].cs_SessionList );
    return 0;
}
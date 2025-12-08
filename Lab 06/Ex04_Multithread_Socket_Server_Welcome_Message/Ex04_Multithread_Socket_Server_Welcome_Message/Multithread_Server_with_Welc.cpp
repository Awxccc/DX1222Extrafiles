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
DWORD WINAPI RecvThread( void *arg );
int broadcast_message( int FromSessionIndex, char MessageBuffer[], int MessageSize );

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
    int Result;

    sprintf_s( MessageBuffer, "Welcome to my Multithread server! Your Session Index is %d", ToSessionIndex );
    Result = send( g_SessionList[ToSessionIndex].Socket, MessageBuffer, strlen( MessageBuffer ), 0 );
    if( SOCKET_ERROR == Result )
    {
        printf( "Send failed: %d\n", WSAGetLastError( ) );
        close_session( ToSessionIndex );
        return -1;
    }

    return 0;
}

int send_session_connected_info( int ToSessionIndex )
{
    char MessageBuffer[BUFSIZ];
    int Result;
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
        Result = send( g_SessionList[ToSessionIndex].Socket, MessageBuffer, strlen( MessageBuffer ), 0 );
        if( SOCKET_ERROR == Result )
        {
            printf( "Send failed: %d\n", WSAGetLastError( ) );
            close_session( ToSessionIndex );
            return -1;
        }
    }

    return 0;
}

int send_new_client_notice( int NewSessionIndex )
{
    char MessageBuffer[BUFSIZ];
    int Result;
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
        Result = send( g_SessionList[Index].Socket, MessageBuffer, strlen( MessageBuffer ), 0 );
        if( SOCKET_ERROR == Result )
        {
            printf( "Send failed: %d\n", WSAGetLastError( ) );
            close_session( NewSessionIndex );
            return -1;
        }
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

DWORD WINAPI RecvThread( void *arg )
{
    char RecvBuffer[DEFAULT_BUFLEN];
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
            // Copy the RecvBuffer into Session Buffer
            EnterCriticalSection( &g_SessionList[SessionIndex].cs_SessionList );
            memcpy( &g_SessionList[SessionIndex].MessageBuffer[0], &RecvBuffer, RecvResult );
            g_SessionList[SessionIndex].MessageSize = RecvResult;
            LeaveCriticalSection( &g_SessionList[SessionIndex].cs_SessionList );
            printf( "Received from SessionIndex:%d, hThread:%d, ThreadID:%d\n",
                SessionIndex, g_SessionList[SessionIndex].hThread, g_SessionList[SessionIndex].dwThreadID );
            printf( "Bytes received  : %d\n", g_SessionList[SessionIndex].MessageSize );
            printf( "Buffer received : %s\n", g_SessionList[SessionIndex].MessageBuffer );

            // TODO : Process the packet.
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

        /// Broadcast message
        broadcast_message( SessionIndex, g_SessionList[SessionIndex].MessageBuffer, g_SessionList[SessionIndex].MessageSize );
    }

    /// Close the session
    close_session( SessionIndex );

    return 0;
}

int broadcast_message( int FromSessionIndex, char MessageBuffer[], int MessageSize )
{
    int Result;

    for( int Index = 0; Index < MAX_SESSION_LIMIT; Index++ )
    {
        if( 0 == g_SessionList[Index].Socket )
        {
            continue;
        }

        Result = send( g_SessionList[Index].Socket, MessageBuffer, MessageSize, 0 );
        if( SOCKET_ERROR == Result )
        {
            printf( "Send failed: %d\n", WSAGetLastError( ) );
            close_session( FromSessionIndex );
        }
    }

    return 0;
}
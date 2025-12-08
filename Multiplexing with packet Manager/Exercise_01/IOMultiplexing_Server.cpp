#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include "../Common/packet_manager.h"

/// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define BUFSIZE     1024
#define PORT_NUMBER 7890

void send_welcome_message(SOCKET ClientSocket)
{
	char WelcomeMessage[100];
	int WelcomeMessageLength;

	sprintf_s(WelcomeMessage, "Welcome to my I/O multiplexing server! Please Login", ClientSocket);
	WelcomeMessageLength = strlen(WelcomeMessage);

	send(ClientSocket, WelcomeMessage, WelcomeMessageLength, 0);
}

int main(int argc, char **argv)
{
	int          Port = PORT_NUMBER;
	WSADATA      WsaData;
	SOCKET       ServerSocket;
	SOCKADDR_IN  ServerAddr;

	unsigned int Index;
	int          ClientLen = sizeof(SOCKADDR_IN);
	SOCKET       ClientSocket;
	SOCKADDR_IN  ClientAddr;

	fd_set       ReadFds, TempFds;
	TIMEVAL      Timeout; // struct timeval timeout;

	char         Message[BUFSIZE];
	char         SendMessage[BUFSIZE];
	char         SendMessageData[BUFSIZE];
	char         MessageData[BUFSIZE];
	char         PacketID[BUFSIZE];
	int          Return;

	if (2 == argc)
	{
		Port = atoi(argv[1]);
	}
	printf("Using port number : [%d]\n", Port);

	if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
	{
		printf("WSAStartup() error!\n");
		return 1;
	}

	ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ServerSocket)
	{
		printf("socket() error\n");
		return 1;
	}

	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(Port);
	ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (SOCKET_ERROR == bind(ServerSocket, (SOCKADDR *)&ServerAddr,
		sizeof(ServerAddr)))
	{
		printf("bind() error\n");
		return 1;
	}

	if (SOCKET_ERROR == listen(ServerSocket, 5))
	{
		printf("listen() error\n");
		return 1;
	}

	FD_ZERO(&ReadFds);
	FD_SET(ServerSocket, &ReadFds);
	memset(Message, '\0', BUFSIZE);
	memset(MessageData, '\0', BUFSIZE);
	memset(SendMessageData, '\0', BUFSIZE);
	memset(SendMessage, '\0', BUFSIZE);
	memset(PacketID, '\0', BUFSIZE);
	while (1)
	{
		TempFds = ReadFds;
		Timeout.tv_sec = 100;
		Timeout.tv_usec = 0;

		if (SOCKET_ERROR == (Return = select(0, &TempFds, 0, 0, &Timeout)))
		{ // Select() function returned error.
			printf("select() error\n");
			return 1;
		}
		if (0 == Return)
		{ // Select() function returned by timeout.
			printf("Select returned timeout.\n");
		}
		else if (0 > Return)
		{
			printf("Select returned error!\n");
		}
		else
		{
			for (Index = 0; Index < TempFds.fd_count; Index++)
			{
				if (TempFds.fd_array[Index] == ServerSocket)
				{ // New connection requested by new client.
					ClientSocket = accept(ServerSocket, (SOCKADDR *)&ClientAddr, &ClientLen);
					FD_SET(ClientSocket, &ReadFds);
					printf("New Client Accepted : Socket Handle [%d]\n", ClientSocket);
					send_welcome_message(ClientSocket);
				}
				else
				{ // Something to read from socket.
					Return = recv(TempFds.fd_array[Index], Message, BUFSIZE, 0);
					if (0 == Return)
					{ // Connection closed message has arrived.
						closesocket(TempFds.fd_array[Index]);
						printf("Connection closed :Socket Handle [%d]\n", TempFds.fd_array[Index]);
						FD_CLR(TempFds.fd_array[Index], &ReadFds);
					}
					else if (0 > Return)
					{ // recv() function returned error.
						closesocket(TempFds.fd_array[Index]);
						printf("Exceptional error :Socket Handle [%d]\n", TempFds.fd_array[Index]);
						FD_CLR(TempFds.fd_array[Index], &ReadFds);
					}
					else
					{ // Message recevied.

						// Note: in your own coding, you should check if this socket handler has logged in, if not, process login process
						// if logged in, process as message data
						printf("Message Received: %s\n", Message);
						packet_decode(Message, PacketID, MessageData);
						if (!strcmp(PacketID, "001"))
						{
							int buffersize;
							char login[BUFSIZE];
							char password[BUFSIZE];
							packet_parser_data(MessageData, "Login", login, buffersize);
							packet_parser_data(MessageData, "Password", password, buffersize);
							// ToDO: Check the login
							bool check = true;
							int MessageLen = 0;
							if (check) MessageLen = packet_add_data(SendMessageData, "LoginC", 1);
							else MessageLen = packet_add_data(SendMessageData, "LoginC", 0);
							MessageLen = packet_encode(SendMessage, BUFSIZE, "002", SendMessageData, MessageLen);
							send(TempFds.fd_array[Index], SendMessage, Return, 0);
							memset(SendMessage, '\0', BUFSIZE);
							memset(Message, '\0', BUFSIZE);
						}
						else {
							send(TempFds.fd_array[Index], Message, Return, 0);
						}
					}
				}
			}
		}
	}

	WSACleanup();

	return 0;
}

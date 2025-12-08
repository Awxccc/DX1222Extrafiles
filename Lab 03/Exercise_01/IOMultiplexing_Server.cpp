#include <stdio.h>
#include <string.h>
#include <winsock2.h>

/// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define BUFSIZE     1024
#define PORT_NUMBER 7890

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
						send(TempFds.fd_array[Index], Message, Return, 0);
					}
				}
			}
		}
	}

	WSACleanup();

	return 0;
}

#include <stdio.h>
#include <string.h>
#include <winsock2.h>
#include <conio.h>
#include "../Common/packet_manager.h"

/// Link with ws2_32.lib
#pragma comment(lib, "Ws2_32.lib")

#define BUFSIZE     1024
#define PORT_NUMBER 7890
#define IP_ADDRESS  "127.0.0.1"

int main(int argc, char **argv)
{
	int          Port = PORT_NUMBER;
	char         IPAddress[16] = IP_ADDRESS;
	WSADATA      WsaData;
	SOCKET       ConnectSocket;
	SOCKADDR_IN  ServerAddr;

	int          ClientLen = sizeof(SOCKADDR_IN);

	fd_set       ReadFds, TempFds;
	TIMEVAL      Timeout; // struct timeval timeout;

	char         Message[BUFSIZE];
	char         MessageData[BUFSIZE];
	char         PacketID[BUFSIZE];
	char         SendMessage[BUFSIZE];
	char         PacketMessage[BUFSIZE];
	char         MessageTemp[BUFSIZE];
	int          MessageLen;
	int          Return;

	printf("Destination IP Address [%s], Port number [%d]\n", IPAddress, Port);

	if (WSAStartup(MAKEWORD(2, 2), &WsaData) != 0)
	{
		printf("WSAStartup() error!");
		return 1;
	}

	ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (INVALID_SOCKET == ConnectSocket)
	{
		printf("socket() error");
		return 1;
	}

	///----------------------
	/// The sockaddr_in structure specifies the address family,
	/// IP address, and port of the server to be connected to.
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(Port);
	ServerAddr.sin_addr.s_addr = inet_addr(IPAddress);

	///----------------------
	/// Connect to server.
	Return = connect(ConnectSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
	if (Return == SOCKET_ERROR)
	{
		closesocket(ConnectSocket);
		printf("Unable to connect to server: %ld\n", WSAGetLastError());
		WSACleanup();
		return 1;
	}

	FD_ZERO(&ReadFds);
	FD_SET(ConnectSocket, &ReadFds);
	

	memset(Message, '\0', BUFSIZE);
	MessageLen = 0;
	int LoginStage = 0;

// handle welcome message
	while (1) {
		TempFds = ReadFds;
		Timeout.tv_sec = 0;
		Timeout.tv_usec = 1000;

		if (SOCKET_ERROR == (Return = select(0, &TempFds, 0, 0, &Timeout)))
		{ // Select() function returned error.
			closesocket(ConnectSocket);
			printf("select() error\n");
			return 1;
		}
		else if (0 > Return)
		{
			printf("Select returned error!\n");
		}
		else if (0 < Return)
		{
			memset(Message, '\0', BUFSIZE);
			printf("Select Processed... Something to read\n");
			Return = recv(ConnectSocket, Message, BUFSIZE, 0);
			if (0 > Return)
			{ // recv() function returned error.
				closesocket(ConnectSocket);
				printf("Exceptional error :Socket Handle [%d]\n", ConnectSocket);
				return 1;
			}
			else if (0 == Return)
			{ // Connection closed message has arrived.
				closesocket(ConnectSocket);
				printf("Connection closed :Socket Handle [%d]\n", ConnectSocket);
				return 0;
			}
			else
			{ // Message received.
				//printf("Bytes received   : %d\n", Return);
				printf("Message received : %s\n", Message);
				printf("Enter Login Name : ");
				break;
			}
		}
	}

	while (1) {
		memset(SendMessage, '\0', BUFSIZE);
		memset(PacketMessage, '\0', BUFSIZE);
		memset(MessageTemp, '\0', BUFSIZE);
		// this loop is only to handle login
		if (LoginStage == 3) break;
		if (LoginStage != 2) {
			while (1)
			{
				if (_kbhit())
				{ // To check keyboard input.
					MessageTemp[MessageLen] = _getch();
					if (('\n' == MessageTemp[MessageLen]) || ('\r' == MessageTemp[MessageLen]))
					{ // Send the message to server.
						putchar('\n');
						MessageTemp[MessageLen] = '\0';
						MessageLen++;

						if (LoginStage == 0) {
							packet_add_data(PacketMessage, "Login", MessageTemp);
							LoginStage++;
							printf("Enter Password : ");
							MessageLen = 0;
							memset(MessageTemp, '\0', BUFSIZE);
						}
						else {
							MessageLen = packet_add_data(PacketMessage, "Password", MessageTemp);
							break;
							memset(MessageTemp, '\0', BUFSIZE);
							LoginStage++;
						}
					}
					else
					{
						putchar(MessageTemp[MessageLen]);
						MessageLen++;
					}
				}
			}
			//encode and send
			MessageLen = packet_encode(SendMessage, BUFSIZE, "001", PacketMessage, MessageLen);
			Return = send(ConnectSocket, SendMessage, MessageLen, 0);
			if (Return == SOCKET_ERROR)
			{
				printf("send failed: %d\n", WSAGetLastError());
				closesocket(ConnectSocket);
				WSACleanup();
				return 1;
			}
			printf("Bytes Sent: %ld\n", Return);
			printf("Verifying Login\n", Return);
			LoginStage = 2;
		}
		else {
			while (1) {
				TempFds = ReadFds;
				Timeout.tv_sec = 0;
				Timeout.tv_usec = 1000;

				if (SOCKET_ERROR == (Return = select(0, &TempFds, 0, 0, &Timeout)))
				{ // Select() function returned error.
					closesocket(ConnectSocket);
					printf("select() error\n");
					return 1;
				}
				else if (0 > Return)
				{
					printf("Select returned error!\n");
				}
				else if (0 < Return)
				{
					memset(Message, '\0', BUFSIZE);
					printf("Select Processed... Something to read\n");
					Return = recv(ConnectSocket, Message, BUFSIZE, 0);
					if (0 > Return)
					{ // recv() function returned error.
						closesocket(ConnectSocket);
						printf("Exceptional error :Socket Handle [%d]\n", ConnectSocket);
						return 1;
					}
					else if (0 == Return)
					{ // Connection closed message has arrived.
						closesocket(ConnectSocket);
						printf("Connection closed :Socket Handle [%d]\n", ConnectSocket);
						return 0;
					}
					else
					{ // Message received.
						memset(PacketID, '\0', BUFSIZE);
						memset(MessageData, '\0', BUFSIZE);
						packet_decode(Message, PacketID, MessageData);
						if (!strcmp(PacketID, "002")) {
							int loginSuccess = packet_parser_data(MessageData, "LoginC");
							if (loginSuccess != 0) {
								printf("Login Success!\n");
								LoginStage = 3;
								break;
							}
							else {
								printf("Login Fail!\n");
								printf("Enter Login Name : ");
								LoginStage = 0;
							}
						}
						else {
							printf("Message received : %s\n", Message);
						}
					}
				}
			}
		}

	}

	//Should go into loop and wait for login success before entering message
	printf("enter messages : ");
	memset(Message, '\0', BUFSIZE);
	MessageLen = 0;


	while (1)
	{
		if (_kbhit())
		{ // To check keyboard input.
			Message[MessageLen] = _getch();
			if (('\n' == Message[MessageLen]) || ('\r' == Message[MessageLen]))
			{ // Send the message to server.
				putchar('\n');
				MessageLen++;
				Message[MessageLen] = '\0';

				Return = send(ConnectSocket, Message, MessageLen, 0);
				if (Return == SOCKET_ERROR)
				{
					printf("send failed: %d\n", WSAGetLastError());
					closesocket(ConnectSocket);
					WSACleanup();
					return 1;
				}
				printf("Bytes Sent: %ld\n", Return);

				MessageLen = 0;
				memset(Message, '\0', BUFSIZE);
			}
			else
			{
				putchar(Message[MessageLen]);
				MessageLen++;
			}
		}
		else
		{
			TempFds = ReadFds;
			Timeout.tv_sec = 0;
			Timeout.tv_usec = 1000;

			if (SOCKET_ERROR == (Return = select(0, &TempFds, 0, 0, &Timeout)))
			{ // Select() function returned error.
				closesocket(ConnectSocket);
				printf("select() error\n");
				return 1;
			}
			else if (0 > Return)
			{
				printf("Select returned error!\n");
			}
			else if (0 < Return)
			{
				memset(Message, '\0', BUFSIZE);
				printf("Select Processed... Something to read\n");
				Return = recv(ConnectSocket, Message, BUFSIZE, 0);
				if (0 > Return)
				{ // recv() function returned error.
					closesocket(ConnectSocket);
					printf("Exceptional error :Socket Handle [%d]\n", ConnectSocket);
					return 1;
				}
				else if (0 == Return)
				{ // Connection closed message has arrived.
					closesocket(ConnectSocket);
					printf("Connection closed :Socket Handle [%d]\n", ConnectSocket);
					return 0;
				}
				else
				{ // Message received.
					printf("Bytes received   : %d\n", Return);
					printf("Message received : %s\n", Message);
					printf("enter messages : ");
				}
			}
		}
	}

	closesocket(ConnectSocket);
	WSACleanup();
	return 0;
}

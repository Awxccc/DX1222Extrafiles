#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <stdio.h>
#include <conio.h>
#include "packet_manager.h"

#pragma comment(lib, "Ws2_32.lib")

#define PORT_NUMBER 7890
#define IP_ADDRESS "127.0.0.1"
#define BUFSIZE 1024

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    SOCKET ConnectSocket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(PORT_NUMBER);
    ServerAddr.sin_addr.s_addr = inet_addr(IP_ADDRESS);
    
    if (connect(ConnectSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR) {
        printf("Failed to connect.\n");
        return 1;
    }

    printf("Connected! Type '/help' for commands.\n> ");

    fd_set ReadFds;
    timeval Timeout;
    char RecvBuffer[BUFSIZE];
    char InputBuffer[BUFSIZE] = { 0 };
    int InputIndex = 0;

    while (1) {
        FD_ZERO(&ReadFds);
        FD_SET(ConnectSocket, &ReadFds);
        Timeout.tv_sec = 0; Timeout.tv_usec = 1000;

        // 1. Receive
        if (select(0, &ReadFds, 0, 0, &Timeout) > 0) {
            memset(RecvBuffer, 0, BUFSIZE);
            if (recv(ConnectSocket, RecvBuffer, BUFSIZE, 0) <= 0) break;

            char pid[10], pdata[BUFSIZE];
            packet_decode(RecvBuffer, pid, pdata);
            printf("\r%s\n> %s", pdata, InputBuffer); // Print over prompt
        }

        // 2. Input
        if (_kbhit()) {
            char ch = _getch();
            if (ch == '\r') { // Enter
                printf("\n");
                InputBuffer[InputIndex] = '\0';

                char packet[BUFSIZE];
                char* cmd = InputBuffer;

                if (strncmp(cmd, "/w ", 3) == 0) {
                    packet_encode(packet, BUFSIZE, "WHISP", cmd + 3); // Skip "/w "
                }
                else if (strcmp(cmd, "/list") == 0) {
                    packet_encode(packet, BUFSIZE, "LIST", "");
                }
                else if (strcmp(cmd, "/help") == 0) {
                    packet_encode(packet, BUFSIZE, "HELP", "");
                }
                else if (strcmp(cmd, "/quit") == 0) {
                    break;
                }
                else if (strncmp(cmd, "/createroom ", 12) == 0) {
                    packet_encode(packet, BUFSIZE, "CROOM", cmd + 12);
                }
                else if (strncmp(cmd, "/join ", 6) == 0) {
                    packet_encode(packet, BUFSIZE, "JOIN", cmd + 6);
                }
                else if (strncmp(cmd, "/addfriend ", 11) == 0) {
                    packet_encode(packet, BUFSIZE, "FRIEND", cmd + 11);
                }
                else if (strcmp(cmd, "/myfriends") == 0) {
                    packet_encode(packet, BUFSIZE, "MYFRIENDS", "");
                }
                else if (strncmp(cmd, "/kick ", 6) == 0) {
                    packet_encode(packet, BUFSIZE, "KICK", cmd + 6);
                }
                else if (strncmp(cmd, "/name ", 6) == 0) {
                    packet_encode(packet, BUFSIZE, "NAME", cmd + 6);
                }
                else if (strncmp(cmd, "/status ", 8) == 0) {
                    packet_encode(packet, BUFSIZE, "STAT", cmd + 8);
                }
                else if (strcmp(cmd, "/silence") == 0) {
                    packet_encode(packet, BUFSIZE, "SILENCE", "");
                }
                else if (strncmp(cmd, "/deletefriend ", 14) == 0) {
                    packet_encode(packet, BUFSIZE, "DELFRIEND", cmd + 14);
                }
                else if (strcmp(cmd, "/lock") == 0) {
                    packet_encode(packet, BUFSIZE, "LOCK", "");
                }
                else if (strncmp(cmd, "/invite ", 8) == 0) {
                    packet_encode(packet, BUFSIZE, "INVITE", cmd + 8);
                }
                else if (strncmp(cmd, "/yell ", 6) == 0) {
                    packet_encode(packet, BUFSIZE, "YELL", cmd + 6);
                }
                else if (strncmp(cmd, "/transfer ", 10) == 0) {
                    packet_encode(packet, BUFSIZE, "TRANSFER", cmd + 10);
                }
                else if (strncmp(cmd, "/block ", 7) == 0) {
                    packet_encode(packet, BUFSIZE, "BLOCK", cmd + 7);
                }
                else {
                    packet_encode(packet, BUFSIZE, "CHAT", cmd);
                }

                send(ConnectSocket, packet, strlen(packet), 0);

                InputIndex = 0;
                memset(InputBuffer, 0, BUFSIZE);
                printf("> ");
            }
            else if (ch == '\b') { // Backspace
                if (InputIndex > 0) {
                    InputBuffer[--InputIndex] = '\0';
                    printf("\b \b");
                }
            }
            else if (InputIndex < BUFSIZE - 1) {
                InputBuffer[InputIndex++] = ch;
                printf("%c", ch);
            }
        }
    }

    closesocket(ConnectSocket);
    WSACleanup();
    return 0;
}
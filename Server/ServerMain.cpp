#include <winsock2.h>
#include <stdio.h>
#include "packet_manager.h"

#pragma comment(lib, "Ws2_32.lib")

#define PORT_NUMBER 7890
#define BUFSIZE 1024
#define MAX_CLIENTS 10
#define MAX_ROOMS 5
#define MAX_FRIENDS 5

struct UserInfo {
    SOCKET socket;
    int sessionID;
    int roomID;         // 0 = Lobby
    bool isRoomMaster;
    bool active;
    int friends[MAX_FRIENDS];
    char username[20];
    char status[20];          // Stores "Online", "Away", "Busy"
    int blocked[MAX_FRIENDS]; // Stores IDs of blocked users
};

struct RoomInfo {
    int roomID;
    char roomName[50];
    char password[50];
    int masterSessionID;
    bool active;
    bool isSilenced;
    bool inviteOnly;
    int invitedUsers[10];
};

// Global Arrays
UserInfo g_Users[MAX_CLIENTS];
RoomInfo g_Rooms[MAX_ROOMS];
int g_NextSessionID = 1;

void InitGlobals() {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        g_Users[i].active = false;
        for (int f = 0; f < MAX_FRIENDS; f++) {
            g_Users[i].friends[f] = -1;
            g_Users[i].blocked[f] = -1; // Init Block List
        }
    }
    for (int i = 0; i < MAX_ROOMS; i++) {
        g_Rooms[i].active = false;
        for (int h = 0; h < 10; h++) g_Rooms[i].invitedUsers[h] = -1;
    }

    // Create Default Lobby
    g_Rooms[0].active = true;
    g_Rooms[0].roomID = 0;
    strcpy_s(g_Rooms[0].roomName, "Lobby");
    g_Rooms[0].password[0] = '\0';
    g_Rooms[0].masterSessionID = -1;
    g_Rooms[0].isSilenced = false;
}

int FindEmptyUserSlot() {
    for (int i = 0; i < MAX_CLIENTS; i++) if (!g_Users[i].active) return i;
    return -1;
}

int FindEmptyRoomSlot() {
    for (int i = 0; i < MAX_ROOMS; i++) if (!g_Rooms[i].active) return i;
    return -1;
}

UserInfo* GetUserBySocket(SOCKET s) {
    for (int i = 0; i < MAX_CLIENTS; i++) if (g_Users[i].active && g_Users[i].socket == s) return &g_Users[i];
    return NULL;
}

UserInfo* GetUserByID(int id) {
    for (int i = 0; i < MAX_CLIENTS; i++) if (g_Users[i].active && g_Users[i].sessionID == id) return &g_Users[i];
    return NULL;
}

RoomInfo* GetRoomByID(int id) {
    for (int i = 0; i < MAX_ROOMS; i++) if (g_Rooms[i].active && g_Rooms[i].roomID == id) return &g_Rooms[i];
    return NULL;
}

void SendPacket(SOCKET s, const char* packetID, const char* data) {
    char packet[BUFSIZE];
    packet_encode(packet, BUFSIZE, packetID, data);
    send(s, packet, strlen(packet), 0);
}

// UPDATED BROADCAST: Takes senderID (Pass -1 for System Messages)
void Broadcast(const char* message, int roomID, SOCKET excludeSocket, int senderID) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_Users[i].active && g_Users[i].roomID == roomID && g_Users[i].socket != excludeSocket) {

            bool isBlocked = false;
            if (senderID != -1) {
                for (int b = 0; b < MAX_FRIENDS; b++) {
                    if (g_Users[i].blocked[b] == senderID) {
                        isBlocked = true;
                        break;
                    }
                }
            }

            if (!isBlocked) {
                SendPacket(g_Users[i].socket, "MSG", message);
            }
        }
    }
}

int main() {
    InitGlobals();

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) return 1;

    SOCKET ServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    SOCKADDR_IN ServerAddr;
    ServerAddr.sin_family = AF_INET;
    ServerAddr.sin_port = htons(PORT_NUMBER);
    ServerAddr.sin_addr.s_addr = htonl(INADDR_ANY);

    bind(ServerSocket, (SOCKADDR*)&ServerAddr, sizeof(ServerAddr));
    listen(ServerSocket, 5);

    fd_set ReadFds, TempFds;
    FD_ZERO(&ReadFds);
    FD_SET(ServerSocket, &ReadFds);

    printf("Server Running on Port %d\n", PORT_NUMBER);

    while (1) {
        TempFds = ReadFds;
        timeval Timeout;
        Timeout.tv_sec = 0; Timeout.tv_usec = 100000;

        if (select(0, &TempFds, 0, 0, &Timeout) == SOCKET_ERROR) break;

        for (unsigned int i = 0; i < TempFds.fd_count; i++) {
            SOCKET currentSocket = TempFds.fd_array[i];

            // 1. Accept Connection
            if (currentSocket == ServerSocket) {
                SOCKADDR_IN ClientAddr;
                int ClientLen = sizeof(ClientAddr);
                SOCKET ClientSocket = accept(ServerSocket, (SOCKADDR*)&ClientAddr, &ClientLen);

                int slot = FindEmptyUserSlot();
                if (slot != -1) {
                    FD_SET(ClientSocket, &ReadFds);
                    g_Users[slot].active = true;
                    g_Users[slot].socket = ClientSocket;
                    g_Users[slot].sessionID = g_NextSessionID++;
                    sprintf_s(g_Users[slot].username, 20, "User_%d", g_Users[slot].sessionID);
                    strcpy_s(g_Users[slot].status, 20, "Online");
                    g_Users[slot].roomID = 0; // Join Lobby
                    g_Users[slot].isRoomMaster = false;

                    // Init Block List
                    for (int b = 0; b < MAX_FRIENDS; b++) g_Users[slot].blocked[b] = -1;

                    printf("User Connected: ID %d\n", g_Users[slot].sessionID);

                    // Req 3: Welcome Message
                    char msg[BUFSIZE];
                    sprintf_s(msg, BUFSIZE, "Welcome! Your ID is %d", g_Users[slot].sessionID);
                    SendPacket(ClientSocket, "SYS", msg);

                    sprintf_s(msg, BUFSIZE, "New Connection! ID is %d", g_Users[slot].sessionID);
                    // FIXED: Added -1 (System Message)
                    Broadcast(msg, 0, ClientSocket, -1);
                }
                else {
                    closesocket(ClientSocket);
                }
            }
            // 2. Handle Data
            else {
                char buffer[BUFSIZE];
                memset(buffer, 0, BUFSIZE);
                int bytes = recv(currentSocket, buffer, BUFSIZE, 0);

                UserInfo* user = GetUserBySocket(currentSocket);
                if (bytes <= 0 || user == NULL) {
                    closesocket(currentSocket);
                    FD_CLR(currentSocket, &ReadFds);
                    if (user) {
                        user->active = false;
                        char msg[BUFSIZE];
                        sprintf_s(msg, BUFSIZE, "User %d Left.", user->sessionID);
                        // FIXED: Added -1 (System Message)
                        Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                    }
                    continue;
                }

                char packetID[10];
                char packetData[BUFSIZE];
                packet_decode(buffer, packetID, packetData);

                // --- COMMANDS ---

                // CHAT (With Block Check)
                if (strcmp(packetID, "CHAT") == 0) {
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (room && room->isSilenced && !user->isRoomMaster) {
                        SendPacket(user->socket, "ERR", "The Room Master has silenced this room.");
                    }
                    else {
                        char msg[BUFSIZE];
                        sprintf_s(msg, BUFSIZE, "[%s]: %s", user->username, packetData);
                        // Pass user->sessionID so it can be blocked
                        Broadcast(msg, user->roomID, INVALID_SOCKET, user->sessionID);
                    }
                }
                // WHISPER (With Block Check & Auto-Reply)
                else if (strcmp(packetID, "WHISP") == 0) {
                    int targetID;
                    char text[BUFSIZE];
                    if (sscanf_s(packetData, "%d %[^\n]", &targetID, text, BUFSIZE) == 2) {
                        UserInfo* target = GetUserByID(targetID);
                        if (target) {
                            // Check Block
                            bool blocked = false;
                            for (int b = 0; b < MAX_FRIENDS; b++)
                                if (target->blocked[b] == user->sessionID) blocked = true;

                            if (!blocked) {
                                char msg[BUFSIZE];
                                sprintf_s(msg, BUFSIZE, "Whisper from %s: %s", user->username, text);
                                SendPacket(target->socket, "MSG", msg);

                                // AFK Auto-Reply
                                if (strcmp(target->status, "Away") == 0) {
                                    SendPacket(user->socket, "SYS", "[Auto-Reply] User is currently Away.");
                                }
                                else if (strcmp(target->status, "Busy") == 0) {
                                    SendPacket(user->socket, "SYS", "[Auto-Reply] User is currently Busy.");
                                }
                            }
                            else {
                                SendPacket(user->socket, "ERR", "You are blocked by this user.");
                            }
                        }
                        else {
                            SendPacket(user->socket, "ERR", "User not found");
                        }
                    }
                }
                // STAT (Strict Mode)
                else if (strcmp(packetID, "STAT") == 0) {
                    char newStatus[20];
                    if (sscanf_s(packetData, "%[^\n]", newStatus, 20) == 1) {
                        if (strcmp(newStatus, "Online") == 0 ||
                            strcmp(newStatus, "Away") == 0 ||
                            strcmp(newStatus, "Busy") == 0) {

                            strcpy_s(user->status, 20, newStatus);
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "Status updated to: %s", newStatus);
                            SendPacket(user->socket, "SYS", msg);
                        }
                        else {
                            SendPacket(user->socket, "ERR", "Invalid Status. Use: Online, Away, or Busy.");
                        }
                    }
                }
                // BLOCK
                else if (strcmp(packetID, "BLOCK") == 0) {
                    int targetID = atoi(packetData);
                    bool added = false;
                    for (int b = 0; b < MAX_FRIENDS; b++) {
                        if (user->blocked[b] == -1) {
                            user->blocked[b] = targetID;
                            added = true;
                            break;
                        }
                    }
                    if (added) SendPacket(user->socket, "SYS", "User Blocked.");
                    else SendPacket(user->socket, "ERR", "Block list full.");
                }
                // LIST
                else if (strcmp(packetID, "LIST") == 0) {
                    char listMsg[BUFSIZE] = "Users: ";
                    char temp[50];
                    for (int k = 0; k < MAX_CLIENTS; k++) {
                        if (g_Users[k].active) {
                            sprintf_s(temp, 50, "%s (%s), ", g_Users[k].username, g_Users[k].status);
                            strcat_s(listMsg, BUFSIZE, temp);
                        }
                    }
                    SendPacket(user->socket, "SYS", listMsg);
                }
                // FRIEND
                else if (strcmp(packetID, "FRIEND") == 0) {
                    int friendID = atoi(packetData);
                    if (GetUserByID(friendID)) {
                        for (int f = 0; f < MAX_FRIENDS; f++) {
                            if (user->friends[f] == -1) {
                                user->friends[f] = friendID;
                                SendPacket(user->socket, "SYS", "Friend Added");
                                break;
                            }
                        }
                    }
                }
                // MYFRIENDS
                else if (strcmp(packetID, "MYFRIENDS") == 0) {
                    char friendListMsg[BUFSIZE] = "Your Friends: ";
                    char temp[20];
                    bool found = false;
                    for (int f = 0; f < MAX_FRIENDS; f++) {
                        int fID = user->friends[f];
                        if (fID != -1) {
                            UserInfo* friendObj = GetUserByID(fID);
                            if (friendObj) sprintf_s(temp, 20, "%d(Online), ", fID);
                            else sprintf_s(temp, 20, "%d(Offline), ", fID);
                            strcat_s(friendListMsg, BUFSIZE, temp);
                            found = true;
                        }
                    }
                    if (!found) strcat_s(friendListMsg, BUFSIZE, "No friends added.");
                    SendPacket(user->socket, "SYS", friendListMsg);
                }
                // DELFRIEND
                else if (strcmp(packetID, "DELFRIEND") == 0) {
                    int targetID = atoi(packetData);
                    bool found = false;
                    for (int i = 0; i < MAX_FRIENDS; i++) {
                        if (user->friends[i] == targetID) {
                            user->friends[i] = -1;
                            found = true;
                            break;
                        }
                    }
                    if (found) SendPacket(user->socket, "SYS", "Friend deleted.");
                    else SendPacket(user->socket, "ERR", "Friend not found.");
                }
                // NAME
                else if (strcmp(packetID, "NAME") == 0) {
                    char newName[20];
                    if (sscanf_s(packetData, "%s", newName, 20) == 1) {
                        bool taken = false;
                        for (int k = 0; k < MAX_CLIENTS; k++) {
                            if (g_Users[k].active && strcmp(g_Users[k].username, newName) == 0) taken = true;
                        }
                        if (taken) SendPacket(user->socket, "ERR", "Name already taken.");
                        else {
                            char notify[BUFSIZE];
                            sprintf_s(notify, BUFSIZE, "%s changed name to %s", user->username, newName);
                            Broadcast(notify, user->roomID, INVALID_SOCKET, -1);
                            strcpy_s(user->username, 20, newName);
                            SendPacket(user->socket, "SYS", "Name changed successfully.");
                        }
                    }
                }
                // CREATE ROOM
                else if (strcmp(packetID, "CROOM") == 0) {
                    char name[50], pass[50];
                    if (sscanf_s(packetData, "%s %s", name, 50, pass, 50) == 2) {
                        int rSlot = FindEmptyRoomSlot();
                        if (rSlot != -1) {
                            g_Rooms[rSlot].active = true;
                            g_Rooms[rSlot].roomID = rSlot;
                            strcpy_s(g_Rooms[rSlot].roomName, 50, name);
                            strcpy_s(g_Rooms[rSlot].password, 50, pass);
                            g_Rooms[rSlot].masterSessionID = user->sessionID;
                            g_Rooms[rSlot].isSilenced = false;
                            for (int i = 0; i < 10; i++) g_Rooms[rSlot].invitedUsers[i] = -1;

                            user->roomID = rSlot;
                            user->isRoomMaster = true;
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "Room Created. ID: %d. You are Master.", rSlot);
                            SendPacket(user->socket, "SYS", msg);
                        }
                    }
                }
                // JOIN ROOM
                else if (strcmp(packetID, "JOIN") == 0) {
                    int rID;
                    char pass[50];
                    if (sscanf_s(packetData, "%d %s", &rID, pass, 50) == 2) {
                        RoomInfo* r = GetRoomByID(rID);
                        if (r == NULL) {
                            SendPacket(user->socket, "ERR", "Room does not exist.");
                        }
                        else {
                            if (r->inviteOnly) {
                                bool isInvited = false;
                                for (int i = 0; i < 10; i++) {
                                    if (r->invitedUsers[i] == user->sessionID) isInvited = true;
                                }
                                if (!isInvited) SendPacket(user->socket, "ERR", "Invite-Only.");
                                else if (strcmp(r->password, pass) == 0) {
                                    user->roomID = rID;
                                    SendPacket(user->socket, "SYS", "Joined Room.");
                                }
                                else SendPacket(user->socket, "ERR", "Wrong Password.");
                            }
                            else if (strcmp(r->password, pass) == 0) {
                                user->roomID = rID;
                                SendPacket(user->socket, "SYS", "Joined Room.");
                            }
                            else SendPacket(user->socket, "ERR", "Wrong Password.");
                        }
                    }
                }
                // KICK
                else if (strcmp(packetID, "KICK") == 0) {
                    int targetID = atoi(packetData);
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room && room->masterSessionID == user->sessionID) {
                        UserInfo* target = GetUserByID(targetID);
                        if (target && target->roomID == user->roomID) {
                            target->roomID = 0;
                            target->isRoomMaster = false;
                            SendPacket(target->socket, "SYS", "You have been kicked to the Lobby.");
                            SendPacket(user->socket, "SYS", "User kicked.");
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "User %d was kicked by Master.", targetID);
                            Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                        }
                        else SendPacket(user->socket, "ERR", "User not in your room.");
                    }
                    else SendPacket(user->socket, "ERR", "Only Room Master can kick.");
                }
                // SILENCE
                else if (strcmp(packetID, "SILENCE") == 0) {
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room) {
                        room->isSilenced = !room->isSilenced;
                        char msg[BUFSIZE];
                        if (room->isSilenced) sprintf_s(msg, BUFSIZE, "Room Silenced by Master.");
                        else sprintf_s(msg, BUFSIZE, "Room Unsilenced.");
                        Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                    }
                }
                // INVITE
                else if (strcmp(packetID, "INVITE") == 0) {
                    int targetID = atoi(packetData);
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room) {
                        for (int i = 0; i < 10; i++) {
                            if (room->invitedUsers[i] == -1) {
                                room->invitedUsers[i] = targetID;
                                break;
                            }
                        }
                        SendPacket(user->socket, "SYS", "User invited.");
                        UserInfo* target = GetUserByID(targetID);
                        if (target) {
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "Invited to Room %d ('%s')", room->roomID, room->roomName);
                            SendPacket(target->socket, "SYS", msg);
                        }
                    }
                }
                // LOCK
                else if (strcmp(packetID, "LOCK") == 0) {
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room) {
                        room->inviteOnly = !room->inviteOnly;
                        char msg[BUFSIZE];
                        sprintf_s(msg, BUFSIZE, "Room is now %s.", room->inviteOnly ? "Invite-Only" : "Public");
                        Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                        if (room->inviteOnly) for (int i = 0; i < 10; i++) room->invitedUsers[i] = -1;
                    }
                }
                // YELL
                else if (strcmp(packetID, "YELL") == 0) {
                    if (user->sessionID == 1) {
                        char msg[BUFSIZE];
                        sprintf_s(msg, BUFSIZE, "[GLOBAL ADMIN]: %s", packetData);
                        for (int i = 0; i < MAX_CLIENTS; i++) {
                            if (g_Users[i].active) SendPacket(g_Users[i].socket, "MSG", msg);
                        }
                    }
                    else SendPacket(user->socket, "ERR", "Only Admin (ID 1) can Yell.");
                }
                // TRANSFER
                else if (strcmp(packetID, "TRANSFER") == 0) {
                    int targetID = atoi(packetData);
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room && room->masterSessionID == user->sessionID) {
                        UserInfo* target = GetUserByID(targetID);
                        if (target && target->roomID == user->roomID) {
                            room->masterSessionID = targetID;
                            user->isRoomMaster = false;
                            target->isRoomMaster = true;
                            SendPacket(user->socket, "SYS", "You are no longer Master.");
                            SendPacket(target->socket, "SYS", "You are now Master!");
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "Master transferred to %s (ID:%d)", target->username, targetID);
                            Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                        }
                        else SendPacket(user->socket, "ERR", "User not found in room.");
                    }
                    else SendPacket(user->socket, "ERR", "You are not Master.");
                }
                // HELP
                else if (strcmp(packetID, "HELP") == 0) {
                    const char* help = "Cmds: /w, /list, /quit, /createroom, /join, /kick, /block, /status, /yell, /transfer";
                    SendPacket(user->socket, "SYS", help);
                }
            }
        }
    }

    WSACleanup();
    return 0;
}
#include <winsock2.h>
#include <stdio.h>
#include <ctype.h>
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
    int friendRequests[MAX_FRIENDS]; // Stores pending friend requests
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
            g_Users[i].friendRequests[f] = -1;
            g_Users[i].blocked[f] = -1;
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

// UPDATED BROADCAST: Blocks messages to "Busy" users
void Broadcast(const char* message, int roomID, SOCKET excludeSocket, int senderID) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_Users[i].active && g_Users[i].username[0] != '\0' && g_Users[i].roomID == roomID && g_Users[i].socket != excludeSocket) {

            // Check Busy Status (Only block User messages, allow System messages)
            if (senderID != -1 && strcmp(g_Users[i].status, "Busy") == 0) {
                continue;
            }

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

// Helper: Get User Count in Room
int GetUserCountInRoom(int roomID) {
    int count = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_Users[i].active && g_Users[i].username[0] != '\0' && g_Users[i].roomID == roomID) {
            count++;
        }
    }
    return count;
}

// Helper: Send Room List (Global)
void SendRoomList(SOCKET s) {
    char msg[BUFSIZE] = "\n--- Available Rooms ---\n";
    bool found = false;

    for (int i = 0; i < MAX_ROOMS; i++) {
        if (g_Rooms[i].active) {
            char type[25];
            if (g_Rooms[i].inviteOnly) strcpy_s(type, 25, "Invite-Only");
            else if (strlen(g_Rooms[i].password) > 0) strcpy_s(type, 25, "Password Protected");
            else strcpy_s(type, 25, "Public");

            int count = GetUserCountInRoom(g_Rooms[i].roomID);

            char line[150];
            sprintf_s(line, 150, "ID: %d | Name: %s | Type: %s | Users: %d\n",
                g_Rooms[i].roomID, g_Rooms[i].roomName, type, count);

            strcat_s(msg, BUFSIZE, line);
            found = true;
        }
    }
    if (!found) strcat_s(msg, BUFSIZE, "No active rooms found.\n");
    strcat_s(msg, BUFSIZE, "-----------------------\n");

    SendPacket(s, "SYS", msg);
}

// --- Helper to Find User by Name or ID ---
UserInfo* FindUserByNameOrID(char* input) {
    if (input == NULL || strlen(input) == 0) return NULL;
    if (isdigit(input[0])) {
        int id = atoi(input);
        return GetUserByID(id);
    }
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (g_Users[i].active && strcmp(g_Users[i].username, input) == 0) {
            return &g_Users[i];
        }
    }
    return NULL;
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

                    g_Users[slot].username[0] = '\0';
                    strcpy_s(g_Users[slot].status, 20, "Online");
                    g_Users[slot].roomID = 0; // Join Lobby
                    g_Users[slot].isRoomMaster = false;

                    for (int b = 0; b < MAX_FRIENDS; b++) {
                        g_Users[slot].blocked[b] = -1;
                        g_Users[slot].friendRequests[b] = -1;
                        g_Users[slot].friends[b] = -1;
                    }

                    printf("Connection Accepted: Socket %d (Waiting for Login)\n", (int)ClientSocket);
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
                        if (user->username[0] != '\0') {
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "User %s Left.", user->username);
                            Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                        }
                        user->active = false;
                        user->username[0] = '\0';
                    }
                    continue;
                }

                char packetID[10];
                char packetData[BUFSIZE];
                packet_decode(buffer, packetID, packetData);

                // --- LOGIN HANDLER ---
                if (user->username[0] == '\0') {
                    if (strcmp(packetID, "LOGIN") == 0) {
                        if (isdigit(packetData[0])) {
                            SendPacket(user->socket, "ERR", "Username cannot start with a number.");
                            closesocket(currentSocket);
                            FD_CLR(currentSocket, &ReadFds);
                            user->active = false;
                            continue;
                        }

                        bool taken = false;
                        for (int k = 0; k < MAX_CLIENTS; k++) {
                            if (g_Users[k].active && strcmp(g_Users[k].username, packetData) == 0) {
                                taken = true;
                                break;
                            }
                        }

                        if (taken) {
                            SendPacket(user->socket, "ERR", "Name already taken. Please reconnect with a different name.");
                            closesocket(currentSocket);
                            FD_CLR(currentSocket, &ReadFds);
                            user->active = false;
                        }
                        else {
                            strcpy_s(user->username, 20, packetData);
                            printf("User Logged In: %s (ID %d)\n", user->username, user->sessionID);

                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "Welcome %s! Your ID is %d. Type /help to see cmds", user->username, user->sessionID);
                            SendPacket(user->socket, "SYS", msg);
                            SendRoomList(user->socket);

                            sprintf_s(msg, BUFSIZE, "New User Connected: %s (ID %d)", user->username, user->sessionID);
                            Broadcast(msg, 0, user->socket, -1);
                        }
                    }
                    else {
                        SendPacket(user->socket, "ERR", "Please login first.");
                        closesocket(currentSocket);
                        FD_CLR(currentSocket, &ReadFds);
                        user->active = false;
                    }
                    continue;
                }

                // --- COMMANDS ---

                // CHAT 
                if (strcmp(packetID, "CHAT") == 0) {
                    if (user->roomID == 0) {
                        SendPacket(user->socket, "ERR", "You are in the Lobby. Please use /join or /createroom to chat.");
                    }
                    else {
                        RoomInfo* room = GetRoomByID(user->roomID);
                        if (room && room->isSilenced && !user->isRoomMaster) {
                            SendPacket(user->socket, "ERR", "The Room Master has silenced this room.");
                        }
                        else {
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "[%s]: %s", user->username, packetData);
                            Broadcast(msg, user->roomID, INVALID_SOCKET, user->sessionID);
                        }
                    }
                }
                // WHISPER (Updated with Busy Check)
                else if (strcmp(packetID, "WHISP") == 0) {
                    char targetStr[50];
                    char text[BUFSIZE];

                    if (sscanf_s(packetData, "%s %[^\n]", targetStr, 50, text, BUFSIZE) == 2) {
                        UserInfo* target = FindUserByNameOrID(targetStr);
                        if (target) {
                            // Check Busy
                            if (strcmp(target->status, "Busy") == 0) {
                                SendPacket(user->socket, "ERR", "User is Busy and not accepting messages.");
                            }
                            else {
                                bool blocked = false;
                                for (int b = 0; b < MAX_FRIENDS; b++)
                                    if (target->blocked[b] == user->sessionID) blocked = true;

                                if (!blocked) {
                                    char msg[BUFSIZE];
                                    sprintf_s(msg, BUFSIZE, "Whisper from %s: %s", user->username, text);
                                    SendPacket(target->socket, "MSG", msg);

                                    if (strcmp(target->status, "Away") == 0)
                                        SendPacket(user->socket, "SYS", "[Auto-Reply] User is Away.");
                                }
                                else SendPacket(user->socket, "ERR", "You are blocked by this user.");
                            }
                        }
                        else SendPacket(user->socket, "ERR", "User not found");
                    }
                }
                // BLOCK
                else if (strcmp(packetID, "BLOCK") == 0) {
                    UserInfo* target = FindUserByNameOrID(packetData);
                    if (target) {
                        if (target->sessionID == user->sessionID) SendPacket(user->socket, "ERR", "Cannot block yourself.");
                        else {
                            bool added = false;
                            for (int b = 0; b < MAX_FRIENDS; b++) {
                                if (user->blocked[b] == -1) {
                                    user->blocked[b] = target->sessionID;
                                    added = true;
                                    break;
                                }
                            }
                            if (added) SendPacket(user->socket, "SYS", "User Blocked.");
                            else SendPacket(user->socket, "ERR", "Block list full.");
                        }
                    }
                    else SendPacket(user->socket, "ERR", "User not found.");
                }
                // LIST (Global)
                else if (strcmp(packetID, "LIST") == 0) {
                    char listMsg[BUFSIZE] = "Users: ";
                    char temp[50];
                    for (int k = 0; k < MAX_CLIENTS; k++) {
                        if (g_Users[k].active && g_Users[k].username[0] != '\0') {
                            sprintf_s(temp, 50, "%s (%s), ", g_Users[k].username, g_Users[k].status);
                            strcat_s(listMsg, BUFSIZE, temp);
                        }
                    }
                    int len = strlen(listMsg);
                    if (len > 2 && listMsg[len - 2] == ',') listMsg[len - 2] = '\0';
                    SendPacket(user->socket, "SYS", listMsg);
                }
                // ROOMLIST
                else if (strcmp(packetID, "ROOMLIST") == 0) {
                    char roomName[50] = "Lobby";
                    if (user->roomID != 0) {
                        RoomInfo* r = GetRoomByID(user->roomID);
                        if (r) strcpy_s(roomName, 50, r->roomName);
                    }
                    char listMsg[BUFSIZE];
                    sprintf_s(listMsg, BUFSIZE, "Users in %s: ", roomName);
                    char temp[50];
                    bool found = false;
                    for (int k = 0; k < MAX_CLIENTS; k++) {
                        if (g_Users[k].active && g_Users[k].username[0] != '\0' && g_Users[k].roomID == user->roomID) {
                            sprintf_s(temp, 50, "%s, ", g_Users[k].username);
                            strcat_s(listMsg, BUFSIZE, temp);
                            found = true;
                        }
                    }
                    int len = strlen(listMsg);
                    if (len > 2 && listMsg[len - 2] == ',') listMsg[len - 2] = '\0';
                    SendPacket(user->socket, "SYS", listMsg);
                }
                // LEAVEROOM
                else if (strcmp(packetID, "LEAVEROOM") == 0) {
                    if (user->roomID == 0) {
                        SendPacket(user->socket, "ERR", "You are already in the Lobby.");
                    }
                    else {
                        RoomInfo* r = GetRoomByID(user->roomID);
                        if (user->isRoomMaster && r) {
                            int oldRoomID = user->roomID;
                            r->active = false;
                            for (int k = 0; k < MAX_CLIENTS; k++) {
                                if (g_Users[k].active && g_Users[k].roomID == oldRoomID) {
                                    g_Users[k].roomID = 0;
                                    g_Users[k].isRoomMaster = false;
                                    SendPacket(g_Users[k].socket, "SYS", "Room Closed by Master. Returned to Lobby.");
                                }
                            }
                        }
                        else {
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "%s left the room.", user->username);
                            Broadcast(msg, user->roomID, user->socket, -1);
                            user->roomID = 0;
                            user->isRoomMaster = false;
                            SendPacket(user->socket, "SYS", "Returned to Lobby.");
                        }
                    }
                }
                // ROOMS
                else if (strcmp(packetID, "ROOMS") == 0) {
                    SendRoomList(user->socket);
                }
                // FRIEND
                else if (strcmp(packetID, "FRIEND") == 0) {
                    UserInfo* target = FindUserByNameOrID(packetData);
                    if (!target) SendPacket(user->socket, "ERR", "User not found.");
                    else if (target->sessionID == user->sessionID) SendPacket(user->socket, "ERR", "Cannot add yourself.");
                    else {
                        bool alreadyFriends = false;
                        for (int f = 0; f < MAX_FRIENDS; f++) if (user->friends[f] == target->sessionID) alreadyFriends = true;

                        if (alreadyFriends) SendPacket(user->socket, "ERR", "Already friends.");
                        else {
                            bool sent = false;
                            for (int r = 0; r < MAX_FRIENDS; r++) {
                                if (target->friendRequests[r] == -1) {
                                    target->friendRequests[r] = user->sessionID;
                                    sent = true;
                                    break;
                                }
                                else if (target->friendRequests[r] == user->sessionID) {
                                    sent = true; break;
                                }
                            }
                            if (sent) {
                                SendPacket(user->socket, "SYS", "Friend Request Sent.");
                                char note[BUFSIZE];
                                sprintf_s(note, BUFSIZE, "Friend Request from %s. Type /accept %s to add.", user->username, user->username);
                                SendPacket(target->socket, "SYS", note);
                            }
                            else SendPacket(user->socket, "ERR", "User's request list is full.");
                        }
                    }
                }
                // ACCEPT
                else if (strcmp(packetID, "ACCEPT") == 0) {
                    UserInfo* requester = FindUserByNameOrID(packetData);
                    if (!requester) SendPacket(user->socket, "ERR", "User not found.");
                    else {
                        int reqIndex = -1;
                        for (int r = 0; r < MAX_FRIENDS; r++) {
                            if (user->friendRequests[r] == requester->sessionID) {
                                reqIndex = r;
                                break;
                            }
                        }
                        if (reqIndex != -1) {
                            bool addedMe = false;
                            for (int f = 0; f < MAX_FRIENDS; f++) { if (user->friends[f] == -1) { user->friends[f] = requester->sessionID; addedMe = true; break; } }
                            bool addedThem = false;
                            for (int f = 0; f < MAX_FRIENDS; f++) { if (requester->friends[f] == -1) { requester->friends[f] = user->sessionID; addedThem = true; break; } }

                            if (addedMe && addedThem) {
                                user->friendRequests[reqIndex] = -1;
                                SendPacket(user->socket, "SYS", "Friend Added!");
                                char note[BUFSIZE];
                                sprintf_s(note, BUFSIZE, "%s accepted your friend request!", user->username);
                                SendPacket(requester->socket, "SYS", note);
                            }
                            else SendPacket(user->socket, "ERR", "One of your friend lists is full.");
                        }
                        else SendPacket(user->socket, "ERR", "No friend request from this user.");
                    }
                }
                // MYFRIENDS
                else if (strcmp(packetID, "MYFRIENDS") == 0) {
                    char friendListMsg[BUFSIZE] = "Your Friends: ";
                    char temp[50];
                    bool found = false;
                    int onlineCount = 0;
                    for (int f = 0; f < MAX_FRIENDS; f++) {
                        int fID = user->friends[f];
                        if (fID != -1) {
                            UserInfo* friendObj = GetUserByID(fID);
                            if (friendObj) { sprintf_s(temp, 50, "%s(Online), ", friendObj->username); onlineCount++; }
                            else sprintf_s(temp, 50, "%d(Offline), ", fID);
                            strcat_s(friendListMsg, BUFSIZE, temp);
                            found = true;
                        }
                    }
                    if (!found) strcat_s(friendListMsg, BUFSIZE, "No friends added.");
                    else {
                        int len = strlen(friendListMsg);
                        if (len > 2 && friendListMsg[len - 2] == ',') friendListMsg[len - 2] = '\0';
                        char countMsg[50];
                        sprintf_s(countMsg, 50, ". Connected: %d", onlineCount);
                        strcat_s(friendListMsg, BUFSIZE, countMsg);
                    }
                    SendPacket(user->socket, "SYS", friendListMsg);
                }
                // DELFRIEND
                else if (strcmp(packetID, "DELFRIEND") == 0) {
                    UserInfo* target = FindUserByNameOrID(packetData);
                    if (target) {
                        int targetID = target->sessionID;
                        bool found = false;
                        for (int i = 0; i < MAX_FRIENDS; i++) {
                            if (user->friends[i] == targetID) {
                                user->friends[i] = -1;
                                found = true;
                                for (int k = 0; k < MAX_FRIENDS; k++) if (target->friends[k] == user->sessionID) target->friends[k] = -1;
                                break;
                            }
                        }
                        if (found) SendPacket(user->socket, "SYS", "Friend deleted.");
                        else SendPacket(user->socket, "ERR", "Friend not found in your list.");
                    }
                    else SendPacket(user->socket, "ERR", "User not found.");
                }
                // NAME
                else if (strcmp(packetID, "NAME") == 0) {
                    char newName[20];
                    if (sscanf_s(packetData, "%s", newName, 20) == 1) {
                        if (isdigit(newName[0])) SendPacket(user->socket, "ERR", "Name cannot start with a number.");
                        else {
                            bool taken = false;
                            for (int k = 0; k < MAX_CLIENTS; k++) if (g_Users[k].active && strcmp(g_Users[k].username, newName) == 0) taken = true;
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
                }
                // CREATE ROOM
                else if (strcmp(packetID, "CROOM") == 0) {
                    char name[50]; char pass[50] = "";
                    int args = sscanf_s(packetData, "%s %s", name, 50, pass, 50);
                    if (args >= 1) {
                        if (args == 1) pass[0] = '\0';
                        bool taken = false;
                        for (int i = 0; i < MAX_ROOMS; i++) if (g_Rooms[i].active && strcmp(g_Rooms[i].roomName, name) == 0) taken = true;
                        if (taken) SendPacket(user->socket, "ERR", "Room name taken. Try again.");
                        else {
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
                            else SendPacket(user->socket, "ERR", "Max rooms reached.");
                        }
                    }
                }
                // JOIN ROOM
                else if (strcmp(packetID, "JOIN") == 0) {
                    char target[50]; char pass[50] = "";
                    int args = sscanf_s(packetData, "%s %s", target, 50, pass, 50);
                    if (args >= 1) {
                        if (args == 1) pass[0] = '\0';
                        RoomInfo* r = NULL;
                        bool isNum = true;
                        for (int i = 0; target[i] != '\0'; i++) if (!isdigit(target[i])) { isNum = false; break; }
                        if (isNum) r = GetRoomByID(atoi(target));
                        if (r == NULL) for (int i = 0; i < MAX_ROOMS; i++) if (g_Rooms[i].active && strcmp(g_Rooms[i].roomName, target) == 0) { r = &g_Rooms[i]; break; }

                        if (r == NULL) SendPacket(user->socket, "ERR", "Room not found.");
                        else {
                            bool authorized = false;
                            int inviteSlot = -1;

                            if (r->inviteOnly) {
                                bool isInvited = false;
                                for (int i = 0; i < 10; i++) {
                                    if (r->invitedUsers[i] == user->sessionID) {
                                        isInvited = true;
                                        inviteSlot = i;
                                        break;
                                    }
                                }
                                if (isInvited) authorized = true;
                                else SendPacket(user->socket, "ERR", "This room is Invite-Only.");
                            }
                            else {
                                if (strlen(r->password) == 0) authorized = true;
                                else {
                                    if (strcmp(r->password, pass) == 0) authorized = true;
                                    else SendPacket(user->socket, "ERR", "Wrong Password.");
                                }
                            }

                            if (authorized) {
                                if (inviteSlot != -1) r->invitedUsers[inviteSlot] = -1;

                                user->roomID = r->roomID;
                                user->isRoomMaster = false;
                                SendPacket(user->socket, "SYS", "Joined Room.");
                                char broadcastMsg[BUFSIZE];
                                sprintf_s(broadcastMsg, BUFSIZE, "%s joined the room.", user->username);
                                Broadcast(broadcastMsg, user->roomID, INVALID_SOCKET, -1);
                            }
                        }
                    }
                }
                // KICK
                else if (strcmp(packetID, "KICK") == 0) {
                    UserInfo* target = FindUserByNameOrID(packetData);
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room && room->masterSessionID == user->sessionID) {
                        if (target) {
                            if (target->sessionID == user->sessionID) SendPacket(user->socket, "ERR", "You cannot kick yourself.");
                            else if (target->roomID == user->roomID) {
                                target->roomID = 0;
                                target->isRoomMaster = false;
                                SendPacket(target->socket, "SYS", "You have been kicked to the Lobby.");
                                SendPacket(user->socket, "SYS", "User kicked.");
                                char msg[BUFSIZE];
                                sprintf_s(msg, BUFSIZE, "User %s (ID %d) was kicked by Master.", target->username, target->sessionID);
                                Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                            }
                            else SendPacket(user->socket, "ERR", "User not in your room.");
                        }
                        else SendPacket(user->socket, "ERR", "User not found.");
                    }
                    else SendPacket(user->socket, "ERR", "Only Room Master can kick.");
                }
                // SILENCE
                else if (strcmp(packetID, "SILENCE") == 0) {
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room) {
                        room->isSilenced = !room->isSilenced;
                        char msg[BUFSIZE];
                        sprintf_s(msg, BUFSIZE, "Room %s.", room->isSilenced ? "Silenced" : "Unsilenced");
                        Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                    }
                }
                // INVITE
                else if (strcmp(packetID, "INVITE") == 0) {
                    UserInfo* target = FindUserByNameOrID(packetData);
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room) {
                        if (target) {
                            for (int i = 0; i < 10; i++) if (room->invitedUsers[i] == -1) { room->invitedUsers[i] = target->sessionID; break; }
                            SendPacket(user->socket, "SYS", "User invited.");
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "Invited to Room %d ('%s')", room->roomID, room->roomName);
                            SendPacket(target->socket, "SYS", msg);
                        }
                        else SendPacket(user->socket, "ERR", "User not found.");
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
                        for (int i = 0; i < MAX_CLIENTS; i++) if (g_Users[i].active) SendPacket(g_Users[i].socket, "MSG", msg);
                    }
                    else SendPacket(user->socket, "ERR", "Only Admin (ID 1) can Yell.");
                }
                // TRANSFER
                else if (strcmp(packetID, "TRANSFER") == 0) {
                    UserInfo* target = FindUserByNameOrID(packetData);
                    RoomInfo* room = GetRoomByID(user->roomID);
                    if (user->isRoomMaster && room && room->masterSessionID == user->sessionID) {
                        if (target && target->roomID == user->roomID) {
                            room->masterSessionID = target->sessionID;
                            user->isRoomMaster = false;
                            target->isRoomMaster = true;
                            SendPacket(user->socket, "SYS", "You are no longer Master.");
                            SendPacket(target->socket, "SYS", "You are now Master!");
                            char msg[BUFSIZE];
                            sprintf_s(msg, BUFSIZE, "Master transferred to %s (ID:%d)", target->username, target->sessionID);
                            Broadcast(msg, user->roomID, INVALID_SOCKET, -1);
                        }
                        else SendPacket(user->socket, "ERR", "User not found in room.");
                    }
                    else SendPacket(user->socket, "ERR", "You are not Master.");
                }
                // GET STATUS
                else if (strcmp(packetID, "GETSTAT") == 0) {
                    char msg[BUFSIZE];
                    sprintf_s(msg, BUFSIZE, "Current Status: %s", user->status);
                    SendPacket(user->socket, "SYS", msg);
                }
                // CHANGE STATUS - Updated to be case-insensitive
                else if (strcmp(packetID, "CSTAT") == 0) {
                    char newStatus[20];
                    if (sscanf_s(packetData, "%[^\n]", newStatus, 20) == 1) {
                        // Create a lowercase copy for comparison
                        char lower[20];
                        strcpy_s(lower, 20, newStatus);
                        for (int i = 0; lower[i]; i++) lower[i] = tolower(lower[i]);

                        if (strcmp(lower, "online") == 0) {
                            strcpy_s(user->status, 20, "Online");
                            SendPacket(user->socket, "SYS", "Status updated to Online.");
                        }
                        else if (strcmp(lower, "away") == 0) {
                            strcpy_s(user->status, 20, "Away");
                            SendPacket(user->socket, "SYS", "Status updated to Away.");
                        }
                        else if (strcmp(lower, "busy") == 0) {
                            strcpy_s(user->status, 20, "Busy");
                            SendPacket(user->socket, "SYS", "Status updated to Busy.");
                        }
                        else {
                            SendPacket(user->socket, "ERR", "Invalid Status. Use: Online, Away, Busy.");
                        }
                    }
                }
                // HELP (UPDATED TO REMOVE < >)
                else if (strcmp(packetID, "HELP") == 0) {
                    const char* help1 = "General: /list, /rooms, /roomlist, /join [id/name] [pass], /createroom [name] [pass], /leaveroom, /name [name], /status, /changestatus [stat], /quit";
                    SendPacket(user->socket, "SYS", help1);
                    const char* help2 = "Friends/Block: /addfriend [id/name], /accept [id/name], /myfriends, /deletefriend [id/name], /w [id/name] [msg], /block [id/name]";
                    SendPacket(user->socket, "SYS", help2);
                    const char* help3 = "Master: /kick [id/name], /silence, /lock, /invite [id/name], /transfer [id/name] | Admin: /yell [msg]";
                    SendPacket(user->socket, "SYS", help3);
                }
            }
        }
    }

    WSACleanup();
    return 0;
}
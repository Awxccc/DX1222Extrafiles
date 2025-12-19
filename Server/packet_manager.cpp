#include "packet_manager.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int packet_add_data(char* Buffer, int BufferSize, const char* DataName, int Value) {
    char temp[256];
    sprintf_s(temp, 256, " %s=%d", DataName, Value);
    strcat_s(Buffer, BufferSize, temp);
    return strlen(Buffer);
}

int packet_add_data(char* Buffer, int BufferSize, const char* DataName, const char* Value) {
    char temp[1024]; // Assuming reasonable value length
    sprintf_s(temp, 1024, " %s=%s", DataName, Value);
    strcat_s(Buffer, BufferSize, temp);
    return strlen(Buffer);
}

int packet_encode(char* Packet, int MaxBufferSize, const char* PacketID, const char* PacketData) {
    // Format: <ID LEN DATA>
    int len = strlen(PacketData);
    sprintf_s(Packet, MaxBufferSize, "<%s %d %s>", PacketID, len, PacketData);
    return strlen(Packet);
}

int packet_decode(const char* Packet, char* PacketID, char* PacketData) {
    // Packet format: <ID LEN DATA>
    // Example: <CHAT 5 Hello>

    // Skip '<'
    if (Packet[0] != '<') return 0;

    int i = 1;
    int j = 0;

    // Get ID
    while (Packet[i] != ' ' && Packet[i] != '\0') {
        PacketID[j++] = Packet[i++];
    }
    PacketID[j] = '\0';

    // Skip space
    if (Packet[i] == ' ') i++;

    // Get Length (we scan it but mainly trust the string extraction)
    int dataLen = 0;
    char lenStr[10];
    j = 0;
    while (Packet[i] != ' ' && Packet[i] != '\0') {
        lenStr[j++] = Packet[i++];
    }
    lenStr[j] = '\0';
    dataLen = atoi(lenStr);

    // Skip space
    if (Packet[i] == ' ') i++;

    // Get Data
    j = 0;
    // Read until '>' (assuming '>' is the trailer)
    // Note: This simple parser assumes '>' is not inside the message text for simplicity.
    // A robust parser would use dataLen to read exactly N chars.
    while (Packet[i] != '>' && Packet[i] != '\0') {
        PacketData[j++] = Packet[i++];
    }
    PacketData[j] = '\0';

    return j; // Return actual data length parsed
}

// Helper to find "KEY=" string
const char* find_key(const char* Packet, const char* DataName) {
    const char* ptr = strstr(Packet, DataName);
    while (ptr != NULL) {
        // Ensure it's the start of a key (preceded by space or start of string)
        // and followed immediately by '='
        char prev = (ptr == Packet) ? ' ' : *(ptr - 1);
        char next = *(ptr + strlen(DataName));

        if (prev == ' ' && next == '=') {
            return ptr + strlen(DataName) + 1; // Return pointer to value
        }
        ptr = strstr(ptr + 1, DataName);
    }
    return NULL;
}

int packet_parser_data(const char* Packet, const char* DataName) {
    const char* valStart = find_key(Packet, DataName);
    if (!valStart) return 0;
    return atoi(valStart);
}

int packet_parser_data(const char* Packet, const char* DataName, char* ReturnData, int ReturnDataSize) {
    const char* valStart = find_key(Packet, DataName);
    if (!valStart) return 0;

    int i = 0;
    // Read until space or end of packet data
    while (valStart[i] != ' ' && valStart[i] != '>' && valStart[i] != '\0' && i < ReturnDataSize - 1) {
        ReturnData[i] = valStart[i];
        i++;
    }
    ReturnData[i] = '\0';
    return i;
}
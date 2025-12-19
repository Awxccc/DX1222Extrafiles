#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>

// Adds an integer value to the buffer: " KEY=VALUE"
int packet_add_data(char* Buffer, int BufferSize, const char* DataName, int Value);

// Adds a string value to the buffer: " KEY=VALUE"
int packet_add_data(char* Buffer, int BufferSize, const char* DataName, const char* Value);

// Encodes the packet into <ID LEN BODY> format
int packet_encode(char* Packet, int MaxBufferSize, const char* PacketID, const char* PacketData);

// Decodes the packet into ID and Body
int packet_decode(const char* Packet, char* PacketID, char* PacketData);

// Parses an integer value from the body
int packet_parser_data(const char* Packet, const char* DataName);

// Parses a string value from the body
int packet_parser_data(const char* Packet, const char* DataName, char* ReturnData, int ReturnDataSize);
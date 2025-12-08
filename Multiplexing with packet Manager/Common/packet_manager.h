#include <iostream>
#include <string>
#include <stdio.h>
#include <string.h>

int packet_add_data(char Buffer[], const char DataName[], const int Value);

int packet_add_data(char Buffer[], const char DataName[], const char Value[]);

int packet_parser_get_data(const char Packet[], const char DataName[], std::string& DataString);

int packet_parser_data(const char Packet[], const char DataName[]);

int packet_parser_data(const char Packet[], const char DataName[], char Buffer[], int& BufferSize);

int packet_encode(char Packet[], const int MaxBufferSize, const char PacketID[], const char PacketData[], int PacketLength);

int packet_decode(const char Packet[], char PacketID[], char PacketData[]);

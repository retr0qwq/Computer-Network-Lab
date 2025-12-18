#include<iostream>
#include<cstdint>
#include<cstring>
#include<iomanip>
using namespace std;
#define MAX_DATA_SIZE 1460
const uint16_t FLAG_SYN  = 0x01;
const uint16_t FLAG_ACK  = 0x02;
const uint16_t FLAG_FIN  = 0x04;
const uint16_t FLAG_DATA = 0x08;
#pragma pack(1)
struct PacketHeader
{
    uint32_t seq;       // 序列号
    uint32_t ack;       // 确认号
    uint16_t flags;     // 标志位
    uint16_t checksum;  // 校验和
    uint16_t window;    // 窗口大小
    uint16_t length;    // 数据长度
};
#pragma pack()

class Packet
{ 
public:
    PacketHeader head;           
    char data[MAX_DATA_SIZE];  
    void reset();
    Packet () { reset(); }
    void print(const char* tag="Packet");
    bool check_checksum() { return calculate_checksum() == 0; };
    void update_checksum();
    void load_data(const char* buffer, uint16_t size);
private:
    uint16_t calculate_checksum();
};
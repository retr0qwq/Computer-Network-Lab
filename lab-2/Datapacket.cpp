#include<iostream>
#include "Datapacket.h"
using namespace std;
void Packet::reset()
{
    memset(&head, 0, sizeof(head));
    memset(data, 0, sizeof(data));
}
void Packet::print(const char* tag)
{
    cout<< "[" << tag << "] "
        << "Seq:" << head.seq << "\t"
        << "Ack:" << head.ack << "\t"
        << "Len:" << head.length << "\t"
        << "Win:" << head.window << "\t";
        
    cout << "Flags:[";
    if (head.flags & FLAG_SYN) cout << "SYN ";
    if (head.flags & FLAG_ACK) cout << "ACK ";
    if (head.flags & FLAG_FIN) cout << "FIN ";
    if (head.flags & FLAG_DATA) cout << "DATA";
    cout << "]";

    if (head.length > 0) {
        // 打印前10个字符
        string preview(data, min((int)head.length, 10));
        cout << " Content: " << preview << "...";
    }
    cout << endl;
}
void Packet::update_checksum() {
    head.checksum = 0; 
    head.checksum = calculate_checksum();
}
void Packet::load_data(const char* buffer, uint16_t size) {
    if (size > MAX_DATA_SIZE) {
        cout << "Data size exceeds maximum limit!" << endl;
        return;
    }
    memcpy(data, buffer, size);
    head.length = size;
}
uint16_t Packet::calculate_checksum() {
    uint32_t sum = 0;
    // 计算头部的校验和
    uint16_t* ptr = (uint16_t*)&head;
    for (size_t i = 0; i < sizeof(PacketHeader) / 2; ++i) {
        sum += *ptr++;
    }
    // 计算数据部分的校验和
    ptr = (uint16_t*)data;
    for (size_t i = 0; i < head.length / 2; ++i) {
        sum += *ptr++;
    }
    if (head.length % 2) {
        sum += data[head.length - 1];
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return static_cast<uint16_t>(~sum);
}
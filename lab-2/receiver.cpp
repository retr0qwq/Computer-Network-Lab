#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctime>
#include <conio.h>
#include <iomanip>
#include <string>
#include <ctime>
#include "Datapacket.h" 

#pragma comment(lib, "ws2_32.lib") 
using namespace std;
#define SERVER_PORT 8080
int timeout=1000;
long long totalBytesReceived = 0;
int main() {
    SetConsoleOutputCP(CP_UTF8);  // 设置控制台输出为 UTF-8
    SetConsoleCP(CP_UTF8);        // 设置控制台输入为 UTF-8
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed!" << endl;
        return 1;
    }
    SOCKET receiver = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (receiver == INVALID_SOCKET) {
        cout << "Socket creation failed." << endl;
        WSACleanup();   
        return 1;
    }
    // 绑定端口
    sockaddr_in recvAddr;
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(SERVER_PORT);
    recvAddr.sin_addr.s_addr = INADDR_ANY; 
    setsockopt(receiver, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    if (bind(receiver, (sockaddr*)&recvAddr, sizeof(recvAddr)) == SOCKET_ERROR) {
        cerr << "Bind failed!" << endl;
        closesocket(receiver);
        WSACleanup();
        return -1;
    }

    cout << "=== Receiver Started on Port " << SERVER_PORT << " ===" << endl;
    cout << "Waiting for connection..." << endl;
    // 创建Packet
    sockaddr_in senderAddr;
    int senderAddrSize = sizeof(senderAddr);
    Packet recvPkt, sendPkt;
    uint32_t expectedSeq = 0;
    ofstream outFile("received_file.dat", ios::binary);
    if (!outFile.is_open()) {
        cerr << "Cannot create output file!" << endl;
        return -1;
    }
    bool isConnected = false;
    bool finished = false; 
    while(!finished){
        int ret = recvfrom(receiver, (char*)&recvPkt, sizeof(Packet), 0, (sockaddr*)&senderAddr, &senderAddrSize);
        if (ret == SOCKET_ERROR) {
            if (WSAGetLastError() == WSAETIMEDOUT) {
                string command;
                if(_kbhit()){
                    getline(cin, command);
                    if (command == "close")
                    {
                        finished = true; 
                        cout << "检测到关闭命令，服务器即将关闭..." << endl;
                    }
                }
                
                continue;
            } else {
                cerr << "Receive failed!" << endl;
                break;
            }
        }
        if (!recvPkt.check_checksum()) {
            cout << "Packet checksum error. Discarding packet with seq: " << recvPkt.head.seq << endl;
            continue;
        }
        if (!isConnected) {
            if (recvPkt.head.flags & FLAG_SYN) {
                cout << "Received SYN. Sending SYN-ACK..." << endl;
                isConnected = true;
                expectedSeq = recvPkt.head.seq + 1;
                sendPkt.reset();
                sendPkt.head.seq = 0;
                sendPkt.head.ack = expectedSeq;
                sendPkt.head.flags = FLAG_SYN | FLAG_ACK;
                sendPkt.update_checksum();
                sendto(receiver, (char*)&sendPkt, sizeof(PacketHeader), 0, (sockaddr*)&senderAddr, senderAddrSize);
                cout << "Connection established." << endl;
                continue;
            }
            else {
                cout << "Waiting for SYN packet to establish connection..." << endl;
                continue;
            }
        }
        else{
            if (recvPkt.head.flags & FLAG_FIN) {
                // 发送ACK
                cout << "Received FIN. Sending ACK for FIN..." << endl;
                sendPkt.reset();
                sendPkt.head.seq = 0;
                sendPkt.head.ack = recvPkt.head.seq + 1;
                sendPkt.head.flags = FLAG_ACK;
                sendPkt.update_checksum();
                sendto(receiver, (char*)&sendPkt, sizeof(PacketHeader), 0, (sockaddr*)&senderAddr, senderAddrSize);
                // 发送FIN
                cout << "Sending FIN to close connection..." << endl;
                sendPkt.reset();
                sendPkt.head.seq = 0;
                sendPkt.head.ack = 0;
                sendPkt.head.flags = FLAG_FIN;      
                sendPkt.update_checksum();
                sendto(receiver, (char*)&sendPkt, sizeof(PacketHeader), 0, (sockaddr*)&senderAddr, senderAddrSize);
                cout << "Connection closed by sender." << endl;
                isConnected = false;
                expectedSeq = 0;
                continue;
            }
            if (recvPkt.head.flags & FLAG_DATA) {
                if (recvPkt.head.seq == expectedSeq) {
                    
                    // 写入文件
                    outFile.write(recvPkt.data, recvPkt.head.length);
                    totalBytesReceived += recvPkt.head.length;

                    expectedSeq += recvPkt.head.length;

                    // 发送 ACK
                    sendPkt.reset();
                    sendPkt.head.flags = FLAG_ACK;
                    sendPkt.head.ack = expectedSeq; 
                    sendPkt.head.seq = 0; 
                    sendPkt.update_checksum();

                    sendto(receiver, (char*)&sendPkt, sizeof(PacketHeader), 0, (sockaddr*)&senderAddr, senderAddrSize);
                } 
                else if (recvPkt.head.seq < expectedSeq) {
                    // 收到重复包 
                    cout << "[DUPLICATE] Expected " << expectedSeq << " got " << recvPkt.head.seq << endl;
                    
                    sendPkt.reset();
                    sendPkt.head.flags = FLAG_ACK;
                    sendPkt.head.ack = expectedSeq;
                    sendPkt.update_checksum();
                    sendto(receiver, (char*)&sendPkt, sizeof(PacketHeader), 0, (sockaddr*)&senderAddr, senderAddrSize);
                }
                else {
                    cout << "[OUT_OF_ORDER] Expected " << expectedSeq << " got " << recvPkt.head.seq << endl;
                    
                    // 触发发送方快重传
                    sendPkt.reset();
                    sendPkt.head.flags = FLAG_ACK;
                    sendPkt.head.ack = expectedSeq;
                    sendPkt.update_checksum();
                    sendto(receiver, (char*)&sendPkt, sizeof(PacketHeader), 0, (sockaddr*)&senderAddr, senderAddrSize);
                }
            } 
            else {
                cout << "Out-of-order packet. Expected seq: " << expectedSeq << ", but got: " << recvPkt.head.seq << endl;
            }
            sendPkt.reset();
            sendPkt.head.seq = 0;
            sendPkt.head.ack = expectedSeq;
            sendPkt.head.flags = FLAG_ACK;
            sendPkt.update_checksum();
            sendto(receiver, (char*)&sendPkt, sizeof(PacketHeader), 0, (sockaddr*)&senderAddr, senderAddrSize);
            cout << "Sent ACK for seq: " << expectedSeq << endl;
        }
    }

    closesocket(receiver);
    WSACleanup();
    cout << "服务器已关闭" << endl;
    return 0;
}
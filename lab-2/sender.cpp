#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Datapacket.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8888
#define TIMEOUT_MS 100        
#define WINDOW_SIZE 5         
#define MAX_MSS 1024 

SOCKET senderSocket;
sockaddr_in serverAddr;
int addrLen = sizeof(serverAddr);
uint32_t base = 0;          // 窗口基准 
uint32_t nextSeqNum = 0;    // 下一个要发送的序号
clock_t timerStart;         
bool timerRunning = false;
bool finished = false;
vector<Packet> sendBuffer; 

void resetSender() {
    base = 0;
    nextSeqNum = 0;
    timerRunning = false;
    sendBuffer.clear();
}

void startTimer() {
    if (!timerRunning) {
        timerStart = clock();
        timerRunning = true;
    }
}
void stopTimer() {
    timerRunning = false;
}

bool isTimerExpired() {
    if (!timerRunning) return false;
    clock_t now = clock();
    double elapsedMs = double(now - timerStart) / CLOCKS_PER_SEC * 1000;
    return elapsedMs >= TIMEOUT_MS;
}

void sendPacket(Packet& pkt) {
    pkt.update_checksum();
    sendto(senderSocket, (char*)&pkt, sizeof(PacketHeader) + pkt.head.length, 0,(sockaddr*)&serverAddr, addrLen);
}

void log(const char* event, uint32_t seq, uint32_t ack, int flag) {
    cout << "[" << event << "] Seq:" << seq << " Ack:" << ack;
    if (flag & FLAG_SYN) cout << " SYN";
    if (flag & FLAG_FIN) cout << " FIN";
    if (flag & FLAG_ACK) cout << " ACK";
    cout << endl;
}

bool handshake(){

}

void transferFile(const string& filename){

}

void teardown(){
    
}
int main(){
    SetConsoleOutputCP(CP_UTF8);  // 设置控制台输出为 UTF-8
    SetConsoleCP(CP_UTF8);        // 设置控制台输入为 UTF-8
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed!" << endl;
        return 1;
    }
    senderSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (senderSocket == INVALID_SOCKET) {
        cerr << "Socket creation failed." << endl;
        WSACleanup();
        return 1;
    }
    
    // 设置服务器地址
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);
    string filename;
    while (true) {
        cout << "\n>>> Please enter filename to send (or 'exit' to quit): ";
        cin >> filename;

        if (filename == "exit") {
            break;
        }

        
        ifstream checkFile(filename);
        if (!checkFile.good()) {
            cout << "[Error] File not found! Please try again." << endl;
            continue;
        }
        checkFile.close();

        resetSender();
        
        if (handshake()) {
            transferFile(filename);
            teardown();
        } else {
            cout << "[Error] Server not responding. Is Receiver running?" << endl;
        }
    }
}
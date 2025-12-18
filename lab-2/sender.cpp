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
#define SERVER_PORT 8080
#define TIMEOUT_MS 100        
#define WINDOW_SIZE 5         
#define MAX_MSS 1024 

SOCKET senderSocket;
sockaddr_in recvAddr;
int addrLen = sizeof(recvAddr);
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
    sendto(senderSocket, (char*)&pkt, sizeof(PacketHeader) + pkt.head.length, 0,(sockaddr*)&recvAddr, addrLen);
}

void log(const char* event, uint32_t seq, uint32_t ack, int flag) {
    cout << "[" << event << "] Seq:" << seq << " Ack:" << ack;
    if (flag & FLAG_SYN) cout << " SYN";
    if (flag & FLAG_FIN) cout << " FIN";
    if (flag & FLAG_ACK) cout << " ACK";
    cout << endl;
}

bool handshake(){
    Packet sendPkt, recvPkt;
    // 发送 SYN 
    sendPkt.reset();
    sendPkt.head.flags = FLAG_SYN;
    sendPkt.head.seq = 0; // 初始序号
    sendPacket(sendPkt);
    log("SEND", 0, 0, FLAG_SYN);

    // 设置接收超时
    int timeout = 2000; 
    setsockopt(senderSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));

    while (true) {
        int ret = recvfrom(senderSocket, (char*)&recvPkt, sizeof(Packet), 0, (sockaddr*)&recvAddr, &addrLen);
        if (ret > 0) {
            if (recvPkt.check_checksum() && (recvPkt.head.flags & (FLAG_SYN | FLAG_ACK))) {
                log("RECV", recvPkt.head.seq, recvPkt.head.ack, recvPkt.head.flags);
                break; 
            }
        } else {
            cout << "[TIMEOUT] Retrying handshake..." << endl;
            sendPacket(sendPkt);
        }
    }

    sendPkt.reset();
    sendPkt.head.flags = FLAG_ACK;
    sendPkt.head.seq = 1;
    sendPkt.head.ack = recvPkt.head.seq + 1; 
    log("SEND", sendPkt.head.seq, sendPkt.head.ack, FLAG_ACK);

    base = 0; 
    nextSeqNum = 0;
    cout << "=== Connection Established ===" << endl;
    return true;
}

void transferFile(const string& filename){
    ifstream file(filename, ios::binary | ios::ate);
    if (!file.is_open()) {
        cerr << "File not found: " << filename << endl;
        return;
    }
    long long fileSize = file.tellg();
    file.seekg(0, ios::beg); // 回到文件头

    cout << "Start Sending File: " << filename << " (" << fileSize << " bytes)" << endl;

    // 将 Socket 设置为非阻塞 (或者极短超时)，方便我们在循环里同时处理发送和接收
    int nonBlockTimeout = 10; // 10ms
    setsockopt(senderSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nonBlockTimeout, sizeof(nonBlockTimeout));

    Packet recvPkt, namePkt;
    namePkt.reset();
    namePkt.head.seq = nextSeqNum;
    namePkt.head.flags = FLAG_DATA; // 依然是数据包
    
    // 拷贝文件名到数据区 (只拷贝名字，不拷贝路径)
    // 简单提取文件名的逻辑：
    string pureName = filename;
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != string::npos) {
        pureName = filename.substr(lastSlash + 1);
    }
    int nameLen = (int)pureName.length();
    strcpy(namePkt.data, pureName.c_str());
    namePkt.head.length = (uint16_t)pureName.length();

    sendPacket(namePkt); // 发送文件名包
    cout << "[SEND_NAME] " << pureName << endl;

    // 加入缓冲区等待 ACK
    sendBuffer.push_back(namePkt);
    if (base == nextSeqNum) startTimer();
    
    // 序号前进 (接收方会根据这个长度更新 expectedSeq)
    nextSeqNum += namePkt.head.length;
    // === 主循环：只要还有未确认的数据，就继续 ===
    while (base < fileSize) {
        
        // --- A. 发送窗口内的新数据 ---
        // 只要 窗口没满 且 还有数据没读完
        while (nextSeqNum < base + WINDOW_SIZE * MAX_MSS && nextSeqNum < fileSize) {
            Packet pkt;
            pkt.reset();
            pkt.head.seq = nextSeqNum;
            pkt.head.flags = FLAG_DATA;
            
            // 读取文件数据
            file.seekg(nextSeqNum - nameLen);
            int bytesToRead = min((long long)MAX_MSS, fileSize - nextSeqNum);
            file.read(pkt.data, bytesToRead);
            pkt.head.length = bytesToRead;

            sendPacket(pkt);
            cout << "[SEND_DATA] Seq=" << nextSeqNum << " Len=" << bytesToRead << endl;

            // 加入缓冲区（为了重传）
            sendBuffer.push_back(pkt);

            // 如果是第一个包，启动计时器
            if (base == nextSeqNum) {
                startTimer();
            }

            nextSeqNum += bytesToRead;
        }

        // --- B. 接收 ACK ---
        int ret = recvfrom(senderSocket, (char*)&recvPkt, sizeof(Packet), 0, (sockaddr*)&recvAddr, &addrLen);
        if (ret > 0) {
            if (recvPkt.check_checksum() && (recvPkt.head.flags & FLAG_ACK)) {
                uint32_t ack = recvPkt.head.ack;
                
                // 累计确认：如果收到的 ack > base，说明 ack 之前的数据都到了
                if (ack > base) {
                    cout << "[RECV_ACK] Ack=" << ack << " (New Base)" << endl;
                    
                    base = ack; // 滑动窗口左沿
                    stopTimer(); // 收到新ACK，停止旧计时器

                    // 清理缓冲区中已经确认的包
                    // 实际实现中，vector删除头部效率低，这里简单演示逻辑
                    // 更好的做法是用 deque 或仅移动指针
                    vector<Packet> newBuffer;
                    for (auto& p : sendBuffer) {
                        if (p.head.seq >= base) newBuffer.push_back(p);
                    }
                    sendBuffer = newBuffer;

                    // 如果还有未确认的包，重启计时器
                    if (base < nextSeqNum) {
                        startTimer();
                    }
                } else {
                    cout << "[RECV_ACK] Duplicate Ack=" << ack << endl;
                    // TODO: 后期在这里实现 Reno 的快重传 (3次重复ACK)
                }
            }
        }

        // --- C. 超时重传 (Timeout) ---
        if (isTimerExpired()) {
            cout << "[TIMEOUT] Resending Window from " << base << endl;
            startTimer(); // 重置计时器

            // 简单的 GBN 重传：重传缓冲区里所有的包
            for (auto& pkt : sendBuffer) {
                sendPacket(pkt);
                cout << "[RESEND] Seq=" << pkt.head.seq << endl;
            }
        }
    }
    cout << "=== File Transfer Completed ===" << endl;
}

void teardown(){
    Packet sendPkt, recvPkt;

    // --- Step 1: 发送 FIN ---
    sendPkt.reset();
    sendPkt.head.flags = FLAG_FIN;
    sendPkt.head.seq = nextSeqNum; // 接着最后的序号
    sendPacket(sendPkt);
    log("SEND_FIN", sendPkt.head.seq, 0, FLAG_FIN);

    // --- Step 2: 等待 ACK ---
    int timeout = 1000;
    setsockopt(senderSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    while(true) {
        int ret = recvfrom(senderSocket, (char*)&recvPkt, sizeof(Packet), 0, (sockaddr*)&recvAddr, &addrLen);
        if (ret > 0 && (recvPkt.head.flags & FLAG_ACK)) {
            log("RECV_ACK", 0, recvPkt.head.ack, FLAG_ACK);
            break; 
        }
        // 简单重传 FIN
        if (ret <= 0) sendPacket(sendPkt);
    }

    // --- Step 3: 等待 Server 的 FIN ---
    cout << "Waiting for Server FIN..." << endl;
    while(true) {
        int ret = recvfrom(senderSocket, (char*)&recvPkt, sizeof(Packet), 0, (sockaddr*)&recvAddr, &addrLen);
        if (ret > 0 && (recvPkt.head.flags & FLAG_FIN)) {
            log("RECV_FIN", recvPkt.head.seq, 0, FLAG_FIN);
            break;
        }
    }

    // --- Step 4: 发送最后的 ACK ---
    sendPkt.reset();
    sendPkt.head.flags = FLAG_ACK;
    sendPkt.head.ack = recvPkt.head.seq; // 这里的seq其实不重要了
    sendPacket(sendPkt);
    log("SEND_ACK", 0, sendPkt.head.ack, FLAG_ACK);

    // 等待 2MSL (这里简单 sleep 一下，模拟 TIME_WAIT)
    cout << "Entering TIME_WAIT..." << endl;
    Sleep(1000); 
    cout << "=== Connection Closed ===" << endl;
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
    memset(&recvAddr, 0, sizeof(recvAddr));
    recvAddr.sin_family = AF_INET;
    recvAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &recvAddr.sin_addr);
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
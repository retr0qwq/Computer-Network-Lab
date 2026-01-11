#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <ctime>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <deque>
#include <chrono>
#include "Datapacket.h"

#pragma comment(lib, "ws2_32.lib")

using namespace std;
using Clock = std::chrono::high_resolution_clock;
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8081
#define TIMEOUT_MS 500         
#define MAX_MSS 10000

enum RenoState {
    SLOW_START,         // 慢启动
    CONGESTION_AVOIDANCE, // 拥塞避免
    FAST_RECOVERY       // 快恢复
};
RenoState state = SLOW_START;
double cwnd = MAX_MSS;
uint32_t ssthresh = 64 * MAX_MSS;
int dupAckCount = 0;
SOCKET senderSocket;
sockaddr_in recvAddr;
int addrLen = sizeof(recvAddr);
uint32_t base = 0;          // 窗口基准 
uint32_t nextSeqNum = 0;    // 下一个要发送的序号
clock_t timerStart;         
bool timerRunning = false;
bool finished = false;
deque<Packet> sendBuffer; 
Clock::time_point startTime;
Clock::time_point endTime;
bool started = false;
uint64_t totalBytesSent = 0;
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

    base = 1; 
    nextSeqNum = 1;
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
    state = SLOW_START;
    cwnd = MAX_MSS;
    ssthresh = 64 * MAX_MSS;
    dupAckCount = 0;
    cout << "[Reno Init] cwnd:" << cwnd << " ssthresh:" << ssthresh << endl;
    // 设置非阻塞接收
    int nonBlockTimeout = 50; 
    setsockopt(senderSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&nonBlockTimeout, sizeof(nonBlockTimeout));

    Packet recvPkt, namePkt;
    namePkt.reset();
    namePkt.head.seq = nextSeqNum;
    namePkt.head.flags = FLAG_DATA;
    
    // 拷贝文件名到数据区
    string pureName = filename;
    size_t lastSlash = filename.find_last_of("/\\");
    if (lastSlash != string::npos) {
        pureName = filename.substr(lastSlash + 1);
    }
    int nameLen = (int)pureName.length();
    strcpy(namePkt.data, pureName.c_str());
    namePkt.head.length = (uint16_t)pureName.length();
    fileSize += nameLen; 
    if (!started) {
        startTime = Clock::now();
        started = true;
    }
    totalBytesSent += nameLen;  
    sendPacket(namePkt); // 发送文件名包
    cout << "[SEND_NAME] " << pureName << endl;

    // 加入缓冲区等待 ACK
    sendBuffer.push_back(namePkt);
    if (base == nextSeqNum) startTimer();

    nextSeqNum += namePkt.head.length;
    while (base < fileSize) {
        
        // 发送窗口内的新数据 
        while (nextSeqNum < base + (int) cwnd && nextSeqNum < fileSize) {
            Packet pkt;
            pkt.reset();
            pkt.head.seq = nextSeqNum;
            pkt.head.flags = FLAG_DATA;
            
            file.seekg(nextSeqNum - nameLen- 1);
            int bytesToRead = min((long long)MAX_MSS, fileSize - nextSeqNum);
            file.read(pkt.data, bytesToRead);
            pkt.head.length = bytesToRead;

            sendPacket(pkt);
            cout << "[SEND_DATA] Seq=" << nextSeqNum << " Len=" << bytesToRead << endl;

            // 加入缓冲区
            sendBuffer.push_back(pkt);
            if (base == nextSeqNum) {
                startTimer();
            }

            nextSeqNum += bytesToRead;
        }

        //  接收 ACK 
        int ret = recvfrom(senderSocket, (char*)&recvPkt, sizeof(Packet), 0, (sockaddr*)&recvAddr, &addrLen);
        if (ret > 0) {
            if (recvPkt.check_checksum() && (recvPkt.head.flags & FLAG_ACK)) {
                uint32_t ack = recvPkt.head.ack;
                
                // 累计确认
                if (ack > base) {
                    cout << "[RECV_ACK] Ack=" << ack << " (New Base)" << endl;
                    uint32_t newlyAcked = ack - base;
                    totalBytesSent  += newlyAcked;
                    base = ack; 
                    if (state == SLOW_START) {
                        cwnd += MAX_MSS;
                        if (cwnd >= ssthresh) {
                            state = CONGESTION_AVOIDANCE;
                        }
                    } 
                    else if (state == CONGESTION_AVOIDANCE) {
                        cwnd += MAX_MSS * ((double)MAX_MSS / cwnd);
                    } 
                    else if (state == FAST_RECOVERY) {
                        state = CONGESTION_AVOIDANCE;
                        cwnd = ssthresh;
                    }
                    dupAckCount = 0; // 重置重复ACK计数
                    cout << "[Reno] cwnd:" << cwnd << " ssthresh:" << ssthresh << endl;
                    stopTimer(); 

                    while( !sendBuffer.empty() && sendBuffer[0].head.seq + sendBuffer[0].head.length <= base) {
                        sendBuffer.pop_front();
                    }

                    if (base < nextSeqNum) {
                        startTimer();
                    }
                }
                else {
                    dupAckCount++;

                    if (state == FAST_RECOVERY) {
                        cwnd += MAX_MSS;
                    }
                    else if (dupAckCount == 3) {
                        // 触发快重传
                        cout << "[Fast Retransmit] 3 Dup ACKs " << endl;
                        ssthresh = max((double)10 * MAX_MSS, cwnd / 2);
                        cwnd = ssthresh + 3 * MAX_MSS;
                        state = FAST_RECOVERY;
                        
                        if (!sendBuffer.empty()) {
                            sendPacket(sendBuffer[0]);
                            cout << "[FAST RESEND] Seq=" << sendBuffer[0].head.seq << endl;
                        }
                    }
                }
            }
        }

        // 超时重传
        if (isTimerExpired()) {
            cout << "[TIMEOUT] Resending Window from " << base << endl;
            ssthresh = max((double)2 * MAX_MSS, cwnd / 2);
            cwnd = MAX_MSS; 
            // 重新进入慢启动
            state = SLOW_START;
            dupAckCount = 0;
            stopTimer();
            startTimer(); 
            if (!sendBuffer.empty()) {
                sendPacket(sendBuffer[0]); 
                cout << "[TIMEOUT RESEND] Seq=" << sendBuffer[0].head.seq << endl;
            }
        }
    }
    cout << "=== File Transfer Completed ===" << endl;
    endTime = Clock::now();
    chrono::duration<double> elapsedSeconds = endTime - startTime;
    double throughput = totalBytesSent / elapsedSeconds.count() / 1024;0; // KB/s
    cout << "Total Bytes Sent: " << totalBytesSent << " bytes" << endl;
    cout << "Total Time: " << elapsedSeconds.count() << " seconds" << endl;
    cout << "Throughput: " << throughput << " KB/s" << endl;
}

void teardown(){
    Packet sendPkt, recvPkt;

    // 发送 FIN
    sendPkt.reset();
    sendPkt.head.flags = FLAG_FIN;
    sendPkt.head.seq = nextSeqNum; // 接着最后的序号
    sendPacket(sendPkt);
    nextSeqNum += 1; 
    log("SEND_FIN", sendPkt.head.seq, 0, FLAG_FIN);

    int timeout = 1000;
    setsockopt(senderSocket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof(timeout));
    
    while(true) {
        int ret = recvfrom(senderSocket, (char*)&recvPkt, sizeof(Packet), 0, (sockaddr*)&recvAddr, &addrLen);
        if (ret > 0 && (recvPkt.head.flags & FLAG_ACK)) {
            log("RECV_ACK", 0, recvPkt.head.ack, FLAG_ACK);
            break; 
        }
        // 重传 FIN
        if (ret <= 0) sendPacket(sendPkt);
    }

    cout << "Waiting for Server FIN..." << endl;
    while(true) {
        int ret = recvfrom(senderSocket, (char*)&recvPkt, sizeof(Packet), 0, (sockaddr*)&recvAddr, &addrLen);
        if (ret > 0 && (recvPkt.head.flags & FLAG_FIN)) {
            log("RECV_FIN", recvPkt.head.seq, 0, FLAG_FIN);
            break;
        }
    }

    sendPkt.reset();
    sendPkt.head.flags = FLAG_ACK;
    sendPkt.head.ack = recvPkt.head.seq; 
    sendPacket(sendPkt);
    log("SEND_ACK", 0, sendPkt.head.ack, FLAG_ACK);

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
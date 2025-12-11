#include <iostream>
#include <fstream>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <ctime>
#include <iomanip>
#include "Datapacket.h" 

#pragma comment(lib, "ws2_32.lib") 
using namespace std;
#define SERVER_PORT 8080


int main() {
    SetConsoleOutputCP(CP_UTF8);  // 设置控制台输出为 UTF-8
    SetConsoleCP(CP_UTF8);        // 设置控制台输入为 UTF-8
    // 初始化 Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cerr << "WSAStartup failed!" << endl;
        return 1;
    }
    SOCKET server = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    WSACleanup();
    return 0;
}
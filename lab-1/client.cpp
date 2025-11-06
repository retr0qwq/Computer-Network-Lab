#include <winsock2.h>
#include <iostream>
#include <thread>
#include <string>

#pragma comment(lib, "ws2_32.lib")
#define PORT 8080
using namespace std;

string username="0d000721";
bool connected = false;
SOCKET client;
void connect()
{
    sockaddr_in client_addr;
    client_addr.sin_family = AF_INET;
    client_addr.sin_port = htons(PORT);
    client_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    client = socket(AF_INET, SOCK_STREAM, 0);
    if (connect(client, (sockaddr*)&client_addr, sizeof(client_addr)) == SOCKET_ERROR) {
        cout << "无法连接到服务器。" << endl;
        return;
    }
    else
    {
        connected = true;
        cout << "成功连接到服务器！" << endl;
    }
}
int main() 
{
    SetConsoleOutputCP(CP_UTF8);  // 设置控制台输出为 UTF-8
    SetConsoleCP(CP_UTF8);        // 设置控制台输入为 UTF-8

    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed." << endl;
        return 1;
    }

    // 创建Socket
    SOCKET client = socket(AF_INET, SOCK_STREAM, 0);
    if (client == INVALID_SOCKET) {
        cout << "Socket creation failed." << endl;
        WSACleanup();
        return 1;
    }

}
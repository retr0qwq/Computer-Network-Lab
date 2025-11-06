#include <winsock2.h>
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <conio.h> // For _kbhit
#include <string>
#include <thread>  // For std::thread
#include <atomic>  // For std::atomic

#define PORT 8080
#pragma comment(lib, "ws2_32.lib")
using namespace std;

// 全局变量，用于线程间通信
atomic<bool> server_running(true);

void monitor_keyboard() {
    string command;
    while (server_running) {
        if (_kbhit()) { // 检测是否有键盘输入
            getline(cin, command);
            if (command == "close") {
                cout << "检测到关闭命令，服务器即将关闭..." << endl;
                // 创建一个客户端套接字，向服务器发送连接请求
                SOCKET client = socket(AF_INET, SOCK_STREAM, 0);
                if (client == INVALID_SOCKET) {
                    cout << "Failed to create client socket." << endl;
                    break;
                }

                sockaddr_in server_addr;
                server_addr.sin_family = AF_INET;
                server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
                server_addr.sin_port = htons(PORT);

                connect(client, (sockaddr*)&server_addr, sizeof(server_addr));
                closesocket(client);

                server_running = false; // 通知主线程退出
                break;
            }
        }
    }
}
void handle_connections(SOCKET server) {
    sockaddr_in client_addr;
    int client_size = sizeof(client_addr);

    while (server_running) {
        SOCKET client_socket = accept(server, (sockaddr*)&client_addr, &client_size);
        if (client_socket == INVALID_SOCKET) {
            if (!server_running) {
                cout << "Server shutting down, exiting connection thread..." << endl;
                break;
            }
            cout << "Failed to accept client." << endl;
            continue;
        }
        cout << "Client connected." << endl;
        closesocket(client_socket); // 关闭客户端连接
    }
}

int main() {
    SetConsoleOutputCP(CP_UTF8);  // 设置控制台输出为 UTF-8
    SetConsoleCP(CP_UTF8);        // 设置控制台输入为 UTF-8

    // 初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed." << endl;
        return 1;
    }

    // 创建Socket
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        cout << "Socket creation failed." << endl;
        WSACleanup();
        return 1;
    }

    // 绑定Socket
    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);
    if (bind(server, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        cout << "Bind failed." << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }

    // 监听Socket
    if (listen(server, 3) == SOCKET_ERROR) {
        cout << "Listen failed." << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    } else {
        cout << "服务器启动成功！\n正在监听端口：" << PORT << "..." << endl;
    }

    // 启动处理客户端连接的线程
    thread connection_thread(handle_connections, server);

    // 主线程继续处理其他任务，例如监听键盘输入
    monitor_keyboard();

    // 等待连接线程结束
    connection_thread.join();

    // 清理资源
    closesocket(server);
    WSACleanup();
    cout << "服务器已关闭。" << endl;
    return 0;
}

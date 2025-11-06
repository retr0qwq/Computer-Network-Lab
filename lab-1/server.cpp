#include <winsock2.h>
#include <iostream>
#include <cstring>
#include <stdio.h>
#include <conio.h> 
#include <string>
#include <thread> 
#include <atomic>  
#include <vector>
#include <mutex>
#include <ctime>
#define PORT 8080
#define NAME_SIZE 100
#define BUFFER_SIZE 5000
#pragma comment(lib, "ws2_32.lib")
using namespace std;

// 全局变量，用于线程间通信
atomic<bool> server_running(true),client_connecting(false);
vector<SOCKET> client_sockets;
mutex client_mutex;
int client_count = 0;
// 广播消息给所有client
void send_message_to_all(const string &message, SOCKET sender_socket) {
    lock_guard<mutex> lock(client_mutex);
    char time_buf[80];
    time_t now = time(nullptr);
    strftime(time_buf, sizeof(time_buf), "[%Y-%m-%d %H:%M:%S]", localtime(&now));
    string msg=string(time_buf) + " " + message;
    cout<<msg;
    for (auto &socket : client_sockets) {
        send(socket, msg.c_str(), msg.size(), 0);
    }
}
// 服务器控制
void monitor_keyboard() {
    string command;
    while (server_running) {
        if (_kbhit()) { // 检测是否有键盘输入
            getline(cin, command);
            if (command == "close")
             {
                server_running = false; 
                cout << "检测到关闭命令，服务器即将关闭..." << endl;
                /*创建一个客户端套接字，向服务器发送连接请求
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
                break;
                */
            }
        }
    }
}
// 处理单个客户端
void handle_client(SOCKET client_socket) 
{
    char name[NAME_SIZE];
    char buffer[BUFFER_SIZE];
    recv(client_socket, name, NAME_SIZE, 0);
    {
        lock_guard<mutex> lock(client_mutex);
        client_count++;
    }
    string welcome_msg = string(name) + " 已加入聊天，当前共有"+ to_string(client_count) + " 位用户在线。\n";
    send_message_to_all(welcome_msg, client_socket);
    while (server_running) {
        memset(buffer, 0, BUFFER_SIZE);
        int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
        if (bytes_received > 0) 
        {
            buffer[bytes_received] = '\0';
            string message = string(name) + ": " + string(buffer);
            if(string(buffer)=="bye\n")
            {
                string leave_msg = string(name) + " 已离开聊天。\n";
                send_message_to_all(leave_msg, client_socket);
                break;
            }
            else
            {
                send_message_to_all(message, client_socket);
            }
        }
        if (bytes_received <= 0) {
            string leave_msg = string(name) + " 已离开聊天。\n";
            send_message_to_all(leave_msg, client_socket);
            break;
        }
    }
    {
        lock_guard<mutex> lock(client_mutex);
        client_sockets.erase(remove(client_sockets.begin(), client_sockets.end(), client_socket), client_sockets.end());
    }
    closesocket(client_socket);
}
// 处理客户端连接
void handle_connections(SOCKET server)
 {
    sockaddr_in client_addr;
    int client_size = sizeof(client_addr);

    while (server_running) 
    {
    SOCKET client_socket = accept(server, (sockaddr*)&client_addr, &client_size);
    if (client_socket == INVALID_SOCKET) {
        // accept 出错，可以打印日志或者直接 continue
        continue;
    }
    {
        lock_guard<mutex> lock(client_mutex);   
        client_sockets.push_back(client_socket);
    }
    thread client_thread(handle_client, client_socket);
    client_thread.detach();
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
    monitor_keyboard();
    closesocket(server);  
    connection_thread.join();

    // 清理资源
    WSACleanup();
    cout << "服务器已关闭。" << endl;
    return 0;
}

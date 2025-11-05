#include <winsock2.h>  
#include <iostream>     
#include <cstring>     
#include <stdio.h>      

#define PORT 8080
#pragma comment(lib, "ws2_32.lib")
using namespace std;
int main()
{
    //初始化Winsock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) 
    {
        cout << "WSAStartup failed." << endl;
        return 1;
    }
    //创建Socket
    SOCKET server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) //检查是否为-1
    {
        cout << "Socket creation failed." << endl;
        WSACleanup();
        return 1;
    }
    //绑定Socket
    sockaddr_in server_addr, client_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr =  inet_addr("127.0.0.1");
    server_addr.sin_port = htons(PORT);
    if(bind(server, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR)
    {
        cout << "Bind failed." << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }
    //监听Socket
    if (listen(server, 3) == SOCKET_ERROR)
    {
        cout << "Listen failed." << endl;
        closesocket(server);
        WSACleanup();
        return 1;
    }
    else
    {
        cout << "Server is listening on port " << PORT << "..." << endl;
    }
    closesocket(server);
    WSACleanup();
    return 0;
}

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
void receive_messages()
{
    char buffer[5000];
    while (connected) {
        memset(buffer, 0, sizeof(buffer));
        int bytes_received = recv(client, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received > 0) 
        {
            buffer[bytes_received] = '\0';
            string message(buffer);
            cout << message << endl;
        }
        else 
            break;
    }
}
void connect_server()
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
        cout << username << "成功连接到服务器！" << endl;
        cout << "您可以开始发送消息了，输入'bye'以断开连接。" << endl;
        send(client, username.c_str(), username.size(), 0);
        thread receive_thread(receive_messages);
        receive_thread.detach();
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
    cout<<"Ciallo～(∠・ω< )⌒☆欢迎使用YUZUSOFT聊天软件！"<<endl;
    cout<<"请输入您的用户名：";
    getline(cin,username);
    cout<<username<<",欢迎使用！"<<endl;
    while(1)
    {
        string command;
        if(!connected)
        {
            cout<<"输入/1以更改用户名，输入/2以连接服务器，输入/3以退出软件"<<endl;
            cout<<"请输入指令："<<endl;
            getline(cin,command);
            if(command=="/1")
            {
                cout<<"请输入新的用户名：";
                getline(cin,username);
                cout<<"用户名已更改为"<<username<<endl;
            }
            else if(command=="/2")
            {
                connect_server();
            }
            else if(command=="/3")
            {
                cout<<"正在退出软件，感谢使用YUZUSOFT聊天软件！"<<endl;
                break;
            }
            else
            {
                cout<<"无效指令，请重新输入！"<<endl;
            }
        }
        else
        {   
            getline(cin,command);
           if(command=="bye")
           {
               cout<<"已断开与服务器的连接！"<<endl;
               connected=false;
               send(client, "bye", 3, 0);
               closesocket(client);
           }
           else
           {
               string message="["+username+"]:"+command;
               send(client, message.c_str(), message.size(), 0);
           }
        }
    }

}
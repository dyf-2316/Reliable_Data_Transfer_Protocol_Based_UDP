#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <zconf.h>

#include <iostream>

#define SOCKET int

SOCKET client_socket;

int SERVER_PORT = 11332;
char *SERVER_IP = "127.0.0.1";

int main(int argc,char** argv)
{

//    std :: cout << "请输入服务器ip地址: ";
//    std :: cin >> SERVER_IP;
//    std :: cout << "请输入服务器对应端口号: ";
//    std :: cin >> SERVER_PORT;

    sockaddr_in server_addr{}; // 这里是服务器地址，用于之后的连接，与服务器端创建方式相同
    server_addr.sin_len = sizeof(sockaddr_in);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    bzero(&(server_addr.sin_zero), 8);

    client_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (client_socket == -1){
        perror("Socket open error.");
        return -1;
    }
    std :: cout << "Socket open successfully." << std :: endl;

    long send_num, recv_num;
    char send_buf[2048], recv_buf[2048];
    socklen_t addr_len = sizeof(struct sockaddr_in);

    while(true)
    {
        std :: cout << "- Client: ";
        bzero(send_buf,2048);
        std :: cin >> send_buf;
        if ((send_num = sendto(client_socket, send_buf, sizeof(send_buf), 0, (struct sockaddr *)&server_addr, addr_len)) == -1)
        {
            perror("send data error.");
            close(client_socket);
            exit(1);
        }
        bzero(recv_buf,2048);
        if ((recv_num = recvfrom(client_socket, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&server_addr, &addr_len)) == -1)
        {
            perror("recv data error.");
            close(client_socket);
            exit(1);
        }
        recv_buf[recv_num]='\0';
        std :: cout << "\t- Server: " << recv_buf << std :: endl;
    }
    close(client_socket);
    return 0;
}
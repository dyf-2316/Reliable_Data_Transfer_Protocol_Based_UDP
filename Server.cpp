#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <zconf.h>

#include <iostream>

#define SOCKET int

SOCKET server_socket;

int SERVER_PORT = 11332;
char *SERVER_IP = "127.0.0.1";

int main(int argc,char** argv)
{

//    std :: cout << "请输入服务器ip地址: ";
//    std :: cin >> SERVER_IP;
//    std :: cout << "请输入服务器对应端口号: ";
//    std :: cin >> SERVER_PORT;

    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1){
        perror("Socket open error.");
        return -1;
    }

    sockaddr_in server_addr{}, client_addr; // 这里是服务器地址，用于之后的连接，与服务器端创建方式相同
    server_addr.sin_len = sizeof(sockaddr_in);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    bzero(&(server_addr.sin_zero), 8);

    if (bind(server_socket, (sockaddr *) &server_addr, sizeof(server_addr)) == -1){
        perror("Socket bind error");
        return -1;
    }
    std :: cout << "Socket open successfully !" << std :: endl;

    int send_num, recv_num;
    char send_buf[2048],recv_buf[2048];
    socklen_t addr_len = sizeof(struct sockaddr_in);

//    server.sin_addr.s_addr = htonl(INADDR_ANY);

    while (true)
    {
        bzero(recv_buf,2048);
        if ((recv_num = recvfrom(server_socket, recv_buf, sizeof(recv_buf), 0, (struct sockaddr *)&client_addr, &addr_len)) == -1)
        {
            perror("recv data error.");
            close(server_socket);
            exit(1);
        }
        recv_buf[recv_num]='\0';

        std :: cout << "\t- Client: " << recv_buf << std :: endl;

        if (0 == strcmp(recv_buf,"quit"))
        {
            perror("the client break the server process\n");
            close(server_socket);
            break;
        }

        bzero(send_buf,2048);
        std :: cout << "- Server: ";
        std :: cin >> send_buf;
        if ((send_num = sendto(server_socket, send_buf, sizeof(send_buf), 0, (struct sockaddr *)&client_addr, addr_len)) == -1)
        {
            perror("send data error.");
            close(server_socket);
            exit(1);
        }

    }
    close(server_socket);
    return 0;
}
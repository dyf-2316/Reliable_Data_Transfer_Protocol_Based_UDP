#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <zconf.h>

#include <iostream>
#include <fstream>

#define SOCKET int
#define MTU 2048
#define MAX_DATA_BYTES MTU - sizeof(u_int) - 3 * sizeof(u_short)

socklen_t ADDR_LEN = sizeof(struct sockaddr_in);

SOCKET client_socket;
sockaddr_in server_addr{};

#define CON 0X0001
#define BOF 0X0002
#define SYN 0X0004
#define DOF 0X0008
#define FIN 0X0010

int SEQ = 0;

struct SendPacket{
    struct Packet{
        u_int seq;
        u_short check_sum;
        u_short len;
        u_short flag;
        char data[MAX_DATA_BYTES];
    } * buff;

    SendPacket(){
        buff = new Packet;
    }

    void init(u_short len, char *data, u_short flag){
        bzero(buff, sizeof(Packet));
        buff->seq = SEQ ++;
        SEQ %= 4;
        buff->check_sum = 0;
        buff->len = len;
        buff->flag = flag;
        if (data) {
            bcopy(data, buff->data, MAX_DATA_BYTES);
        }
    }

    void make_pkt(u_short check_sum){
        buff->check_sum = check_sum;
    }

};


struct RecPacket{
    struct Packet{
        u_int ack;
        u_short check_sum;
    } * buff;

    RecPacket(){
        buff = new Packet;
    };

    void extract_pkt(const char* message) {
        bzero(buff, sizeof(Packet));
        buff->ack = *((u_int *) &(message[0]));
        buff->check_sum = *((u_short *) &(message[4]));
    }
};

void show_send_pkt(SendPacket* packet){
    std::cout << "- Send   seq: " << packet->buff->seq << "  len: " << packet->buff->len ;
    std::cout << "  checksum: " << packet->buff->check_sum << "  flag: " << packet->buff->flag << std :: endl;
}

void show_rec_pkt(RecPacket* packet){
    std::cout << "\t- Rec   ack: " << packet->buff->ack << "  len: " << "  checksum: " << packet->buff->check_sum << std :: endl;
}

void udp_send(char *package, int size){
    sendto(client_socket, package, size, 0, (struct sockaddr *)&server_addr, ADDR_LEN);
}

u_short compute_check_sum(u_short *data, int count){
    u_long sum = 0;
    while (count--){
        sum += *data++;
        if (sum & 0XFFFF0000){
            sum &= 0XFFFF;
            sum ++;
        }
    }
    return ~(sum & 0XFFFF);
}

void rdt_send(char *data, u_int len, u_short flag, SendPacket *packet){
    packet->init(len, data, flag);
    u_short check_sum = compute_check_sum((u_short*)packet->buff, sizeof(SendPacket::Packet) / 2);
    packet->buff->check_sum = check_sum;
    show_send_pkt(packet);
    udp_send((char*)packet->buff, sizeof(SendPacket::Packet));
}

void rdt_resend(SendPacket *packet){
    show_send_pkt(packet);
    udp_send((char*)packet->buff, sizeof(SendPacket::Packet));
}

void udp_receive(char *message, int size) {
    bzero(message, size);
    long recv_num = recvfrom(client_socket, message, size, 0, (struct sockaddr *)&server_addr, &ADDR_LEN);
}

bool rdt_receive(RecPacket *packet){
    char message[sizeof(RecPacket::Packet)];
    udp_receive(message, sizeof(RecPacket::Packet));
    packet->extract_pkt(message);
    show_rec_pkt(packet);
    if(compute_check_sum((u_short *)packet->buff, sizeof(RecPacket::Packet) / 2) != 0){
        return false;
    } else return packet->buff->ack == SEQ;
}

bool rdt_init(char *server_ip, int server_port){

    server_addr.sin_len = sizeof(sockaddr_in);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    bzero(&(server_addr.sin_zero), 8);
    client_socket = socket(AF_INET, SOCK_DGRAM, 0);

    if (client_socket == -1){
        perror("Socket open error.");
        return false;
    }

    return true;
}



int main(int argc,char** argv)
{
    int server_port = 11332;
    char *server_ip = "127.0.0.1";
//    std :: cout << "请输入服务器ip地址: ";
//    std :: cin >> server_ip;
//    std :: cout << "请输入服务器对应端口号: ";
//    std :: cin >> server_port;

    if(! rdt_init("127.0.0.1", 11332)){
        std :: cout << "初始化失败\n"; //// 命名空间与bind函数冲突，所以不使用命名空间进行输入输出
        return 1;
    } else {
        std::cout << "初始化成功，准备发送文件\n";
    }

    auto *send_packet = new SendPacket;
    auto *rec_packet = new RecPacket;

    rdt_send(nullptr, 0, CON, send_packet);
    while (! rdt_receive(rec_packet)){
        rdt_resend(send_packet);
    }

    char* file_path = "/Users/dyf/Desktop/计算机网络/project/lab3/Reliable_Data_Transfer_Protocol_Based_UDP/任务1测试文件/1.jpg";
    char* file_name = "1.jpg";
    std :: ifstream in_file(file_path, std :: ios :: binary | std :: ios :: in);  //以二进制读模式打开文件
    if (!in_file) {
        perror(" Source file open error.\n");
        return -1;
    }

    rdt_send(file_name, strlen(file_name), BOF, send_packet);
    while (! rdt_receive(rec_packet)){
        rdt_resend(send_packet);
    }

    char data[MAX_DATA_BYTES];
    int len;

    while (! in_file.eof()){

        in_file.read(data, MAX_DATA_BYTES);
        len = in_file.gcount();

        rdt_send(data, len, SYN, send_packet);
        while (! rdt_receive(rec_packet)){
            rdt_resend(send_packet);
        }

    }

    rdt_send(nullptr, 0, DOF, send_packet);
    while (! rdt_receive(rec_packet)){
        rdt_resend(send_packet);
    }

    in_file.close();

    rdt_send(nullptr, 0, FIN, send_packet);
    while (! rdt_receive(rec_packet)){
        rdt_resend(send_packet);
    }

    close(client_socket);
    
    return 0;
}
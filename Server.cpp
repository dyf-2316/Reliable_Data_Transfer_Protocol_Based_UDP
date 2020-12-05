#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <zconf.h>

#include <iostream>
#include <fstream>

#define SOCKET int
#define MTU 2048
#define ACK_SIZE 4
#define MAX_DATA_BYTES MTU - sizeof(u_int) - 3 * sizeof(u_short)

#define CON 0X0001
#define BOF 0X0002
#define SYN 0X0004
#define DOF 0X0008
#define FIN 0X0010

socklen_t ADDR_LEN = sizeof(struct sockaddr_in);

SOCKET server_socket;
sockaddr_in client_addr{};

int ACK = 0;

struct RecPacket{
    struct Packet{
        u_int seq;
        u_short check_sum;
        u_short len;
        u_short flag;
        char data[MAX_DATA_BYTES];
    } * buff;

    RecPacket(){
        buff = new Packet;
    }

    void extract_pkt(char* message) {
        bzero(buff, sizeof(Packet));
        buff->seq = *((u_int *) &(message[0]));
        buff->check_sum = *((u_short *) &(message[4]));
        buff->len = *((u_short *)&(message[6]));
        buff->flag = *((u_short *)&(message[8]));
        bcopy((char *)&(message[10]), buff->data, MAX_DATA_BYTES);
    }
};

struct SendPacket{
    struct Packet{
        u_int ack;
        u_short check_sum;
    } * buff;

    SendPacket(){
        buff = new Packet;
    }

    void init(){
        bzero(buff, sizeof(Packet));
        buff->ack = ACK;
    }

    void make_pkt(u_short check_sum){
        buff->check_sum = check_sum;
    }
};

void show_rec_pkt(RecPacket* packet){
    std::cout << "- Send   seq: " << packet->buff->seq << "  len: " << packet->buff->len ;
    std::cout << "  checksum: " << packet->buff->check_sum << "  flag: " << packet->buff->flag << std :: endl;
}

void show_send_pkt(SendPacket* packet){
    std::cout << "\t- Rec   ack: " << packet->buff->ack << "  checksum: " << packet->buff->check_sum << std :: endl;
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

void udp_receive(char *message, int size){
    bzero(message, size);
    long recv_num = recvfrom(server_socket, message, size, 0, (struct sockaddr *)&client_addr, &ADDR_LEN);
}

void udp_send(char *package, int size){
    sendto(server_socket, package, size, 0, (struct sockaddr *)&client_addr, ADDR_LEN);
}

void rdt_send(SendPacket *packet){
    packet->init();
    u_short check_sum = compute_check_sum((u_short*)packet->buff, sizeof(SendPacket::Packet) / 2);
    packet->buff->check_sum = check_sum;
    show_send_pkt(packet);
    udp_send((char*)packet->buff, sizeof(SendPacket::Packet));
}

void rdt_resend(SendPacket *packet){
    udp_send((char*)packet->buff, sizeof(SendPacket::Packet));
}

bool rdt_receive(RecPacket *packet, u_short flag){
    char message[MTU];
    udp_receive(message, MTU);
    packet->extract_pkt(message);
    show_rec_pkt(packet);
    if(compute_check_sum((u_short*)packet->buff, sizeof(RecPacket::Packet) / 2) != 0){
        return false;
    } else if(packet->buff->seq == ACK && packet->buff->flag & flag){
        ACK ++;
        ACK %= ACK_SIZE;
        return true;
    } else return false;
}

bool rdt_init(char *server_ip, int server_port){

    server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == -1){
        perror("Socket open error.");
        return false;
    }

    sockaddr_in server_addr{}; // 这里是服务器地址，用于之后的连接，与服务器端创建方式相同
    server_addr.sin_len = sizeof(sockaddr_in);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);
    bzero(&(server_addr.sin_zero), 8);

    if (bind(server_socket, (sockaddr *) &server_addr, sizeof(server_addr)) == -1){
        perror("Socket bind error");
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
        std :: cout << "初始化失败\n"; //
        return 1;
    } else {
        std::cout << "初始化成功，准备接受文件\n";
    }

    auto *send_packet = new SendPacket;
    auto *rec_packet = new RecPacket;

    while (! rdt_receive(rec_packet, CON)){
        rdt_send(send_packet);
    }
    rdt_send(send_packet);

    std :: string load_path = "/Users/dyf/Desktop/计算机网络/project/lab3/Reliable_Data_Transfer_Protocol_Based_UDP/download/";

    while (! rdt_receive(rec_packet, BOF)){
        rdt_send(send_packet);
    }
    rec_packet->buff->data[rec_packet->buff->len] = '\0';
    std :: string file_name = (rec_packet->buff->data);
    rdt_send(send_packet);

    std :: ofstream out_file(load_path + file_name, std :: ios :: binary | std :: ios :: out);  //以二进制写模式打开文件
    if (!out_file) {
        perror("File open error.\n");
        return -1;
    }


    while (true){
        while ( !rdt_receive(rec_packet,SYN | DOF)) {
            rdt_send(send_packet);
        }
        if(rec_packet->buff->flag == DOF){
            break;
        }
        rdt_send(send_packet);
        out_file.write(rec_packet->buff->data, rec_packet->buff->len);
    }
    rdt_send(send_packet);

    out_file.close();

    while (!rdt_receive(rec_packet, FIN)){
        rdt_send(send_packet);
    }

    close(server_socket);

    return 0;
}
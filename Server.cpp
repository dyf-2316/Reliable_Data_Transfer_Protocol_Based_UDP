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
#define RES 0X0011

#define DEFAULT_PORT 11332
#define DEFAULT_IP_ADDR "127.0.0.1"
#define DEFAULT_LOAD_PATH "./download/"

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
//    show_send_pkt(packet);
    udp_send((char*)packet->buff, sizeof(SendPacket::Packet));
}

bool rdt_receive(RecPacket *packet, u_short flag){
    char message[MTU];
    udp_receive(message, MTU);
    packet->extract_pkt(message);
//    show_rec_pkt(packet);
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
    int server_port ;
    std :: string server_ip ;

    std :: cout << "请输入服务器ip地址: ";
    std :: cin >> server_ip;
    if(server_ip == "-1"){
        std :: cout << "\t默认端口号为: " << DEFAULT_IP_ADDR << "\n";
        server_ip = DEFAULT_IP_ADDR;
    }

    std :: cout << "请输入服务器对应端口号: ";
    std :: cin >> server_port;
    if( server_port == -1){
        std :: cout << "\t默认端口号为: " << DEFAULT_PORT << "\n";
        server_port = DEFAULT_PORT;
    }

    std :: string file_name;

    std :: string load_path ;
    std :: cout << "请输入下载路径: ";
    std :: cin >> load_path;
    if(load_path == "-1"){
        if (0 != access("./download", 0))
        {
            system("mkdir ./download");
        }
        std :: cout << "\t默认下载路径: " << DEFAULT_LOAD_PATH << "\n";
        load_path = DEFAULT_LOAD_PATH;
    }

    if(! rdt_init((char*)server_ip.c_str(), server_port)){
        std :: cout << "初始化失败\n"; //
        return 1;
    }else {
        std::cout << "服务器初始化成功，正在与客户端建立连接\n";
    }

    int state = 111;

    auto *send_packet = new SendPacket;
    auto *rec_packet = new RecPacket;

    std :: ofstream *out_file;

    while (state){
        switch (state){
            case 111:
                while (! rdt_receive(rec_packet, CON | RES)){
                    rdt_send(send_packet);
                }
                if(rec_packet->buff->flag == CON) {
                    rdt_send(send_packet);
                    std::cout << "客户端连接建立成功" << std::endl;
                    state = 222;
                } else if(rec_packet->buff->flag == RES){
                    state = 666;
                    continue;
                }

                while (! rdt_receive(rec_packet, BOF|FIN|RES)){
                    rdt_send(send_packet);
                }
                if(rec_packet->buff->flag == BOF){
                    state = 222;
                } else if (rec_packet->buff->flag == FIN){
                    state = 555;
                } else if (rec_packet->buff->flag == RES){
                    state = 666;
                    continue;
                }
                break;
            case 222:
                rec_packet->buff->data[rec_packet->buff->len] = '\0';
                file_name = (rec_packet->buff->data);
                rdt_send(send_packet);
                out_file = new std :: ofstream(load_path + file_name, std :: ios :: binary | std :: ios :: out);  //以二进制写模式打开文件
                if (!*out_file) {
                    perror("File open error.\n");
                    return -1;
                }
                std :: cout << file_name << "文件正在传输" << std :: endl;
                state = 333;
                break;
            case 333:
                while (true){
                    while ( !rdt_receive(rec_packet,SYN | DOF | RES)) {
                        rdt_send(send_packet);
                    }
                    if(rec_packet->buff->flag == DOF){
                        break;
                    } else if(rec_packet->buff->flag == RES){
                        state = 666;
                        break;
                    } else if(rec_packet->buff->flag == SYN){
                        rdt_send(send_packet);
                        out_file->write(rec_packet->buff->data, rec_packet->buff->len);
                    }

                }
                rdt_send(send_packet);
                state = 444;
                break;
            case 444:
                std :: cout << file_name << "文件传输成功" << std :: endl;
                out_file->close();
                delete out_file;
                while (! rdt_receive(rec_packet, BOF|FIN)){
                    rdt_send(send_packet);
                }
                if(rec_packet->buff->flag == BOF){
                    state = 222;
                } else if (rec_packet->buff->flag == FIN){
                    state = 555;
                } else if (rec_packet->buff->flag == RES){
                    state = 666;
                }
                break;
            case 555:
                rdt_send(send_packet);
                std :: cout << "客户端连接断开成功" << std :: endl;
                state = 0;
                break;
            case 666:
                std :: cout << "客户端强制断开连接" << std :: endl;
                state = 0;
        }
    }

    close(server_socket);

    return 0;
}
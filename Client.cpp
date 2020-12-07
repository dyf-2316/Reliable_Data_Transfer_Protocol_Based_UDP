#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <zconf.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/time.h>

#include <iostream>
#include <fstream>

#define SOCKET int
#define MTU 2048
#define MAX_DATA_BYTES MTU - sizeof(u_int) - 3 * sizeof(u_short)
#define MAX_RN 5

#define DEFAULT_PORT 11332
#define DEFAULT_IP_ADDR "127.0.0.1"

socklen_t ADDR_LEN = sizeof(struct sockaddr_in);

SOCKET client_socket;
sockaddr_in server_addr{};

struct timeval timeout = {3,0};

#define CON 0X0001
#define BOF 0X0002
#define SYN 0X0004
#define DOF 0X0008
#define FIN 0X0010
#define RES 0X0011

int SEQ = 0;

int RN = 0;

class Timer{
    double _begin;
    double _end;
    double get_time(){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
public:

    void begin(){
        _begin = get_time();
    }
    void end(){
        _end = get_time();
    }

    double show(){
        return _end - _begin;
    }
};

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
    packet->make_pkt(check_sum);
//    show_send_pkt(packet);
    udp_send((char*)packet->buff, sizeof(SendPacket::Packet));
}

void rdt_resend(SendPacket *packet){
//    show_send_pkt(packet);
    udp_send((char*)packet->buff, sizeof(SendPacket::Packet));
}

bool udp_receive(char *message, int size) {
    bzero(message, size);
    return recvfrom(client_socket, message, size, 0, (struct sockaddr *)&server_addr, &ADDR_LEN) == -1;
}

bool rdt_receive(RecPacket *packet){
    char message[sizeof(RecPacket::Packet)];
    if(udp_receive(message, sizeof(RecPacket::Packet))){
        RN++;
        if (RN == MAX_RN){
            return true;
        }
        return false;
    }
    packet->extract_pkt(message);
//    show_rec_pkt(packet);
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

void set_receive_timeout(){
    setsockopt(client_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout,sizeof(struct timeval));
}



int main(int argc,char** argv)
{
    int server_port = 11332;
    std :: string server_ip = "127.0.0.1";
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

    if(! rdt_init((char*)server_ip.c_str(), server_port)){
        perror("Socket initialize error.\n");
        return 1;
    } else {
        std::cout << "客户端初始化成功，正在与服务器建立连接\n";
    }

    set_receive_timeout();

    auto *send_packet = new SendPacket;
    auto *rec_packet = new RecPacket;

    int state = 111;

    std :: string file_path, file_name;

    std :: ifstream *in_file;

    Timer timer;

    char data[MAX_DATA_BYTES];
    int len = MAX_DATA_BYTES;

    while (state){
        switch (state){
            case 111:
                rdt_send(nullptr, 0, CON, send_packet);
                while (! rdt_receive(rec_packet)){
                    rdt_resend(send_packet);
                }
                if(RN == MAX_RN){
                    state = 777;
                    continue ;
                }
                RN = 0;
                state = 222;
                std :: cout << "服务器连接建立成功" << std :: endl;
                std :: cout << "请输入文件路径: ";
                break;
            case 222:
                std :: cin >> file_path;
                if (file_path == "QUIT"){
                    state = 666;
                    continue;
                }
                in_file = new std :: ifstream(file_path, std :: ios :: binary | std :: ios :: in);  //以二进制读模式打开文件
                if (!*in_file ) {
                    std :: cout << "该文件不存在请重新输入: ";
                    continue;
                }
                file_name = file_path.substr(file_path.rfind('/')+1);
                state = 333;
                break;
            case 333:
                rdt_send((char*)file_name.c_str(), file_name.length(), BOF, send_packet);
                while (! rdt_receive(rec_packet)) {
                    rdt_resend(send_packet);
                }
                if(RN == MAX_RN){
                    state = 777;
                    continue ;
                }
                RN = 0;
                std :: cout << file_name << " 传输开始" << std :: endl;
                state = 444;
                break;
            case 444:
                timer.begin();
                while (! in_file->eof()){
                    in_file->read(data, MAX_DATA_BYTES);
                    rdt_send(data, in_file->gcount(), SYN, send_packet);
                    while (! rdt_receive(rec_packet)){
                        rdt_resend(send_packet);
                    }
                    if(RN == MAX_RN){
                        state = 777;
                        continue ;
                    }
                    RN = 0;
                }
                state = 555;
                timer.end();
                break;
            case 555:
                std :: cout << file_name << " 传输完成, 总计用时" <<timer.show()<< "ms" << std :: endl;
                rdt_send(nullptr, 0, DOF, send_packet);
                while (! rdt_receive(rec_packet)){
                    rdt_resend(send_packet);
                }
                if(RN == MAX_RN){
                    state = 777;
                    continue ;
                }
                RN = 0;
                in_file->close();
                delete in_file;
                state = 222;
                std :: cout << "请输入文件路径：";
                break;
            case 666:
                rdt_send(nullptr, 0, FIN, send_packet);
                while (! rdt_receive(rec_packet)){
                    rdt_resend(send_packet);
                }
                std :: cout << "连接断开成功" << std :: endl;
                state = 0;
                break;
            case 777:
                rdt_send(nullptr, 0, RES, send_packet);
                std :: cout << "服务器无响应，强制连接断开！" << std :: endl;
                state = 0;
                break;
        }

    }

    close(client_socket);

    return 0;
}
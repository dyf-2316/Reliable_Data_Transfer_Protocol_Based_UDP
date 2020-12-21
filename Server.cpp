#include <cstdio>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <zconf.h>
#include <sys/time.h>

#include <iostream>
#include <c++/v1/fstream>

#define SOCKET int
#define MTU 2048
#define ACK_SIZE UINT32_MAX
#define MSS (MTU - sizeof(u_int) - 3 * sizeof(u_short))

#define CON 0X0001
#define BOF 0X0002
#define SYN 0X0004
#define DOF 0X0008
#define FIN 0X0010
#define RES 0X0011

#define MAX_BUF 32768;
#define DEFAULT_PORT 11332
#define DEFAULT_IP_ADDR "127.0.0.1"
#define DEFAULT_LOAD_PATH "./download/"

socklen_t ADDR_LEN = sizeof(struct sockaddr_in);


SOCKET server_socket;
sockaddr_in client_addr{};

u_int ACK = 0;

struct timeval rec_timeout = {0, 5};


class Timer{
    long _begin;
    long get_time(){
        struct timeval tv{};
        gettimeofday(&tv, nullptr);
        return tv.tv_sec * 1000000 + tv.tv_usec ;
    }
public:

    void begin(){
        _begin = get_time();
    }

    long end(){
        return get_time() - _begin;
    }

};

struct RecPacket{
    struct Packet{
        u_int seq;
        u_short check_sum;
        u_short len;
        u_short flag;
        char data[MSS];
    } * buff;

    bool timeout_rec;

    RecPacket(){
        buff = new Packet;
    }

    void extract_pkt(char* message) {
        bzero(buff, sizeof(Packet));
        timeout_rec = false;
        buff->seq = *((u_int *) &(message[0]));
        buff->check_sum = *((u_short *) &(message[4]));
        buff->len = *((u_short *)&(message[6]));
        buff->flag = *((u_short *)&(message[8]));
        bcopy((char *)&(message[10]), buff->data, buff->len);
    }
};

struct SendPacket{
    struct Packet{
        u_int ack;
        u_short check_sum;
        u_short window;
    } * buff;

    SendPacket(){
        buff = new Packet;
    }

    void init(u_short window){
        bzero(buff, sizeof(Packet));
        buff->ack = ACK;
        buff->window = window;
    }

    void make_pkt(u_short check_sum){
        buff->check_sum = check_sum;
    }
};

void show_rec_pkt(RecPacket* packet){
    std::cout << "\t- Rec   seq: " << packet->buff->seq << "  len: " << packet->buff->len ;
    std::cout << "  checksum: " << packet->buff->check_sum << "  flag: " << packet->buff->flag << std :: endl;
}

void show_send_pkt(SendPacket* packet){
    std::cout << "- Send   ack: " << packet->buff->ack << "  checksum: " << packet->buff->check_sum << "  window: " << packet->buff->window << std :: endl;
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

bool udp_receive(char *message, int size){
    bzero(message, size);
    return recvfrom(server_socket, message, size, 0, (struct sockaddr *)&client_addr, &ADDR_LEN) != -1;
}

void udp_send(char *package, int size){
    sendto(server_socket, package, size, 0, (struct sockaddr *)&client_addr, ADDR_LEN);
}

void rdt_send(SendPacket *packet, u_short window){
    packet->init(window);
    u_short check_sum = compute_check_sum((u_short*)packet->buff, sizeof(SendPacket::Packet) / 2);
    packet->make_pkt(check_sum);
    show_send_pkt(packet);
    udp_send((char*)packet->buff, sizeof(SendPacket::Packet));
}

bool rdt_receive(RecPacket *packet, u_short flag){
    char message[MTU];
    if(! udp_receive(message, MTU)){
        packet->timeout_rec = true;
        return true;
    }
    packet->extract_pkt(message);
    show_rec_pkt(packet);
    if(compute_check_sum((u_short*)packet->buff, sizeof(RecPacket::Packet) / 2) != 0){
        return false;
    } else if(packet->buff->seq == ACK && packet->buff->flag & flag){
        ACK ++;
        ACK %= UINT32_MAX;
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

void set_receive_timeout(){
    setsockopt(server_socket, SOL_SOCKET, SO_RCVTIMEO, (char *)&rec_timeout,sizeof(struct timeval));
}

int main(int argc,char** argv)
{
    int server_port = DEFAULT_PORT;
    std :: string server_ip = DEFAULT_IP_ADDR;

//    std :: cout << "请输入服务器ip地址: ";
//    std :: cin >> server_ip;
//    if(server_ip == "-1"){
//        std :: cout << "\t默认端口号为: " << DEFAULT_IP_ADDR << "\n";
//        server_ip = DEFAULT_IP_ADDR;
//    }

//    std :: cout << "请输入服务器对应端口号: ";
//    std :: cin >> server_port;
//    if( server_port == -1){
//        std :: cout << "\t默认端口号为: " << DEFAULT_PORT << "\n";
//        server_port = DEFAULT_PORT;
//    }

    std :: string file_name ;

    std :: string load_path = DEFAULT_LOAD_PATH;

//    std :: cout << "请输入下载路径: ";
//    std :: cin >> load_path;
//    if(load_path == "-1"){
//        if (0 != access("./download", 0))
//        {
//            system("mkdir ./download");
//        }
//        std :: cout << "\t默认下载路径: " << DEFAULT_LOAD_PATH << "\n";
//        load_path = DEFAULT_LOAD_PATH;
//    }

    int max_rec_window = MAX_BUF;
    std :: cout << "请输入接收端窗口大小: ";
    std :: cin >> max_rec_window;
    if( max_rec_window == -1){
        std :: cout << "\t默认发送端窗口为: " << MAX_BUF ;
        std :: cout << "\n";
        max_rec_window = MAX_BUF;
    }
    u_short rec_window = max_rec_window;
    char rec_buf[rec_window];

    if(! rdt_init((char*)server_ip.c_str(), server_port)){
        std :: cout << "初始化失败\n"; //
        return 1;
    }else {
        std::cout << "服务器初始化成功，正在与客户端建立连接\n";
    }

    int state = 100;

    set_receive_timeout();
    auto *send_packet = new SendPacket;
    auto *rec_packet = new RecPacket;

    Timer Ack_timer{};
    u_int Ack_delay_time = 5;

    bool timeout_send = true;

    std :: ofstream *out_file = nullptr;

    while (state){
        switch (state){
            case 100:
                while (! rdt_receive(rec_packet, CON | RES)){
                    rdt_send(send_packet, rec_window);
                }
                if(rec_packet->timeout_rec){
                    rec_packet->timeout_rec = false;
                    break;
                }
                if(rec_packet->buff->flag == CON) {
                    rdt_send(send_packet, rec_window);
                    rec_packet->buff->data[rec_packet->buff->len] = '\0';
                    std::cout << "发送端连接建立成功，发送端窗口大小为 " << atoi(rec_packet->buff->data) << std::endl;
                    state = 110;
                } else if(rec_packet->buff->flag == RES){
                    state = 600;
                    continue;
                }
                break;
            case 110:
                while (! rdt_receive(rec_packet, BOF|FIN|RES|CON)){
                    rdt_send(send_packet, rec_window);
                }
                if(rec_packet->timeout_rec){
                    rec_packet->timeout_rec = false;
                    break;
                }
                if(rec_packet->buff->flag == BOF){
                    state = 200;
                } else if (rec_packet->buff->flag == FIN){
                    state = 500;
                } else if (rec_packet->buff->flag == RES){
                    state = 600;
                    continue;
                } else if(rec_packet->buff->flag == CON) {
                    rdt_send(send_packet, rec_window);
                    rec_packet->buff->data[rec_packet->buff->len] = '\0';
                    std::cout << "发送端窗口更改为 " << atoi(rec_packet->buff->data) << std::endl;
                    state = 110;
                }
                break;
            case 200:
                rec_packet->buff->data[rec_packet->buff->len] = '\0';
                file_name = (rec_packet->buff->data);
                rdt_send(send_packet, rec_window);
                out_file = new std :: ofstream(load_path + file_name, std :: ios :: binary | std :: ios :: out);  //以二进制写模式打开文件
                if (!*out_file) {
                    perror("File open error.\n");
                    return -1;
                }
                std :: cout << file_name << "文件正在传输" << std :: endl;
                state = 300;
                Ack_timer.begin();
                break;
            case 300:
                while ( !rdt_receive(rec_packet,SYN | DOF | RES)) {
                    rdt_send(send_packet, rec_window);
                }
                if(rec_packet->timeout_rec){
                    if(Ack_timer.end() > Ack_delay_time && timeout_send){
                        rdt_send(send_packet, rec_window);
                        Ack_timer.begin();
                        timeout_send = false;
                    }
                    rec_packet->timeout_rec = false;
                    break;
                } else{
                    timeout_send = true;
                }
                if(rec_packet->buff->flag == DOF){
                    out_file->write(rec_buf, max_rec_window - rec_window);
                    bzero(rec_buf, max_rec_window);
                    rec_window = max_rec_window;
                    rdt_send(send_packet, rec_window);
                    state = 400;
                    break;
                } else if(rec_packet->buff->flag == RES){
                    state = 600;
                    break;
                } else if(rec_packet->buff->flag == SYN){
                    if(rec_window - rec_packet->buff->len <= 0){
                        out_file->write(rec_buf, max_rec_window - rec_window);
                        bzero(rec_buf, max_rec_window);
                        rec_window = max_rec_window;
                        bcopy(rec_packet->buff->data, rec_buf, rec_packet->buff->len);
                        rec_window -= rec_packet->buff->len;
                    } else {
                        bcopy(rec_packet->buff->data, &rec_buf[max_rec_window - rec_window], rec_packet->buff->len);
                        rec_window -= rec_packet->buff->len;
                    }
                    if(Ack_timer.end() > Ack_delay_time){
                        rdt_send(send_packet, rec_window);
                        Ack_timer.begin();
                        timeout_send = false;
                    }
                }
                break;
            case 400:
                std :: cout << file_name << "文件传输成功" << std :: endl;
                out_file->close();
                delete out_file;
                state = 410;
                break;
            case 410:
                while (! rdt_receive(rec_packet, BOF|FIN|CON)){
                    rdt_send(send_packet, rec_window);
                }
                if(rec_packet->buff->flag == BOF){
                    state = 200;
                } else if (rec_packet->buff->flag == FIN){
                    state = 500;
                } else if (rec_packet->buff->flag == RES){
                    state = 600;
                } else if(rec_packet->buff->flag == CON) {
                    rdt_send(send_packet, rec_window);
                    rec_packet->buff->data[rec_packet->buff->len] = '\0';
                    std::cout << "发送端窗口更改为 " << atoi(rec_packet->buff->data) << std::endl;
                    state = 110;
                }
                break;
            case 500:
                rdt_send(send_packet, rec_window);
                std :: cout << "发送端连接断开成功" << std :: endl;
                state = 0;
                break;
            case 600:
                std :: cout << "发送端强制断开连接" << std :: endl;
                state = 0;
        }
    }

    close(server_socket);

    return 0;
}
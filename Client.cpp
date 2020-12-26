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
#include <queue>

#define SOCKET int
#define MTU 2048
#define MSS (MTU - sizeof(u_int) - 3 * sizeof(u_short))
#define MAX_RC 20
#define RTO 50

#define DEFAULT_PORT 11332
#define DEFAULT_IP_ADDR "127.0.0.1"
#define DEFAULT_SEND_WIN 4

socklen_t ADDR_LEN = sizeof(struct sockaddr_in);

SOCKET client_socket;
sockaddr_in server_addr{};

struct timeval timeout = {0,1000};

#define CON 0X0001
#define BOF 0X0002
#define SYN 0X0004
#define DOF 0X0008
#define FIN 0X0010
#define RES 0X0011

u_int SEQ = 0;

u_int base = 0;
u_int nextseqnum = 0;

class Timer{
    double _begin;
    double get_time(){
        struct timeval tv;
        gettimeofday(&tv, NULL);
        return tv.tv_sec * 1000 + tv.tv_usec / 1000;
    }
public:

    void begin(){
        _begin = get_time();
    }

    double end(){
        return get_time() - _begin;
    }

};

struct SendPacket{
    struct Packet{
        u_int seq;
        u_short check_sum;
        u_short len;
        u_short flag;
        char data[MSS];
    } buff;

    u_int size ;

    SendPacket(){
    }

    SendPacket(SendPacket *packet){
        bzero(&buff, sizeof(Packet));
        buff.len = packet->buff.len;
        buff.seq = packet->buff.seq;
        buff.flag = packet->buff.flag;
        buff.check_sum = packet->buff.check_sum;
        bcopy(packet->buff.data, this->buff.data, packet->buff.len);
        size = packet->size;
    }


    void init(u_short len, char *data, u_short flag){
        bzero(&buff, sizeof(Packet));
        size = 0;
        buff.seq = SEQ ++;
        SEQ %= UINT32_MAX;
        buff.check_sum = 0;
        buff.len = len;
        buff.flag = flag;
        if (data) {
            bcopy(data, buff.data, buff.len);
        }
        size += len + MTU - MSS;
        size = ((size % 2) ? size+1 : size);
    }

    void make_pkt(u_short check_sum){
        buff.check_sum = check_sum;
    }

};

std :: vector<SendPacket> sndpkt;

struct RecPacket{
    struct Packet{
        u_int ack;
        u_short check_sum;
        u_short window;
    } * buff;

    RecPacket(){
        buff = new Packet;
    };

    void extract_pkt(const char* message) {
        bzero(buff, sizeof(Packet));
        buff->ack = *((u_int *) &(message[0]));
        buff->check_sum = *((u_short *) &(message[4]));
        buff->window = *((u_short *) &(message[6]));
    }
};

void show_send_pkt(SendPacket* packet){
    std::cout << "- Send   seq: " << packet->buff.seq << "  len: " << packet->buff.len ;
    std::cout << "  checksum: " << packet->buff.check_sum << "  flag: " << packet->buff.flag << "  size: " << packet->size << std :: endl;
}

void show_rec_pkt(RecPacket* packet){
    std::cout << "\t- Rec   ack: " << packet->buff->ack << "  checksum: " << packet->buff->check_sum << "  window: " << packet->buff->window <<std :: endl;
}

void udp_send(char *package, int size){
    sendto(client_socket, package, size, 0, (struct sockaddr *)&server_addr, ADDR_LEN);
}

u_short compute_check_sum(u_short *data, u_int count){
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
    u_short check_sum = compute_check_sum((u_short*)&packet->buff, packet->size/2);
    packet->make_pkt(check_sum);
    show_send_pkt(packet);
    if(packet->buff.seq == 9)
        return;
    udp_send((char*)&packet->buff, packet->size);
}

void rdt_resend(SendPacket *packet){
    std :: cout << "R ";
    show_send_pkt(packet);
    udp_send((char*)&packet->buff, packet->size);
}

bool udp_receive(char *message, int size) {
    bzero(message, size);
    return recvfrom(client_socket, message, size, 0, (struct sockaddr *)&server_addr, &ADDR_LEN) != -1;
}

bool rdt_receive(RecPacket *packet){
    char message[sizeof(RecPacket::Packet)];
    if(! udp_receive(message, sizeof(RecPacket::Packet))){
        return false;
    }
    packet->extract_pkt(message);
    show_rec_pkt(packet);
    return compute_check_sum((u_short *) packet->buff, sizeof(RecPacket::Packet) / 2) == 0;
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

int get_file_size(const std::string& file_path){
    std::ifstream in(file_path.c_str());
    in.seekg(0, std::ios::end);
    size_t size = in.tellg();
    in.close();
    return size; //单位是：byte
}



int main(int argc,char** argv)
{
    int server_port = DEFAULT_PORT;
    std :: string server_ip = DEFAULT_IP_ADDR;
    std :: cout << "请输入接收端ip地址: ";
    std :: cin >> server_ip;
    if(server_ip == "-1"){
        std :: cout << "\t默认端口号为: " << DEFAULT_IP_ADDR << "\n";
        server_ip = DEFAULT_IP_ADDR;
    }

    std :: cout << "请输入接收端对应端口号: ";
    std :: cin >> server_port;
    if( server_port == -1){
        std :: cout << "\t默认端口号为: " << DEFAULT_PORT << "\n";
        server_port = DEFAULT_PORT;
    }

    if(! rdt_init((char*)server_ip.c_str(), server_port)){
        perror("Socket initialize error.\n");
        return 1;
    } else {
        std::cout << "发送端初始化成功，正在与接收端建立连接\n";
    }

    set_receive_timeout();

    auto *send_packet = new SendPacket;
    auto *rec_packet = new RecPacket;

    int state = 100;

    u_short recv_window = 0;
    u_short send_window = 1;

    int dep_ACK_count = 0;
    u_int dep_ACK = 0;
    u_int  ACK_count = 0;

    std :: string file_path, file_name;

    std :: ifstream *in_file = nullptr;

    Timer Transfer_timer{}, RTO_timer{};

    bool is_eof = false;

    char data[MSS];
    int len = 0;

    int resend_count = 0;

    double end_transfer = 0;

    int ssthresh = 32;

    while (state){
        switch (state){
            case 100:
                rdt_send((char *)std :: to_string(send_window).c_str(), std :: to_string(send_window).size(), CON, send_packet);
                RTO_timer.begin();
                nextseqnum ++;
                while (! rdt_receive(rec_packet)){
                    if(RTO_timer.end() > RTO){
                        resend_count ++;
                        RTO_timer.begin();
                        rdt_resend(send_packet);
                    }
                    if (resend_count == MAX_RC) break;
                }
                if(resend_count == MAX_RC){
                    state = 700;
                    continue ;
                }
                base ++;
                dep_ACK = base;
                resend_count = 0;
                state = 200;
                std :: cout << "接收端连接建立成功" << std :: endl;
                std :: cout << "请输入文件路径: ";
                break;
            case 200:
                std :: cin >> file_path;
                if (file_path == "QUIT"){
                    state = 600;
                    continue;
                } else if (file_path == "CONFIG") {
                    state = 210;
                    continue;
                }
                in_file = new std :: ifstream(file_path, std :: ios :: binary | std :: ios :: in);  //以二进制读模式打开文件
                if (!*in_file ) {
                    std :: cout << "该文件不存在请重新输入: ";
                    continue;
                }
                file_name = file_path.substr(file_path.rfind('/')+1);
                state = 300;
                break;
            case 210:
                std :: cout << "请输入发送端固定窗口大小: ";
                std :: cin >> send_window;
                state = 100;
                break;
            case 300:
                rdt_send((char*)file_name.c_str(), file_name.length(), BOF, send_packet);
                RTO_timer.begin();
                nextseqnum ++;
                while (! rdt_receive(rec_packet)) {
                    if(RTO_timer.end() > RTO){
                        resend_count ++;
                        rdt_resend(send_packet);
                        RTO_timer.begin();
                    }
                    if (resend_count == MAX_RC) break;
                }
                if(resend_count == MAX_RC){
                    state = 700;
                    continue ;
                }
                resend_count = 0;
                base ++;
                dep_ACK = base;
                recv_window = rec_packet->buff->window;
                std :: cout << file_name << " 传输开始" << std :: endl;
                send_window = 1;
                ssthresh = 32;
                ACK_count = 0;
                Transfer_timer.begin();
                state = 400;
                break;
            case 400:
                is_eof = in_file->eof();
                if((! is_eof && (nextseqnum < base + send_window)) && (recv_window > 0)) {
                    len = std :: min((int)MSS, (int)recv_window);
                    in_file->read(data, len);
                    len = in_file->gcount();
                    recv_window -= len;
                    rdt_send(data, len, SYN, send_packet);
                    if(base == nextseqnum){
                        RTO_timer.begin();
                    }
                    sndpkt.emplace_back(*send_packet);
                    nextseqnum ++;
                }else {
                    state = 410;
                }
                break;
            case 410:
                while (! rdt_receive(rec_packet)){
                    if(RTO_timer.end() > RTO){
                        ssthresh = (send_window/2)%2 ? send_window / 2 + 1 : send_window / 2;
                        send_window = 1;
                        std :: cout << "接收超时，窗口改为" << send_window << "\n";
                        ACK_count = 0;
                        resend_count ++;
                        RTO_timer.begin();
                        for (auto packet : sndpkt){
                            rdt_resend(&packet);
                        }
                    }
                    if (resend_count == MAX_RC) break;
                }
                if(resend_count == MAX_RC){
                    state = 700;
                    continue ;
                }
                if(send_window < ssthresh) state = 411;
                else state = 412;
                break;
            case 411:
                if(rec_packet->buff->ack == dep_ACK){
                    dep_ACK_count++;
                } else{
                    dep_ACK_count = 0;
                    send_window = send_window + rec_packet->buff->ack - base;
                    std :: cout << "接受新的ACK，窗口改为" << send_window << "\n";
                }
                state = 420;
                break;
            case 412:
                if(rec_packet->buff->ack == dep_ACK){
                    dep_ACK_count++;
                } else{
                    dep_ACK_count = 0;
                    ACK_count += rec_packet->buff->ack - base;
                    if(ACK_count >= send_window){
                        ACK_count -= send_window;
                        send_window = send_window + 1;
                        std :: cout << "接受新的RTT，窗口改为" << send_window << "\n";
                    }
                }
                state = 420;
                break;
            case 420:
                if(dep_ACK_count == 3){
                    for (auto packet : sndpkt){
                        std :: cout << "3";
                        rdt_resend(&packet);
                    }
                    ssthresh = (send_window/2)%2 ? send_window / 2 + 1 : send_window / 2;
                    send_window = ssthresh + 3;
                    std :: cout << "快速重传，窗口改为" << send_window << "\n";
                    ACK_count = 0;
                }
                state = 430;
                break;
            case 430:
                resend_count = 0;
                base = rec_packet->buff->ack;
                dep_ACK = base;
                recv_window = rec_packet->buff->window;
                while ( !sndpkt.empty() && sndpkt.front().buff.seq < base){
                    sndpkt.erase(sndpkt.begin());
                }
                RTO_timer.begin();
                if(is_eof && sndpkt.empty()){
                    end_transfer = Transfer_timer.end();
                    state = 500;
                } else if (!is_eof ){
                    state = 400;
                } else{
                    state = 410;
                }
                break;
            case 500:
                std :: cout << file_name << " 传输完成, 总计用时" << end_transfer << "ms" ;
                std :: cout << ", 吞吐率为"<< get_file_size(file_path) / end_transfer / 1000 << "MB/s"<< std :: endl;
                rdt_send(nullptr, 0, DOF, send_packet);
                nextseqnum ++;
                while (! rdt_receive(rec_packet)){
                    rdt_resend(send_packet);
                }
                if(resend_count == MAX_RC){
                    state = 700;
                    continue ;
                }
                base++;
                resend_count = 0;
                in_file->close();
                delete in_file;
                state = 200;
                std :: cout << "请输入文件路径：";
                break;
            case 600:
                rdt_send(nullptr, 0, FIN, send_packet);
                nextseqnum ++;
                while (! rdt_receive(rec_packet)){
                    rdt_resend(send_packet);
                }
                base ++;
                std :: cout << "接收端连接断开成功" << std :: endl;
                state = 0;
                break;
            case 700:
                rdt_send(nullptr, 0, RES, send_packet);
                std :: cout << "接收端无响应，强制连接断开！" << std :: endl;
                state = 0;
                break;
        }

    }

    close(client_socket);

    return 0;
}
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h> // 系统级的类型和数据结构
#include <sys/socket.h> // socket相关
#include <pthread.h> // 多线程相关
#include <netinet/in.h> // IPv4 IPv6相关的定义和数据结构
#include <netinet/ip.h>
#include <netinet/ip6.h> // 补充，IPv6相关的数据结构、常量和宏
#include <net/ethernet.h> // 补充，以太网头相关的数据结构、常量和宏
#include <netinet/in_systm.h> // 补充，定义一些与网络协议和系统相关的数据类型和常量
#include <arpa/inet.h> // IP地址转换、字节序操作相关的函数
#include <pcap.h> // pcap库，packet capture

#include <time.h>
#include <unistd.h>

#define WINDOW_SIZE 40 // 支持的最大统计窗口（最多保留40秒的流量数据）

// 统计结构体
typedef struct {
    unsigned long total_bytes;                  // 累计总流量（单位：字节）
    unsigned long peak_rate;                    // 峰值（单位：字节/秒）
    unsigned long long bytes_window[WINDOW_SIZE]; // 每秒钟的流量环形缓存
    int current_second;                         // 当前环形缓存中的索引位置
    time_t last_time;                           // 上次统计的时间戳
} TrafficStat;

// 全局统计数据结构
TrafficStat stats = {0};

// 回调函数：处理每一个抓到的数据包
void packet_handler(u_char *args, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    struct ether_header *eth_hdr = (struct ether_header *)packet;

    // 只处理 IPv4 包
    if (ntohs(eth_hdr->ether_type) != ETHERTYPE_IP) return;

    // 获取 IP 头（位于以太网头之后）
    struct ip *ip_hdr = (struct ip *)(packet + sizeof(struct ether_header));
    int pkt_len = ntohs(ip_hdr->ip_len);  // 获取 IP 层长度

    // 打印包的基本信息
    printf("Src: %s -> Dst: %s | Len: %d bytes\n",
           inet_ntoa(ip_hdr->ip_src),
           inet_ntoa(ip_hdr->ip_dst),
           pkt_len);

    // 获取当前时间，更新滑动窗口索引
    time_t now = time(NULL);
    int delta = (int)(now - stats.last_time);

    if (delta >= 1) {
        // 每秒推进环形缓冲区的指针
        for (int i = 0; i < delta && i < WINDOW_SIZE; ++i) {
            stats.current_second = (stats.current_second + 1) % WINDOW_SIZE;
            stats.bytes_window[stats.current_second] = 0; // 清空该秒的数据
        }
        stats.last_time = now;
    }

    // 累计当前时间片的流量
    stats.bytes_window[stats.current_second] += pkt_len;
    stats.total_bytes += pkt_len;

    // 判断是否更新峰值
    if (stats.bytes_window[stats.current_second] > stats.peak_rate) {
        stats.peak_rate = stats.bytes_window[stats.current_second];
    }
}

// 计算最近N秒内的平均流量（字节/秒）
unsigned long average_rate(int window_seconds) {
    unsigned long sum = 0;

    for (int i = 0; i < window_seconds && i < WINDOW_SIZE; ++i) {
        int idx = (stats.current_second - i + WINDOW_SIZE) % WINDOW_SIZE;
        sum += stats.bytes_window[idx];
    }

    return sum / window_seconds;
}

int main() {
    char errbuf[PCAP_ERRBUF_SIZE]; // 错误缓冲区
    pcap_t *handle;
    const char *dev = "br-lan"; // OpenWRT 常用网络接口（可换成 eth0/wlan0）

    // 打开网卡进行抓包（混杂模式，捕获所有流量）
    handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (!handle) {
        fprintf(stderr, "Couldn't open device %s: %s\n", dev, errbuf);
        return 2;
    }

    printf("Capturing on interface %s...\n", dev);
    stats.last_time = time(NULL); // 初始化时间戳

    // 主循环：每秒打印一次统计数据
    for (;;) {
        // 捕获新数据包并触发回调（非阻塞）
        pcap_dispatch(handle, -1, packet_handler, NULL);

        // 每秒打印一次统计信息
        printf("[Stats] Total: %lu bytes | 2s avg: %lu B/s | 10s avg: %lu B/s | 40s avg: %lu B/s | Peak: %lu B/s\n",
               stats.total_bytes,
               average_rate(2),
               average_rate(10),
               average_rate(40),
               stats.peak_rate);

        sleep(1); // 等待1秒再继续
    }

    // 关闭抓包句柄
    pcap_close(handle);
    return 0;
}
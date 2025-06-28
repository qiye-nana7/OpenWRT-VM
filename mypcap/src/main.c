#include <stdio.h>
#include <stdlib.h>
#include <pcap.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <net/ethernet.h>  // Ethernet Header Definition
#include <netinet/ip.h>    // IPv4 Header Definition
#include <arpa/inet.h>     // inet_ntop
#include <pthread.h>

// 最大统计窗口大小
#define WINDOW_SIZE 40

// 最大可记录的接口数量
#define MAX_INTERFACES 10

// 每个接口最多记录多少个 IP 的流量
#define MAX_IPS_PER_IFACE 200

// 单个 IP 的流量统计结构
typedef struct {
    char ip[INET_ADDRSTRLEN];
    unsigned long bytes;
} IPStat;

// 网络流量统计结构
typedef struct {
    unsigned long total_bytes;                  // 累计总流量（单位：字节）
    unsigned long peak_rate;                    // 峰值（单位：字节/秒）
    unsigned long long bytes_window[WINDOW_SIZE]; // 每秒钟的流量环形缓存
    int current_second;                         // 当前环形缓存中的索引位置
    time_t last_time;                           // 上次统计的时间戳
    IPStat ip_stats[MAX_IPS_PER_IFACE];         // 每个 IP 的流量统计
    int num_ips;                                // 当前已记录的 IP 数量
} TrafficStat;

// 单个网卡信息结构
typedef struct {
    char ifname[64];
    char description[256];
    TrafficStat stats;
    pthread_t thread;
    pcap_t *handle;
} InterfaceInfo;

// 全局接口数组
InterfaceInfo g_interfaces[MAX_INTERFACES];
int g_num_interfaces = 0;

// 输出颜色定义
#define RESET   "\033[0m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define CYAN    "\033[36m"
#define RED     "\033[31m"
#define WHITE   "\033[37m"

// 计算最近 N 秒内的平均流量（字节/秒）
unsigned long average_rate(TrafficStat *stats, int window_seconds) {
    unsigned long sum = 0;

    for (int i = 0; i < window_seconds && i < WINDOW_SIZE; ++i) {
        int idx = (stats->current_second - i + WINDOW_SIZE) % WINDOW_SIZE;
        sum += stats->bytes_window[idx];
    }

    return window_seconds > 0 ? sum / window_seconds : 0;
}

// 检查接口是否为常用接口
int is_common_interface(const char *interface_name) {
    // 常用的接口名称
    const char *common_interfaces[] = {"eth0", "br-lan", "wlan0", "eth1", "lo"};
    size_t num_interfaces = sizeof(common_interfaces) / sizeof(common_interfaces[0]);

    for (size_t i = 0; i < num_interfaces; ++i) {
        if (strcmp(interface_name, common_interfaces[i]) == 0) {
            return 1;
        }
    }
    return 0;
}

// 在 IP 列表中查找或新建一个统计项
IPStat* find_or_create_ip_stat(TrafficStat *stats, const char *ip) {
    for (int i = 0; i < stats->num_ips; i++) {
        if (strcmp(stats->ip_stats[i].ip, ip) == 0) {
            return &stats->ip_stats[i];
        }
    }
    if (stats->num_ips < MAX_IPS_PER_IFACE) {
        strncpy(stats->ip_stats[stats->num_ips].ip, ip, INET_ADDRSTRLEN - 1);
        stats->ip_stats[stats->num_ips].bytes = 0;
        stats->num_ips++;
        return &stats->ip_stats[stats->num_ips - 1];
    }
    return NULL;
}

// 回调函数：每次捕获数据包时触发
void packet_handler(u_char *args, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    InterfaceInfo *iface = (InterfaceInfo *)args;
    struct ether_header *eth_hdr = (struct ether_header *)packet;

    // 只处理 IPv4 包
    if (ntohs(eth_hdr->ether_type) != ETHERTYPE_IP) return;

    // 获取 IP 头（位于以太网头之后）
    struct ip *ip_hdr = (struct ip *)(packet + sizeof(struct ether_header));
    int pkt_len = ntohs(ip_hdr->ip_len);

    // 获取源 IP 和目的 IP
    char src_ip[INET_ADDRSTRLEN];
    char dst_ip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(ip_hdr->ip_src), src_ip, INET_ADDRSTRLEN);
    inet_ntop(AF_INET, &(ip_hdr->ip_dst), dst_ip, INET_ADDRSTRLEN);

    // // 获取每个包时打印（调试选项）
    // printf(GREEN "[%s] 捕获数据包：Src: %s -> Dst: %s, 长度: %d\n" RESET,
    //        iface->ifname, src_ip, dst_ip, pkt_len);

    // 更新流量统计
    time_t now = time(NULL);
    int delta = (int)(now - iface->stats.last_time);

    if (delta >= 1) {
        for (int i = 0; i < delta && i < WINDOW_SIZE; ++i) {
            iface->stats.current_second = (iface->stats.current_second + 1) % WINDOW_SIZE;
            iface->stats.bytes_window[iface->stats.current_second] = 0; // 清空该秒数据
        }
        iface->stats.last_time = now;
    }

    iface->stats.bytes_window[iface->stats.current_second] += pkt_len;
    iface->stats.total_bytes += pkt_len;

    if (iface->stats.bytes_window[iface->stats.current_second] > iface->stats.peak_rate) {
        iface->stats.peak_rate = iface->stats.bytes_window[iface->stats.current_second];
    }

    // 更新单个 IP 的统计
    IPStat *src_stat = find_or_create_ip_stat(&iface->stats, src_ip);
    if (src_stat) src_stat->bytes += pkt_len;

    IPStat *dst_stat = find_or_create_ip_stat(&iface->stats, dst_ip);
    if (dst_stat) dst_stat->bytes += pkt_len;
}

// 每个接口的抓包线程
void *capture_thread(void *arg) {
    InterfaceInfo *iface = (InterfaceInfo *)arg;
    printf(YELLOW "开始抓取接口 [%s] 的数据...\n" RESET, iface->ifname);
    pcap_loop(iface->handle, -1, packet_handler, (u_char *)iface);
    return NULL;
}

// 显示所有接口的统计信息
void display_stats() {
    printf("\033[H\033[J");  // 清屏
    printf(CYAN "Network Interfaces and Real-time Traffic Monitor\n" RESET);

    for (int i = 0; i < g_num_interfaces; i++) {
        InterfaceInfo *iface = &g_interfaces[i];
        printf(GREEN "Interface %d: %s\n" RESET, i, iface->ifname);
        printf(CYAN "  Description: %s\n" RESET, iface->description);
        printf(YELLOW "  [统计] 累计: %lu bytes | "
                      RESET CYAN "2秒均值: %lu B/s | "
                                  "10秒均值: %lu B/s | "
                                  "40秒均值: %lu B/s | "
                      RESET RED "峰值: %lu B/s\n" RESET,
               iface->stats.total_bytes,
               average_rate(&iface->stats, 2),
               average_rate(&iface->stats, 10),
               average_rate(&iface->stats, 40),
               iface->stats.peak_rate);

        // 打印每个 IP 的流量
        for (int j = 0; j < iface->stats.num_ips; j++) {
            IPStat *ipstat = &iface->stats.ip_stats[j];
            printf(WHITE "    IP: %s -> %lu bytes\n" RESET,
                   ipstat->ip, ipstat->bytes);
        }
        printf("\n");
    }
}

int main() {
    char errbuf[PCAP_ERRBUF_SIZE];
    pcap_if_t *interfaces, *d;

    printf("启动网络流量监控...\n");

    if (pcap_findalldevs(&interfaces, errbuf) == -1) {
        fprintf(stderr, "查找设备失败: %s\n", errbuf);
        return 1;
    }

    // 遍历所有接口
    for (d = interfaces; d != NULL; d = d->next) {
        if (!is_common_interface(d->name)) continue;

        if (g_num_interfaces >= MAX_INTERFACES) {
            fprintf(stderr, "接口数量超限，跳过剩余接口。\n");
            break;
        }

        InterfaceInfo *iface = &g_interfaces[g_num_interfaces];
        strncpy(iface->ifname, d->name, sizeof(iface->ifname) - 1);
        strncpy(iface->description, d->description ? d->description : "无描述", sizeof(iface->description) - 1);

        iface->handle = pcap_open_live(d->name, BUFSIZ, 1, 1000, errbuf);
        if (!iface->handle) {
            fprintf(stderr, "无法打开设备 %s: %s\n", d->name, errbuf);
            continue;
        }

        iface->stats.last_time = time(NULL);

        pthread_create(&iface->thread, NULL, capture_thread, iface);
        g_num_interfaces++;
    }

    // 每秒打印一次统计
    while (1) {
        display_stats();
        sleep(1);
    }

    for (int i = 0; i < g_num_interfaces; i++) {
        pthread_join(g_interfaces[i].thread, NULL);
    }

    pcap_freealldevs(interfaces);
    return 0;
}

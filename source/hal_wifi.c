#ifdef LINUX_WIFI_DEMO
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <linux/if.h>

#include "tuya_gw_wifi_types.h"

#define RECV_BUF_MAX 512
#define AP_NUM_MAX   32
#define WLAN_DEV     "wlan0"

static wifi_work_mode_t g_wk_mode = WMODE_STATION;

#pragma pack(1)
/* http://www.radiotap.org/ */
typedef struct {
    uint8_t it_version;
    uint8_t it_pad;
    uint16_t it_len;
    uint32_t it_present;
}ieee80211_radiotap_header;
#pragma pack()

static int sniffer_enabled = 0;
static pthread_t sniffer_tid = 0;
static sniffer_callback sniffer_cb = NULL;

static int run_command(const char *cmd, char *data, int len)
{
    FILE *fp;
    int ret;

    fp = popen(cmd, "r");
    if (fp == NULL)
        return -1;

	if (data != NULL) {
        memset(data, 0, len);

        fread(data, len, 1, fp);
        len = strlen(data);
        if (data[len - 1] == '\n')
            data[len - 1] = '\0';
    }

    pclose(fp);

    return 0;
}

static void *sniffer_process(void *arg)
{
    uint8_t rev_buf[RECV_BUF_MAX] = {0};
    int sock = 0;
    int skip_len = 26; /* radiotap 默认长度为26 */
	struct ifreq ifr;
    
    sock = socket(PF_PACKET, SOCK_RAW, htons(0x03));
    if (sock < 0) {
        log_err("create socket failed");
		return NULL;
    }

	memset(&ifr, 0x00, sizeof(ifr));
	strncpy(ifr.ifr_name, WLAN_DEV , strlen(WLAN_DEV) + 1);
	setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, (char *)&ifr, sizeof(ifr));

    while ((sniffer_cb != NULL) && sniffer_enabled) {
        int rev_num = recvfrom(sock, rev_buf, RECV_BUF_MAX, 0, NULL, NULL);
        ieee80211_radiotap_header *header = (ieee80211_radiotap_header *)rev_buf;
        skip_len = header->it_len;

        if (skip_len >= RECV_BUF_MAX)
            continue;

        if (rev_num > skip_len) {
            sniffer_cb(rev_buf + skip_len, rev_num - skip_len, 99);
        }
    }

    sniffer_cb = NULL;

    close(sock);
}

int hal_wifi_scan_all_ap(ap_scan_info_s **aps, uint32_t *num)
{
    static ap_scan_info_s s_aps[AP_NUM_MAX];
    int i = 0, idx = -1;
    FILE *fp = NULL;
    char buf[256] = {0};

    memset(s_aps, 0, sizeof(s_aps));

    *aps = s_aps;

    fp = popen("iwlist "WLAN_DEV" scan", "r");
    if (fp == NULL)
        return -1;

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *bssid = strstr(buf, " - Address: ");
        if (bssid != NULL) {
            idx++;
            if (idx >= AP_NUM_MAX) {
                log_warn("reach max record");
                idx--;
                break;
            }

            int x1,x2,x3,x4,x5,x6;
            uint8_t *tbssid = s_aps[idx].bssid;

            sscanf(bssid + strlen(" - Address: "), "%x:%x:%x:%x:%x:%x", &x1, &x2, &x3, &x4, &x5, &x6);
            tbssid[0] = x1 & 0xFF;
            tbssid[1] = x2 & 0xFF;
            tbssid[2] = x3 & 0xFF;
            tbssid[3] = x4 & 0xFF;
            tbssid[4] = x5 & 0xFF;
            tbssid[5] = x6 & 0xFF;

            goto next;
        } else {
            if (idx < 0)
                goto next;
        }

        {
            char *quality = strstr(buf, "Quality=");
            if (quality != NULL) {
                int x = 0;
                int y = 0;

                sscanf(quality + strlen("Quality=") , "%d/%d ",&x,&y);
                s_aps[idx].rssi = x * 100 / (y + 1);

                goto next;
            }
        }

        {
            char *channel = strstr(buf, "(Channel ");
            if (channel != NULL) {
                int x = 0;

                sscanf(channel + strlen("(Channel "), "%d)", &x);
                s_aps[idx].channel = x;

                goto next;
            }

        }

        {
            char *essid = strstr(buf, "ESSID:\"");
            if (essid != NULL) {
                sscanf(essid + strlen("ESSID:\""), "%s", s_aps[idx].ssid);
                s_aps[idx].s_len = strlen(s_aps[idx].ssid);

                if (s_aps[idx].s_len != 0) {
                    s_aps[idx].ssid[s_aps[idx].s_len - 1] = 0;
                    s_aps[idx].s_len--;
                }

                goto next;
            }
        }

next:
        memset(buf, 0, sizeof(buf));
    }

    pclose(fp);

    *num = idx + 1;

    log_debug("WiFi scan ap start");
    for (i = 0; i < *num; i++) {
        log_debug("index: %d, channel: %d, bssid: %02X-%02X-%02X-%02X-%02X-%02X, RSSI: %d, SSID: %s", \
                   i, s_aps[i].channel, \
                   s_aps[i].bssid[0], s_aps[i].bssid[1], s_aps[i].bssid[2], \
                   s_aps[i].bssid[3], s_aps[i].bssid[4], s_aps[i].bssid[5], \
                   s_aps[i].rssi, s_aps[i].ssid);
    }
    log_debug("WiFi scan ap end");

    return 0;
}

int hal_wifi_scan_assigned_ap(char *ssid, ap_scan_info_s **ap)
{
    int i = 0;
    uint32_t num = 0;
    ap_scan_info_s *aps = NULL;

    *ap = NULL;

    hal_wifi_scan_all_ap(&aps, &num);

    for (i = 0; i < num; i++) {
        if (memcmp(aps[i].ssid, ssid, aps[i].s_len) == 0) {
            *ap = aps + i;
            break;
        }
    }

    return 0;
}

int hal_wifi_release_ap(ap_scan_info_s *ap)
{
    return 0;
}

int hal_wifi_set_cur_channel(uint8_t channel)
{
    char cmd[128] = {0};	

    log_debug("WiFi set channel: %d", channel);

    snprintf(cmd, sizeof(cmd), "iwconfig %s channel %d", WLAN_DEV, channel);
    run_command(cmd, NULL, 0);

    return 0;
}

int hal_wifi_get_cur_channel(uint8_t *channel)
{
    char buf[128] = {0};
    char *chan_str = NULL;
    FILE *fp = NULL;
    
    fp = popen("iwlist "WLAN_DEV" channel", "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        chan_str = strstr(buf, " (Channel ");
        if (channel != NULL)
            break;
    }

    chan_str = strstr(buf, " (Channel ");
    if (chan_str != NULL) {
        int x = 0;

        sscanf(chan_str + strlen(" (Channel "), "%d", &x);
        *channel = x;
    } else {
        pclose(fp);
        return -1;
    }

    pclose(fp);

    log_debug("WiFi get channel: %d", *channel);

    return 0;
}

int hal_wifi_set_work_mode(wifi_work_mode_t mode)
{
	char cmd[256] = {0};

	g_wk_mode = mode;

	log_debug("WiFi set mode: %d", mode);

    switch (mode) {
        case WMODE_LOWPOWER:
            break;
        case WMODE_SNIFFER:
            snprintf(cmd, sizeof(cmd), "iwconfig %s mode Monitor", WLAN_DEV);
            break;
        case WMODE_STATION:
            snprintf(cmd, sizeof(cmd), "iwconfig %s mode Managed", WLAN_DEV);
            break;
        case WMODE_SOFTAP:
            snprintf(cmd, sizeof(cmd), "iwconfig %s mode Master", WLAN_DEV);
            break;
        case WMODE_STATIONAP:
            break;
        default:
            break;
    }

	system(cmd);

    return 0;
}


int hal_wifi_set_sniffer(int enable, sniffer_callback cb)
{
    if (enable) {
        log_debug("enable sniffer");

        hal_wifi_set_work_mode(WMODE_SNIFFER);
        sniffer_cb = cb;
        sniffer_enabled = 1;
        pthread_create(&sniffer_tid, NULL, sniffer_process, NULL);
    } else {
        log_debug("disable sniffer");

        hal_wifi_set_work_mode(WMODE_STATION);
        sniffer_enabled = 0;
        pthread_join(sniffer_tid, NULL);
    }

    return 0;
}

int hal_wifi_get_ip(wifi_type_t type, ip_info_s *ip)
{
    struct ifreq ifr;
    int sock = 0;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        log_err("create socket failed");
        return -1;
    }

    strncpy(ifr.ifr_name, WLAN_DEV, strlen(WLAN_DEV) + 1);

    if (ioctl(sock, SIOCGIFADDR, &ifr) == 0)
        strncpy(ip->ip, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), sizeof(ip->ip));

    if (ioctl(sock, SIOCGIFBRDADDR, &ifr) == 0)
        strncpy(ip->gateway, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_broadaddr)->sin_addr), sizeof(ip->gateway));

    if (ioctl(sock, SIOCGIFNETMASK, &ifr) == 0)
        strncpy(ip->mask, inet_ntoa(((struct sockaddr_in *)&ifr.ifr_addr)->sin_addr), sizeof(ip->mask));

    close(sock);

    log_debug("WiFi got ip->ip: %s", ip->ip);

    return 0;
}

int hal_wifi_get_mac(wifi_type_t type, mac_info_s *mac)
{
    char buf[256] = {0};
    FILE *fp = NULL;
    
    fp = popen("ifconfig "WLAN_DEV, "r");
    if (fp == NULL) {
        return -1;
    }

    while (fgets(buf, sizeof(buf), fp) != NULL) {
        char *ether = strstr(buf, "ether ");
        if (ether != NULL) {
            int x1, x2, x3, x4, x5, x6;

            sscanf(ether + strlen("ether "), "%x:%x:%x:%x:%x:%x", &x1, &x2, &x3, &x4, &x5, &x6);
            mac->mac[0] = x1 & 0xFF;
            mac->mac[1] = x2 & 0xFF;
            mac->mac[2] = x3 & 0xFF;
            mac->mac[3] = x4 & 0xFF;
            mac->mac[4] = x5 & 0xFF;
            mac->mac[5] = x6 & 0xFF;
        }
    }

    pclose(fp);

    log_debug("WiFi got MAC: %02X-%02X-%02X-%02X-%02X-%02X", mac->mac[0], mac->mac[1], mac->mac[2], \
                                                             mac->mac[3],mac->mac[4],mac->mac[5]);

    return 0;
}

int hal_wifi_set_mac(wifi_type_t type, mac_info_s *mac)
{
    return 0;
}

int hal_wifi_get_work_mode(wifi_work_mode_t *mode)
{
	*mode = g_wk_mode;

	log_debug("WiFi get mode: %d", *mode);

    return 0;
}

int hal_wifi_connect_station(char *ssid, char *passwd)
{
    return 0;
}

int hal_wifi_disconnect_station(void)
{
    return 0;
}

int hal_wifi_get_station_rssi(char *rssi)
{
    return 0;
}

int hal_wifi_get_station_mac(mac_info_s *mac)
{
    return 0;
}

int hal_wifi_get_station_conn_stat(station_conn_stat_t *stat)
{
    char cmd[128] = {0};
    char buf[128] = {0};

    snprintf(cmd, sizeof(cmd), "ifconfig %s | grep inet", WLAN_DEV);

    run_command(cmd, buf, sizeof(buf));
    if (strlen(buf) > 0) {
      log_debug("got ip, connect finish");
      *stat = STAT_GOT_IP;
      return 0;
    }

    *stat = STAT_CONN_SUCCESS;

    return 0;
}

int hal_wifi_ap_start(ap_cfg_info_s *cfg)
{
	log_debug("start ap: %s", cfg->ssid);

    return 0;
}

int hal_wifi_ap_stop(void)
{
    return 0;
}

int hal_wifi_set_country_code(int code)
{
    return 0;
}

#else

#include "tuya_gw_wifi_types.h"

int hal_wifi_scan_all_ap(ap_scan_info_s **aps, uint32_t *num)
{
    (void)aps;
    (void)num;
    return 0;
}

int hal_wifi_scan_assigned_ap(char *ssid, ap_scan_info_s **ap)
{
    (void)ssid;
    (void)ap;
    return 0;
}

int hal_wifi_release_ap(ap_scan_info_s *ap)
{
    (void)ap;
    return 0;
}

int hal_wifi_set_cur_channel(uint8_t channel)
{
    (void)channel;
    return 0;
}

int hal_wifi_get_cur_channel(uint8_t *channel)
{
    (void)channel;
    return 0;
}

int hal_wifi_set_sniffer(int enable, sniffer_callback cb)
{
    (void)enable;
    (void)cb;
    return 0;
}

int hal_wifi_get_ip(wifi_type_t type, ip_info_s *ip)
{
    (void)type;
    (void)ip;
    return 0;
}

int hal_wifi_get_mac(wifi_type_t type, mac_info_s *mac)
{
    (void)type;
    (void)mac;
    return 0;
}

int hal_wifi_set_mac(wifi_type_t type, mac_info_s *mac)
{
    (void)mac;
    (void)type;
    return 0;
}

int hal_wifi_set_work_mode(wifi_work_mode_t mode)
{
    (void)mode;
    return 0;
}

int hal_wifi_get_work_mode(wifi_work_mode_t *mode)
{
    (void)mode;
    return 0;
}

int hal_wifi_connect_station(char *ssid, char *passwd)
{
    (void)ssid;
    (void)passwd;
    return 0;
}

int hal_wifi_disconnect_station(void)
{
    return 0;
}

int hal_wifi_get_station_rssi(char *rssi)
{
    (void)rssi;
    return 0;
}

int hal_wifi_get_station_mac(mac_info_s *mac)
{
    (void)mac;
    return 0;
}

int hal_wifi_get_station_conn_stat(station_conn_stat_t *stat)
{
    (void)stat;
    return 0;
}

int hal_wifi_ap_start(ap_cfg_info_s *cfg)
{
    (void)cfg;
    return 0;
}

int hal_wifi_ap_stop(void)
{
    return 0;
}

int hal_wifi_set_country_code(int code)
{
    (void)code;
    return 0;
}
#endif

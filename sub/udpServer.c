#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define DEFAULT_PORT 5000
#define DEFAULT_MCAST "239.255.0.1"

// 打印使用方法
void usage (const char *prog) {
    printf ("Usage: %s [broadcast|multicast] [port] [group_if needed]\n", prog);
    printf ("  broadcast: listen for UDP broadcast\n");
    printf ("  multicast: listen for UDP multicast\n");
    printf ("  port: optional, default %d\n", DEFAULT_PORT);
    printf ("  group_if: optional, interface IP for multicast, default INADDR_ANY\n");
    exit (1);
}

int main (int argc, char *argv[]) {
    if (argc < 2) usage (argv[0]);

    int sockfd;
    struct sockaddr_in local_addr, client_addr;
    socklen_t addrlen = sizeof (client_addr);
    char buf[BUF_SIZE];
    int port = (argc >= 3) ? atoi (argv[2]) : DEFAULT_PORT;
    int is_multicast = (strcmp (argv[1], "multicast") == 0);

    // 创建 UDP socket
    if ((sockfd = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror ("socket failed");
        exit (1);
    }

    // 允许地址复用
    int opt = 1;
    setsockopt (sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (opt));

    // 绑定本地端口
    memset (&local_addr, 0, sizeof (local_addr));
    local_addr.sin_family = AF_INET;
    local_addr.sin_addr.s_addr = htonl (INADDR_ANY);
    local_addr.sin_port = htons (port);

    if (bind (sockfd, (struct sockaddr *) &local_addr, sizeof (local_addr)) < 0) {
        perror ("bind failed");
        close (sockfd);
        exit (1);
    }

    if (is_multicast) {
        const char *mcast_group = DEFAULT_MCAST;
        const char *iface_ip = "0.0.0.0";

        if (argc >= 4) mcast_group = argv[3];// 用户自定义组播地址
        if (argc >= 5) iface_ip = argv[4];   // 自定义网卡IP

        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr (mcast_group);
        mreq.imr_interface.s_addr = inet_addr (iface_ip);

        if (setsockopt (sockfd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof (mreq)) < 0) {
            perror ("IP_ADD_MEMBERSHIP failed");
            close (sockfd);
            exit (1);
        }

        printf ("UDP Multicast Server listening on group=%s, port=%d\n", mcast_group, port);
    } else {
        printf ("UDP Broadcast Server listening on port %d\n", port);
    }

    while (1) {
        int n = recvfrom (sockfd, buf, BUF_SIZE, 0, (struct sockaddr *) &client_addr, &addrlen);
        if (n > 0) {
            buf[n] = '\0';
            printf ("Received from %s:%d : %s\n", inet_ntoa (client_addr.sin_addr), ntohs (client_addr.sin_port), buf);

            // 响应客户端
            char reply[BUF_SIZE];
            snprintf (reply, sizeof (reply), "Hello %s, I am UDP server!", inet_ntoa (client_addr.sin_addr));
            sendto (sockfd, reply, strlen (reply), 0, (struct sockaddr *) &client_addr, addrlen);
        }
    }

    close (sockfd);
    return 0;
}

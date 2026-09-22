#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

int efd;             /* epoll file descriptor */
int sfd;             /* serial port file descriptor */
int port = 1;        /* serial port number */
int baudrate = 9600; /* serial port bandwidth rate */
int databits = 8;    /* serial port databits */
int parity = 'n';    /* serial parity */
int stopbits = 1;    /* serial stopbits */
uint32_t timeout = 100;  /* serial port timeout, ms */

void DumpHex(uint8_t *b, int count) {
    for (int i = 0; i < count; i++) {
        printf("%02x ", b[i]);
    }
    printf("\n");
}

int SerialOpen(int port, int baudrate, int databits, int parity, int stopbits) {
    char serialPort[64] = {0};
    snprintf(serialPort, sizeof(serialPort), "/dev/ttymxc%d", port);

    struct termios opt;
    sfd = open(serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (sfd < 0) {
        printf("open %s failed!\n");
        return -1;
    }

    tcgetattr(sfd, &opt);

    /* stop */
    if (stopbits == 2) {
        opt.c_cflag |= CSTOPB;
    } else {
        opt.c_cflag &= ~CSTOPB;
    }

    /* databit */
    switch (databits) {
        case 5:
            opt.c_cflag |= CS5;
            break;
        case 6:
            opt.c_cflag |= CS6;
            break;
        case 7:
            opt.c_cflag |= CS7;
            break;
        case 8:
            opt.c_cflag |= CS8;
            break;
        default:
            opt.c_cflag |= CS8;
            break;
    }

    /* parity */
    switch (parity) {
        case 'n':
        case 'N': {//无奇偶校验位
            opt.c_cflag &= ~PARENB;
            opt.c_iflag &= ~INPCK;
            break;
        }
        case 'o':
        case 'O': {// 奇校验
            opt.c_cflag |= PARENB;
            opt.c_cflag |= PARODD;
            opt.c_iflag |= INPCK;
            opt.c_iflag &= ~ISTRIP;
            break;
        }
        case 'e':
        case 'E': {// 偶校验
            opt.c_cflag |= PARENB;
            opt.c_cflag &= ~PARODD;
            opt.c_iflag |= INPCK;
            opt.c_iflag &= ~ISTRIP;
            break;
        }
        default:
            opt.c_cflag &= ~PARENB;
            opt.c_iflag &= ~INPCK;
            break;
    }

    switch (baudrate) {
        case 9600: {
            cfsetispeed(&opt, B9600);
            cfsetospeed(&opt, B9600);
            break;
        }
        case 19200: {
            cfsetispeed(&opt, B19200);
            cfsetospeed(&opt, B19200);
            break;
        }
        case 38400: {
            cfsetispeed(&opt, B38400);
            cfsetospeed(&opt, B38400);
            break;
        }
        case 57600: {
            cfsetispeed(&opt, B57600);
            cfsetospeed(&opt, B57600);
            break;
        }
        case 115200: {
            cfsetispeed(&opt, B115200);
            cfsetospeed(&opt, B115200);
            break;
        }
        default: {
            cfsetispeed(&opt, B9600);
            cfsetospeed(&opt, B9600);
            break;
        }
    }

    opt.c_cflag |= (CLOCAL | CREAD);
    opt.c_cflag &= ~CRTSCTS; //无硬件流控制
    opt.c_cflag &= ~CSIZE;
    opt.c_iflag &= ~(IXON | IXOFF | IXANY);
    opt.c_iflag &= ~IGNPAR;
    opt.c_iflag &= ~(ICRNL | IGNCR);
    opt.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    opt.c_oflag &= ~(ONLCR | OCRNL);
    opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    opt.c_oflag &= ~OPOST;
    opt.c_cc[VMIN] = 0;
    opt.c_cc[VTIME] = 0;

    tcdrain(sfd);
    tcflush(sfd, TCIOFLUSH);
    if (tcsetattr(sfd, TCSANOW, &opt) < 0) {
        close(sfd);
        return -1;
    }

    struct epoll_event event;
    event.events = EPOLLET | EPOLLIN;
    event.data.fd = sfd;

    efd = epoll_create(6);
    if (efd < 0) {
        printf("epoll_create failed!\n");
        return -1;
    }

    if (epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event) < 0) {
        printf("epoll_add failed(epoll_ctl)[efd:%d,fd:%d][%s]", efd, sfd, strerror(errno));
        return -1;
    }

    return 0;
}

void SerialClose(void) {
    tcflush(sfd, TCOFLUSH);
    tcflush(sfd, TCIFLUSH);
    close(sfd);
}

int SerialWrite(const uint8_t *pBuf, int len) {
    int ret = write(sfd, pBuf, len);
    if (ret < 0) {
        printf("write 'ttymxc%d' fail!", port);
        return -1;
    }

    return 0;
}

int main(int argc, char *argv[]) {
    struct epoll_event events[6];
    struct epoll_event evn;
    bool bStopped = false;
    struct timeval newtime, oldtime;
    gettimeofday(&oldtime, NULL);
    uint32_t timediff = 0;  // ms
    int length = 0;
    uint8_t buf[1024] = {0};

    if (argc < 4) {
        printf("usage: ./serial <port> <baudrate> <parity('n')> <Timeout(ms)> <len> <data1> [data2] ...\n");
        return -1;
    }
    if (length > 1024) {
        printf("data length is too long!\n");
        return -1;
    }
    port = atoi(argv[1]);
    baudrate = atoi(argv[2]);
    if ((*argv[3] == 'o') || (*argv[3] == 'O') || (*argv[3] == 'e') || (*argv[3] == 'E')) {
        parity = *argv[3];
    } else {
        parity = 'n';
    }

    timeout += atoi(argv[4]);
    length = atoi(argv[5]);

//    printf("port:%d, baudrate:%d, parity:%c, timeout:%d, length:%d, ", port, baudrate, parity, timeout, length);

    for (int i = 0; i < length; i++) {
        buf[i] = strtol(argv[6 + i], NULL, 16);
    }

    SerialOpen(port, baudrate, databits, parity, stopbits);
//    DumpHex(buf, length);
    SerialWrite(buf, length);

    while (!bStopped && timediff < timeout) {
        gettimeofday(&newtime, NULL);
        timediff = (newtime.tv_sec - oldtime.tv_sec) * 1000 + (newtime.tv_usec - oldtime.tv_usec) / 1000;

        int size = epoll_wait(efd, events, 1, 50);
        if (size <= 0) continue;
        for (int i = 0; i < size; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN))) {
                bStopped = true;
                break;
            } else if (events[i].events & EPOLLIN) {
                int len = 0;
                ioctl(events[i].data.fd, FIONREAD, &len);
                uint8_t *pBuf = (uint8_t *) calloc(len, sizeof(uint8_t));
                if (!pBuf) {
                    bStopped = true;
                    break;
                }
                len = read(events[i].data.fd, pBuf, len);
                bStopped = true;
                DumpHex(pBuf, len);
                free(pBuf);
                break;
            }
        }
    }

    epoll_ctl(efd, EPOLL_CTL_DEL, sfd, &evn);
    SerialClose();

    if (timediff >= timeout) {
        printf("recv time out!\n");
    }

    return 0;
}
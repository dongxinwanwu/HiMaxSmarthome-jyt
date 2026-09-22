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

int sfd; /* serial port file descriptor */
int SerialOpen (int port, int baudrate, int databits, int parity, int stopbits) {
    char serialPort[64] = {0};
    sprintf (serialPort, "/dev/ttymxc%d", port);

    struct termios opt;
    sfd = open (serialPort, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (sfd < 0) {
        printf ("open %s failed!\n", serialPort);
        return -1;
    }

    tcgetattr (sfd, &opt);

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
            cfsetispeed (&opt, B9600);
            cfsetospeed (&opt, B9600);
            break;
        }
        case 19200: {
            cfsetispeed (&opt, B19200);
            cfsetospeed (&opt, B19200);
            break;
        }
        case 38400: {
            cfsetispeed (&opt, B38400);
            cfsetospeed (&opt, B38400);
            break;
        }
        case 57600: {
            cfsetispeed (&opt, B57600);
            cfsetospeed (&opt, B57600);
            break;
        }
        case 115200: {
            cfsetispeed (&opt, B115200);
            cfsetospeed (&opt, B115200);
            break;
        }
        default: {
            cfsetispeed (&opt, B9600);
            cfsetospeed (&opt, B9600);
            break;
        }
    }

    opt.c_cflag |= (CLOCAL | CREAD);
    opt.c_cflag &= ~CRTSCTS;//无硬件流控制
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

    tcdrain (sfd);
    tcflush (sfd, TCIOFLUSH);
    if (tcsetattr (sfd, TCSANOW, &opt) < 0) {
        close (sfd);
        return -1;
    }

    return 0;
}

void SerialClose (void) {
    tcflush (sfd, TCOFLUSH);
    tcflush (sfd, TCIFLUSH);
    close (sfd);
}

int SerialWrite (const uint8_t *pBuf, int len) {
    int ret = write (sfd, pBuf, len);
    if (ret < 0) {
        printf ("write 'ttymxc5' fail!");
        return -1;
    }

    return 0;
}

int main (void) {
    uint8_t stopAck[1] = {0x0E};
    uint8_t autoAck[4] = {0xF1, 0xFF, 0xCC, 0x00};
    uint8_t startAck[1] = {0x0F};
    system ("echo 0 > /sys/class/reset/knx-rst/reset");
    system ("echo 1 > /sys/class/leds/knx-heartbeat/brightness");
    SerialOpen (5, 19200, 8, 'e', 1);
    usleep (1000 * 500);

    SerialWrite (stopAck, 1);
    usleep (1000 * 200);

    SerialWrite (autoAck, 4);
    usleep (1000 * 200);
    SerialWrite (autoAck, 4);
    usleep (1000 * 200);
    SerialWrite (autoAck, 4);
    usleep (1000 * 200);

    SerialWrite (startAck, 1);
    usleep (1000 * 200);

    SerialClose ();

    return 0;
}

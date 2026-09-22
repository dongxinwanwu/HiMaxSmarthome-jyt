#include "serialHandle.h"
#include "common.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "pthread.h"
#include "thpool.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <zlog.h>

typedef struct SerialParam {
    int efd;      /* epoll file descriptor */
    int sfd;      /* serial port file descriptor */
    int port;     /* serial port number */
    int baudrate; /* serial port bandwidth rate */
    int databits; /* serial port databits */
    int parity;   /* serial parity */
    int stopbits; /* serial stopbits */
    char devDesc[32];
    bool bBusy;
    bool bFullDuplex;

    uint16_t key;
    uint16_t rcvLen;
    bool bRcvRun;
    pthread_mutex_t *pMutex;
    threadpool hThreadPool;
    struct timespec tp;

    void (*threadEntry) (void *arg);
} SerialParam_t;

static threadpool hSerialThpool = NULL;
static pthread_mutex_t Serial1Mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t Serial2Mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t Serial3Mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t Serial4Mutex = PTHREAD_MUTEX_INITIALIZER;

// void DumpHex (const char *szTag, uint8_t *b, int count) {
//     printf ("%s: ", szTag);
//     for (int i = 0; i < count; i++) {
//         printf ("%02x ", b[i]);
//     }
//     printf ("\n");
// }

void SerialRcvEntry (void *arg) {
    if (!arg) {
        logErr ( COLOR_RED "serial recv thread parameter invalid" COLOR_CLEAR);
        return;
    }
    struct timespec tp = GetClock ();

    MbusBase_t *pBase = (MbusBase_t *) arg;
    struct epoll_event events[6];
    struct epoll_event evn;
    struct timeval newtime, oldtime;
    int timeout = 10; /* 10ms */
    if (pBase->bFullDuplex) {
        timeout = -1; /* wait forever */
    }
    gettimeofday (&oldtime, NULL);
    pBase->pSerial->pParam->bRcvRun = true;

    while (pBase->pSerial->pParam->bRcvRun) {
        if (!pBase->bFullDuplex) {
            gettimeofday (&newtime, NULL);
            uint32_t timediff = (newtime.tv_sec - oldtime.tv_sec) * 1000 * 1000 + newtime.tv_usec - oldtime.tv_usec;
            if (timediff >= 100000u + pBase->delay * 1000) { /* wait for 100ms + delay timeout */
                logDbg ( "ttymxc%d recv timeout=%.03fms!", pBase->pSerial->pParam->port, timediff / 1000000.0);
                break;
            }
        }
        int size = epoll_wait (pBase->pSerial->pParam->efd, events, 1, timeout);
        if (size <= 0) continue;
        if ((events[0].events & EPOLLERR) || (events[0].events & EPOLLHUP) || (!(events[0].events & EPOLLIN))) {
            logErr ( "ttymxc%d recv error!", pBase->pSerial->pParam->port);
            if (!pBase->bFullDuplex) break;
        } else if (events[0].events & EPOLLIN) {
            int len = 0;
            ioctl (events[0].data.fd, FIONREAD, &len);
            uint8_t *pBuf = (uint8_t *) calloc (len, sizeof (uint8_t));
            if (!pBuf) {
                logErr ( "ttymxc%d calloc failed!", pBase->pSerial->pParam->port);
                break;
            }
            len = read (events[0].data.fd, pBuf, len);

            if (!pBase->pIf->parse && !pBase->pIf->parseExt) {
                free (pBuf);
                logErr ( "ttymxc%d report() is null", pBase->pSerial->pParam->port);
                break;
            }
            char buf[64];
            snprintf (buf, sizeof (buf), "[ttymxc%d rcv]", pBase->pSerial->pParam->port);
            logHex (buf, pBuf, len);

            MbusObj_t **ppObj = NULL;
#if 0
            if (pBase->element.primaryType == MODBUS_DEVICE_VENTILATION) {
                //                MbusVent_t *pVent = container_of (pBase, MbusVent_t, MbusBase_t);
                MbusVent_t *pVent1 = (MbusVent_t *) pBase;
                logDbg ( "switch=%d, speed=%d", pVent1->pSwitch->sndValue, pVent1->pSpeed->sndValue);

                MbusVent_t **ppVent2 = (MbusVent_t **) pBase;
                logDbg ( "switch=%d, speed=%d", (*ppVent2)->pSwitch->sndValue, (*ppVent2)->pSpeed->sndValue);
            }
#endif
            if (pBase->pIf->parse) {
                ppObj = pBase->pIf->parse (pBase->devAddr, pBase->regAddr, (const void *) pBase, pBuf, (uint16_t) len);
            } else if (pBase->pIf->parseExt) {
                ppObj = pBase->pIf->parseExt (pBase->devAddr, pBase->regAddr, pBuf, (uint16_t) len);
            }
            pBase->lostCnt = 0; /*  */
            MbusObj_t **ppObjTmp = ppObj;
            free (pBuf);
            if (!ppObj) {
                logErr ( "ttymxc%d report() parse fail!", pBase->pSerial->pParam->port);
                if (pBase->bFullDuplex) continue;
                break;
            }
            for (; *ppObj; ppObj++) {
                (*ppObj)->pPrimaryKey = pBase->element.pPrimaryKey;
                (*ppObj)->ePrimaryType = pBase->element.primaryType;
                logTrc ( "ttymxc%d recv thread queue_put devID=%s, devType=%d, localPoint=%d, value=%d",
                            pBase->pSerial->pParam->port, (*ppObj)->pPrimaryKey, (*ppObj)->ePrimaryType, (*ppObj)->localPoint, (*ppObj)->value);
                int8_t ret = queue_put (pBase->pMbusDataArrived, (void *) *ppObj);
                if (ret != 0) {
                    logErr ( COLOR_PURPLE "ttymxc%d recv thread queue_put fail, ret=%d" COLOR_CLEAR, pBase->pSerial->pParam->port, ret);
                }
            }
            free (ppObjTmp);
            ppObjTmp = NULL;
            ppObj = NULL;
            if (!pBase->bFullDuplex) break;
        }
    }
    if (epoll_ctl (pBase->pSerial->pParam->efd, EPOLL_CTL_DEL, pBase->pSerial->pParam->sfd, &evn) < 0) {
        logErr ( "epoll_del failed(epoll_ctl)[efd:%d,fd:%d][%s]", pBase->pSerial->pParam->efd, pBase->pSerial->pParam->sfd, strerror (errno));
    }
    pBase->pSerial->close (pBase->pSerial);
    uint64_t lostTime = GetElapsed (&tp);
    logTrc ( "ttymxc%d recv lost time(%.03f)ms.", pBase->pSerial->pParam->port, lostTime / 1000000.0);
}

int SerialOpen (Serial_t *this) {
    if (!this || !this->pParam) {
        logErr ( COLOR_RED "serial open parameter invalid" COLOR_CLEAR);
        return -1;
    }

    if (this->pParam->devDesc[0] == '\0') {
        snprintf (this->pParam->devDesc, sizeof (this->pParam->devDesc), "/dev/ttymxc%d", this->pParam->port);
    }

    if (this->pParam->bFullDuplex) {
        int ret = pthread_mutex_trylock (this->pParam->pMutex);
        if (ret != 0) {
            logTrc ( "ttymxc%d not repeat opening!", this->pParam->port);
            return 0;
        }
    } else {
        if (this->pParam->pMutex) {
            pthread_mutex_lock (this->pParam->pMutex);
        }
    }

    this->pParam->tp = GetClock ();

    struct termios opt;
    this->pParam->sfd = open (this->pParam->devDesc, O_RDWR | O_NOCTTY | O_NONBLOCK);
    if (this->pParam->sfd < 0) {
        if (this->pParam->pMutex) {
            pthread_mutex_unlock (this->pParam->pMutex);
        }
        logErr ( COLOR_RED "open %s failed!" COLOR_CLEAR, this->pParam->devDesc);
        return -1;
    }

    tcgetattr (this->pParam->sfd, &opt);

    /* stop */
    if (this->pParam->stopbits == 2) {
        opt.c_cflag |= CSTOPB;
    } else {
        opt.c_cflag &= ~CSTOPB;
    }

    /* databit */
    opt.c_cflag &= ~CSIZE;
    switch (this->pParam->databits) {
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
        default:
            opt.c_cflag |= CS8;
            break;
    }

    /* parity */
    switch (this->pParam->parity) {
        case 'o':
        case 'O': // 奇校验
            opt.c_cflag |= PARENB;
            opt.c_cflag |= PARODD;
            opt.c_iflag |= INPCK;
            opt.c_iflag &= ~ISTRIP;
            break;
        case 'e':
        case 'E': // 偶校验
            opt.c_cflag |= PARENB;
            opt.c_cflag &= ~PARODD;
            opt.c_iflag |= INPCK;
            opt.c_iflag &= ~ISTRIP;
            break;
        case 'n':
        case 'N': // 无奇偶校验位
        default:
            opt.c_cflag &= ~PARENB;
            opt.c_iflag &= ~INPCK;
            break;
    }

    switch (this->pParam->baudrate) {
        case 1200: {
            cfsetispeed (&opt, B1200);
            cfsetospeed (&opt, B1200);
            break;
        }
        case 2400: {
            cfsetispeed (&opt, B2400);
            cfsetospeed (&opt, B2400);
            break;
        }
        case 4800: {
            cfsetispeed (&opt, B4800);
            cfsetospeed (&opt, B4800);
            break;
        }
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
    opt.c_iflag &= ~(IXON | IXOFF | IXANY);
    opt.c_iflag &= ~IGNPAR;
    opt.c_iflag &= ~(ICRNL | IGNCR);
    opt.c_iflag &= ~(INLCR | ICRNL | IGNCR);
    opt.c_oflag &= ~(ONLCR | OCRNL);
    opt.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
    opt.c_oflag &= ~OPOST;

    opt.c_cc[VMIN] = 0;
    opt.c_cc[VTIME] = 0;

    tcdrain (this->pParam->sfd);
    tcflush (this->pParam->sfd, TCIOFLUSH);
    if (tcsetattr (this->pParam->sfd, TCSANOW, &opt) < 0) {
        logErr ( "serial tcsetattr failed: %s", strerror (errno));
        close (this->pParam->sfd);
        this->pParam->sfd = -1;
        if (this->pParam->pMutex) {
            pthread_mutex_unlock (this->pParam->pMutex);
        }

        return -1;
    }

    struct epoll_event event;
    event.events = EPOLLET | EPOLLIN;
    event.data.fd = this->pParam->sfd;

    if (epoll_ctl (this->pParam->efd, EPOLL_CTL_ADD, this->pParam->sfd, &event) < 0) {
        logErr ( "epoll_add failed(epoll_ctl)[efd:%d,fd:%d][%s]", this->pParam->efd, this->pParam->sfd, strerror (errno));
        close (this->pParam->sfd);
        this->pParam->sfd = -1;
        if (this->pParam->pMutex) {
            pthread_mutex_unlock (this->pParam->pMutex);
        }

        return -1;
    }

    logTrc ( "ttymxc%d/%d/%d/%c/%d opened!", this->pParam->port, this->pParam->baudrate, this->pParam->databits, this->pParam->parity, this->pParam->stopbits);

    return 0;
}

int SerialOpenExt (struct Serial *this, const char *pDevDesc, int baudrate, int databits, int parity, int stopbits) {
    if (!this || !pDevDesc) {
        logErr ( "SerialOpenExt failed");
        return -1;
    }

    this->pParam->baudrate = baudrate;
    this->pParam->databits = databits;
    this->pParam->parity = parity;
    this->pParam->stopbits = stopbits;
    strcpy (this->pParam->devDesc, pDevDesc);
    this->open (this);

    return 0;
}

void SerialClose (Serial_t *this) {
    if (!this || (this->pParam->sfd == -1)) {
        logErr ( "SerialClose failed");
        return;
    }
    tcflush (this->pParam->sfd, TCOFLUSH);
    tcflush (this->pParam->sfd, TCIFLUSH);
    close (this->pParam->sfd);
    this->pParam->sfd = -1;
    this->pParam->bRcvRun = false;
    uint64_t elapsed = GetElapsed (&this->pParam->tp);
    logTrc ( "ttymxc%d closed! occupied time: %.03fms", this->pParam->port, elapsed / 1000000.0);
    if (this->pParam->pMutex) {
        pthread_mutex_unlock (this->pParam->pMutex);
    }
}

int SerialWrite (Serial_t *this, const uint8_t *pBuf, uint32_t len) {
    if (!this || !pBuf || (len == 0)) {
        logErr ( COLOR_RED "write ttymxc%d fail, invalid input param! pBuf=%p, len=%d" COLOR_CLEAR, this->pParam->port, pBuf, len);
        return -1;
    }

    int ret = write (this->pParam->sfd, pBuf, len);
    if (ret < 0) {
        logErr ( COLOR_RED "write ttymxc%d fail!" COLOR_CLEAR, this->pParam->port);
        return -1;
    }

    char buf[64];
    snprintf (buf, sizeof (buf), "[ttymxc%d snd]", this->pParam->port);
    logHex (buf, pBuf, len);

    return 0;
}

int SerialRead (Serial_t **pThis) {
    if (!pThis || !*pThis) {
        return -1;
    }

    MbusBase_t *pModbus = container_of (pThis, MbusBase_t, pSerial);
    if (pModbus->pSerial->pParam->bFullDuplex && pModbus->pSerial->pParam->bRcvRun) {
        logTrc ( "ttymxc%d already opened!", pModbus->pSerial->pParam->port);
        return 0;
    }

    int ret = thpool_add_work (pModbus->pSerial->pParam->hThreadPool, pModbus->pSerial->pParam->threadEntry, pModbus);
    if (ret != 0) {
        logErr ( COLOR_RED "thpool_add_work fail!" COLOR_CLEAR);
    }

    return 0;
}

int SetPort (struct Serial *this, int port) {
    if (!this || (port < 1) || (port > 4)) {
        logErr ( COLOR_RED "[SetPort] port=%d not permission!" COLOR_CLEAR, port);
        return -1;
    }

    if (port == 1) {
        this->pParam->pMutex = &Serial1Mutex;
    } else if (port == 2) {
        this->pParam->pMutex = &Serial2Mutex;
    } else if (port == 3) {
        this->pParam->pMutex = &Serial3Mutex;
    } else if (port == 4) {
        this->pParam->pMutex = &Serial4Mutex;
    }

    this->pParam->port = port;
    return 0;
}

int SetBaudrate (struct Serial *this, int baudrate) {
    if (!this || (baudrate < 2400) || (baudrate > 115200)) {
        logErr ( COLOR_RED "[SetBaudrate] baudrate=%d not permission!" COLOR_CLEAR, baudrate);
        return -1;
    }

    this->pParam->baudrate = baudrate;

    return 0;
}

int SetDatabits (struct Serial *this, int databits) {
    if (!this || (databits < 5) || (databits > 8)) {
        return -1;
    }

    this->pParam->databits = databits;
    return 0;
}

int SetParity (struct Serial *this, int parity) {
    if (!this || ((parity != 'n') && (parity != 'N') && (parity != 'o') && (parity != 'O') && (parity != 'e') && (parity != 'E'))) {
        return -1;
    }

    this->pParam->parity = parity;

    return 0;
}

int SetStopbits (struct Serial *this, int stopbits) {
    if (!this || (stopbits < 1) || (stopbits > 2)) {
        return -1;
    }

    this->pParam->stopbits = stopbits;

    return 0;
}

int SetLock (struct Serial *this, pthread_mutex_t *pMutex) {
    if (!this || !pMutex) {
        return -1;
    }

    this->pParam->pMutex = pMutex;

    return 0;
}

int SetFullDuplex (struct Serial *this, bool bFullDuplex) {
    if (!this) {
        return -1;
    }

    this->pParam->bFullDuplex = bFullDuplex;

    return 0;
}

int GetPort (struct Serial *this) {

    return this->pParam->port;
}

int GetBaudrate (struct Serial *this) {
    return this->pParam->baudrate;
}

int GetDatabits (struct Serial *this) {
    return this->pParam->databits;
}

int GetParity (struct Serial *this) {
    return this->pParam->parity;
}

int GetStopbits (struct Serial *this) {
    return this->pParam->stopbits;
}

int SetKey (struct Serial *this, uint16_t key) {
    if (!this) {
        return -1;
    }

    this->pParam->key = key;

    return 0;
}

int SetRcvLen (struct Serial *this, uint16_t len) {
    if (!this) {
        return -1;
    }

    this->pParam->rcvLen = len;

    return 0;
}

Serial_t *SerialInit (void) {
    Serial_t *pSerial = (Serial_t *) calloc (1, sizeof (Serial_t));
    if (!pSerial) {
        return NULL;
    }
    pSerial->pParam = (SerialParam_t *) calloc (1, sizeof (SerialParam_t));
    if (!pSerial->pParam) {
        free (pSerial);
        return NULL;
    }

    if (!hSerialThpool) {
        hSerialThpool = thpool_init (30);
        if (!hSerialThpool) {
            free (pSerial->pParam);
            free (pSerial);

            return NULL;
        }
    }

    pSerial->pParam->efd = epoll_create (6);
    if (pSerial->pParam->efd < 0) {
        logErr ( "seiral epoll_create() fail!");
        free (pSerial->pParam);
        free (pSerial);
        // Note: thpool is not destroyed, as it is a global static resource.
        return NULL;
    }

    pSerial->pParam->hThreadPool = hSerialThpool;
    pSerial->pParam->threadEntry = SerialRcvEntry;
    pSerial->pParam->sfd = -1;
    pSerial->pParam->bBusy = false;
    pSerial->pParam->bFullDuplex = false;
    pSerial->pParam->bRcvRun = false;
    memset (pSerial->pParam->devDesc, 0, sizeof (pSerial->pParam->devDesc));

    pSerial->open = SerialOpen;
    pSerial->openExt = SerialOpenExt;
    pSerial->close = SerialClose;
    pSerial->send = SerialWrite;
    pSerial->recv = SerialRead;
    pSerial->setKey = SetKey;
    pSerial->setRcvLen = SetRcvLen;
    pSerial->setPort = SetPort;
    pSerial->setBaudrate = SetBaudrate;
    pSerial->setDatabits = SetDatabits;
    pSerial->setParity = SetParity;
    pSerial->setStopbits = SetStopbits;
    pSerial->setLock = SetLock;
    pSerial->setFullDuplex = SetFullDuplex;
    pSerial->getPort = GetPort;
    pSerial->getBaudrate = GetBaudrate;
    pSerial->getDatabits = GetDatabits;
    pSerial->getParity = GetParity;
    pSerial->getStopbits = GetStopbits;

    return pSerial;
}

void SerialDestroy (Serial_t *pSerial) {
    if (pSerial) {
        if (pSerial->pParam) {
            if (pSerial->pParam->sfd != -1) {
                close (pSerial->pParam->sfd);
                pSerial->pParam->sfd = -1;
            }
            if (pSerial->pParam->efd != -1) {
                close (pSerial->pParam->efd);
                pSerial->pParam->efd = -1;
            }
            free (pSerial->pParam);
            pSerial->pParam = NULL;
        }
        free (pSerial);
    }
}

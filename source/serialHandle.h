#ifndef __SERIAL_HANDLE_H__
#define __SERIAL_HANDLE_H__

#include "common.h"
#include <stdint.h>
#include "pthread.h"
#include <stdbool.h>

typedef struct SerialParam *pSerialParam_t;


typedef struct Serial {
    pSerialParam_t pParam;

    int (*open)(struct Serial *this);
    int (*openExt)(struct Serial *this, const char *pPath, int baudrate, int databits, int parity, int stopbits);
    void (*close)(struct Serial *this);
    int (*send)(struct Serial *this, const uint8_t *pBuf, uint32_t len);
    int (*recv)(struct Serial **pThis);
    int (*setKey)(struct Serial *this, uint16_t key);
    int (*setRcvLen)(struct Serial *this, uint16_t len);

    int (*setPort)(struct Serial *this, int port);
    int (*setBaudrate)(struct Serial *this, int baudrate);
    int (*setDatabits)(struct Serial *this, int databits);
    int (*setParity)(struct Serial *this, int parity);
    int (*setStopbits)(struct Serial *this, int stopbits);
    int (*setLock)(struct Serial *this, pthread_mutex_t *pMutex);
    int (*setFullDuplex)(struct Serial *this, bool bFullDuplex);

    int (*getPort)(struct Serial *this);
    int (*getBaudrate)(struct Serial *this);
    int (*getDatabits)(struct Serial *this);
    int (*getParity)(struct Serial *this);
    int (*getStopbits)(struct Serial *this);
} Serial_t;


Serial_t *SerialInit(void);
void SerialDestroy(Serial_t *pSerial);
void SerialCopy(const Serial_t *pSrc, Serial_t *pDst);
int ModbusDevIdMatch(void *pNode, void *pDevId);

#endif /** __SERIAL_HANDLE_H__ */

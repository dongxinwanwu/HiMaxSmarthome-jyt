#ifndef _CRC16_H__
#define _CRC16_H__

#include <stdint.h>
#include <stdio.h>
#include "common.h"

#ifdef __cplusplus
extern "C"
{
#endif

UNUSED uint16_t crc16(const uint8_t *pBuf, int len);
UNUSED uint8_t cs(uint8_t *pData, uint8_t size);
UNUSED uint8_t BCC8(uint8_t* data, uint8_t size);

#ifdef __cplusplus
}
#endif

#endif

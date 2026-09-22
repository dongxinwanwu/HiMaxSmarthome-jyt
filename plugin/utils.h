#ifndef __UTILS_H__
#define __UTILS_H__

#include "misc_dev_plugins.h"

MbusObj_t **NewMbusObjs(int size);
uint8_t *memmatch(uint8_t *pSrc, uint16_t dst, int len);

#endif
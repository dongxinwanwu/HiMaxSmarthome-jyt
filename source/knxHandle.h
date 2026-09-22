#ifndef __KNX_KNOB_H__
#define __KNX_KNOB_H__

#include "common.h"
#include "thpool.h"
#include "tuya_cloud_com_defs.h"
#include <stdint.h>

int KnxInit(threadpool hKnxThpool);
void KnxDestroy(void);
int KnxGroupWrite(const char *pDevId, const TY_OBJ_DP_S *pObj);
int isKnxGroupAddr(const char *pBuf);
__attribute__((unused)) int isKnxHwAddr(const char *pBuf);
int KnxGroupRead(const char *pGroup);
int Knx_RequestHB(const char *pDevId);
void Knx_SyncDevStatus(threadpool hKnxThpool);
int Knx_Write1Byte (char *pGroupAddr, uint8_t value);
int Knx_SmartPanelModeReport (void);
int Knx_ComparePhy(const char *pPhyA, const char *pPhyB);
uint16_t KnxHwAddr (const char *pBuf);
void KNX_FakePacket(char* pGrp, char* pValue);

#endif

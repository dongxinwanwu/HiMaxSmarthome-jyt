#ifndef __MODBUS_HANDLE_H__
#define __MODBUS_HANDLE_H__

#include "common.h"
#include "list.h"
#include "thpool.h"
#include "tuya_cloud_com_defs.h"
#include <stdint.h>

int ModbusDevWrite (const char *pDevId, const TY_OBJ_DP_S *pObj);
int ModbusInit (threadpool hModbusThpool, list_t *pLst);
void Mbus_RequestHB (threadpool hModbusThpool, const char *pDevId);
int ModbusDevIdMatch (void *pNode, void *pDevId);
//void Mbus_SyncAllDevStatus(void *);
void Mbus_ForceSyncDevStatus (threadpool hMbusThpool);
void Mbus_SyncDevStatus (threadpool hModbusThpool);

#endif

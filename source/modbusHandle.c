#include "modbusHandle.h"
#include "knxHandle.h"
#include "list.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "queue.h"
#include "serialHandle.h"
#include "tuya_gw_dp_api.h"
#include "tuya_gw_misc_api.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define MBUS_FORCE_SYNC_NUM 3

static queue_t *pMonitorQueue;
extern bool g_bOnline;
extern bool g_bActive;

void ModbusDestroy (void);

int ModbusDevIdMatch (void *pNode, void *pDevId) {
    if (!pNode || !pDevId) return 0;

    MbusBase_t **ppBase = (MbusBase_t **) pNode;
    if ((*ppBase)->element.devId == *(uint32_t *)pDevId) {
        return 1;
    }

    return 0;
}

int ModbusDevWrite (const char *pDevId, const TY_OBJ_DP_S *pObj) {
    if (!pDevId || !pObj) {
        logErr ( "ModbusDevWrite param invalid");
        return -1;
    }

    char *endptr;
    errno = 0;
    long val = strtol ((char *) pDevId + 12, &endptr, 10);
    if (errno != 0 || endptr == (char *) pDevId + 12) {
        logErr ("Invalid device ID format");
        return -1;
    }
    uint32_t devId = (uint32_t) val;

    listSetMatchMethod (pMbusDevList, ModbusDevIdMatch);
    listNode *pNode = listSearchKey (pMbusDevList, &devId);
    if (!pNode) {
        logErr ( COLOR_RED "mbus device %s not found in the local list!" COLOR_CLEAR, pDevId);
        return -1;
    }

    MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
    if (!(*ppBase)->pIf || (!(*ppBase)->pIf->build && !(*ppBase)->pIf->buildExt)) {
        logErr ( COLOR_RED "(*ppBase)->pIf=null" COLOR_CLEAR);
        return -1;
    }

    if (!(*ppBase)->pSerial) {
        logErr ( COLOR_RED "mbus device write pVent->pBase->pSerial=null." COLOR_CLEAR);
        return -1;
    }
    int ret = (*ppBase)->pSerial->open ((*ppBase)->pSerial);
    if (ret == 0) {
        Msg_t *pMsg = NULL;
        if ((*ppBase)->pIf->build) {
            pMsg = (*ppBase)->pIf->build ((*ppBase)->devAddr, (*ppBase)->regAddr, (*ppBase)->regAddr, (const void *) pNode->value, (ty_obj_dp_s *) pObj);
        } else if ((*ppBase)->pIf->buildExt) {
            pMsg = (*ppBase)->pIf->buildExt ((*ppBase)->devAddr, (*ppBase)->regAddr, (*ppBase)->regAddr, (ty_obj_dp_s *) pObj);
        }
        if (!pMsg) {
            logErr ( COLOR_RED "mbus device build msg fail! devId=%d, regAddr=%d, groupAddr=%d" COLOR_CLEAR, (*ppBase)->devAddr, (*ppBase)->regAddr, (*ppBase)->regAddr);
            (*ppBase)->pSerial->close ((*ppBase)->pSerial);
            return -1;
        }
        if (!pMsg->pBuf || (pMsg->len == 0)) {
            if (pMsg->pBuf) {
                free (pMsg->pBuf);
                pMsg->pBuf = NULL;
            }
            free (pMsg);
            pMsg = NULL;
            logErr ( COLOR_RED "mbus device build msg invalid!" COLOR_CLEAR);
            (*ppBase)->pSerial->close ((*ppBase)->pSerial);
            return -1;
        }

        (*ppBase)->pSerial->send ((*ppBase)->pSerial, pMsg->pBuf, pMsg->len);
        (*ppBase)->lostCnt++;
        if ((*ppBase)->lostCnt < 3) {
            (*ppBase)->lostCnt++;
        } else {
            (*ppBase)->lostCnt = 0;
            /* todo: 通讯失联 */
        }

        if ((*ppBase)->bFeedback) {

            (*ppBase)->pSerial->recv (&(*ppBase)->pSerial);
        } else if (!(*ppBase)->bFullDuplex) {
            struct timespec tim, tim2;
            tim.tv_sec = 0;
            tim.tv_nsec = (1000 * 1000 * (10 + (*ppBase)->delay));
            nanosleep (&tim, &tim2);
            (*ppBase)->pSerial->close ((*ppBase)->pSerial);
        }
        free (pMsg->pBuf);
        pMsg->pBuf = NULL;
        free (pMsg);
        pMsg = NULL;
    } else {
        logErr ( COLOR_RED "mbus device write serial open fail." COLOR_CLEAR);
    }

    if (!(*ppBase)->bFeedback) {
        if ((*ppBase)->element.bMaster && (*ppBase)->element.pvForeignKey) {
            KnxVent_t *pKnxVent = (KnxVent_t *) (*ppBase)->element.pvForeignKey;
            KnxGroupWrite (pKnxVent->base.element.pPrimaryKey, pObj);
        }

        if (g_bActive) {
            ret = tuya_user_iot_report_obj_dp ((*ppBase)->element.pPrimaryKey, pObj, 1);
            if (ret != OPRT_OK) {
                logErr ( "Report mbus dev state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                            (*ppBase)->element.pPrimaryKey, pObj->dpid, (int) pObj->type, pObj->value.dp_value);
            } else {
                logDbg ( "Report mbus dev state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                            (*ppBase)->element.pPrimaryKey, pObj->dpid, (int) pObj->type, pObj->value.dp_value);
            }

            ret = tuya_user_iot_misc_dev_hb_fresh ((*ppBase)->element.pPrimaryKey);
            if (ret != 0) {
                logErr ( "tuya_user_iot_misc_dev_hb_fresh err: %d", ret);
            }
        }
    }

    MbusBase_t *pBase = (*ppBase)->pNext;
    while (pBase) {
        if (!(*ppBase)->pSerial) {
            logErr ( COLOR_RED "mbus device write pVent->pBase->pSerial=null." COLOR_CLEAR);
            return -1;
        }

        int ret1 = pBase->pSerial->open (pBase->pSerial);
        if (ret1 == 0) {
            Msg_t *pMsg = NULL;
            if (pBase->pIf->build) {
                pMsg = pBase->pIf->build (pBase->devAddr, pBase->regAddr, (*ppBase)->regAddr, (const void *) pNode->value, (ty_obj_dp_s *) pObj);
            } else if (pBase->pIf->buildExt) {
                pMsg = pBase->pIf->buildExt (pBase->devAddr, pBase->regAddr, (*ppBase)->regAddr, (ty_obj_dp_s *) pObj);
            }

            if (!pMsg) {
                logErr ( COLOR_RED "mbus device write plugin->control call fail.1" COLOR_CLEAR);
                return -1;
            }
            if (!pMsg->pBuf || (pMsg->len == 0)) {
                if (pMsg->pBuf) {
                    free (pMsg->pBuf);
                    pMsg->pBuf = NULL;
                }
                free (pMsg);
                pMsg = NULL;
                logErr ( COLOR_RED "mbus device write plugin->control call fail.2" COLOR_CLEAR);
                return -1;
            }

            (*ppBase)->pSerial->send ((*ppBase)->pSerial, pMsg->pBuf, pMsg->len);
            free (pMsg->pBuf);
            pMsg->pBuf = NULL;
            free (pMsg);
            pMsg = NULL;
            usleep (1000 * 100 + pBase->delay * 1000);

            pBase->pSerial->close (pBase->pSerial);
        }

        pBase = pBase->pNext;
    }

    return 0;
}

void ModbusDevMonitor (void *param) {
    if (!param) {
        logErr ( "[knx dev Entry] param error!");
        return;
    }
    queue_t *pQueue = (queue_t *) param;
    MbusObj_t *pObj = NULL;
    TY_OBJ_DP_S tyObj;
    int ret = -1;
    char errorCode[16] = {0};

    while (true) {
        if (queue_get_wait (pQueue, (void **) &pObj) != 0) {
            continue;
        }
        if (!pObj) {
            logErr ( "[mbus dev monitor] queue_get_wait get pData == NULL!");
            continue;
        }
        memset (&tyObj, 0, sizeof (tyObj));
        char *endptr;
        errno = 0;
        long val = strtol ((char *) pObj->pPrimaryKey + 12, &endptr, 10);
        if (errno != 0 || endptr == (char *) pObj->pPrimaryKey + 12) {
            logErr ("Invalid device ID format");
            return;
        }
        uint32_t devId = (uint32_t) val;

        listSetMatchMethod (pMbusDevList, ModbusDevIdMatch);
        listNode *pNode = listSearchKey (pMbusDevList, &devId);
        if (!pNode) {
            logErr ( COLOR_RED "mbus device %s not found in the local list!" COLOR_CLEAR, pObj->pPrimaryKey);
            free (pObj);
            pObj = NULL;
            continue;
        }

        bool bReport = true;
        MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
        if (!(*ppBase)->element.bMaster) continue;

        if ((*ppBase)->element.primaryType == MODBUS_DEVICE_AIRCONDITION) {
            MbusAcd_t *pAir = (MbusAcd_t *) pNode->value;
            if (pObj->localPoint == pAir->pSwitch->localPoint) {// 空调开关
                tyObj.dpid = (uint8_t) pAir->pSwitch->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAir->pSwitch->type;
                pAir->pSwitch->value = pObj->value;
                pAir->pSwitch->rcvValue = pObj->rcvValue;
                if ((pAir->pSwitch->valueOld == pAir->pSwitch->value) && (pAir->pSwitch->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAir->pSwitch->forceSync = 0;
                    logDbg ( "mbus acd %s, report switch=%d", pObj->pPrimaryKey, pAir->pSwitch->value);
                }
                pAir->pSwitch->valueOld = pAir->pSwitch->value;
            } else if (pObj->localPoint == pAir->pSpeed->localPoint) {// 空调风速
                tyObj.dpid = (uint8_t) pAir->pSpeed->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAir->pSpeed->type;
                pAir->pSpeed->value = pObj->value;
                pAir->pSpeed->rcvValue = pObj->rcvValue;
                if ((pAir->pSpeed->valueOld == pAir->pSpeed->value) && (pAir->pSpeed->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAir->pSpeed->forceSync = 0;
                    logDbg ( "mbus acd %s, report speed=%d", pObj->pPrimaryKey, pAir->pSpeed->value);
                }
                pAir->pSpeed->valueOld = pAir->pSpeed->value;
            } else if (pObj->localPoint == pAir->pMode->localPoint) {// 空调模式
                tyObj.dpid = (uint8_t) pAir->pMode->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAir->pMode->type;
                pAir->pMode->value = pObj->value;
                pAir->pMode->rcvValue = pObj->rcvValue;
                if ((pAir->pMode->valueOld == pAir->pMode->value) && (pAir->pMode->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAir->pMode->forceSync = 0;
                    logDbg ( "mbus acd %s, report mode=%d", pObj->pPrimaryKey, pAir->pMode->value);
                }
                pAir->pMode->valueOld = pAir->pMode->value;
            } else if (pObj->localPoint == pAir->pTemp->localPoint) {// 空调温度
                tyObj.dpid = (uint8_t) pAir->pTemp->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAir->pTemp->type;
                pAir->pTemp->value = pObj->value;
                pAir->pTemp->rcvValue = pObj->rcvValue;
                if ((pAir->pTemp->valueOld == pAir->pTemp->value) && (pAir->pTemp->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAir->pTemp->forceSync = 0;
                    logDbg ( "mbus acd %s, report temp=%d", pObj->pPrimaryKey, pAir->pTemp->value);
                }
                pAir->pTemp->valueOld = pAir->pTemp->value;
            } else if (pObj->localPoint == pAir->pEnvTemp->localPoint) {// 空调环境温度
                tyObj.dpid = (uint8_t) pAir->pEnvTemp->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAir->pEnvTemp->type;
                pAir->pEnvTemp->value = pObj->value;
                pAir->pEnvTemp->rcvValue = pObj->rcvValue;
                bReport = false;
                if ((pAir->pEnvTemp->valueOld == pAir->pEnvTemp->value) && (pAir->pEnvTemp->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAir->pEnvTemp->forceSync = 0;
                    logDbg ( "mbus acd %s, report envtemp=%d", pObj->pPrimaryKey, pAir->pEnvTemp->value);
                }
                pAir->pEnvTemp->valueOld = pAir->pEnvTemp->value;
            } else if (pObj->localPoint == pAir->pErrorCode->localPoint) {// 空调故障码
                tyObj.dpid = (uint8_t) pAir->pErrorCode->cloudPoint;
                tyObj.value.dp_str = errorCode;
                *tyObj.value.dp_str = pObj->value >> 8;
                *(tyObj.value.dp_str + 1) = pObj->value & 0xff;
                tyObj.type = PROP_STR;
                pAir->pErrorCode->value = pObj->value;
                pAir->pErrorCode->rcvValue = pObj->rcvValue;
                if ((pAir->pErrorCode->valueOld == pAir->pErrorCode->value) && (pAir->pErrorCode->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAir->pErrorCode->forceSync = 0;
                    logDbg ( "mbus acd %s, report error=%s", pObj->pPrimaryKey, tyObj.value.dp_str);
                }
                pAir->pErrorCode->valueOld = pAir->pErrorCode->value;
            } else {
                logErr ( "the mbus acd(%s) mbus->plugin->feature(%d)(%s) not supported!", pObj->pPrimaryKey, pObj->localPoint, GetPointFeature (pObj->localPoint));

                continue;
            }
        } else if ((*ppBase)->element.primaryType == MODBUS_DEVICE_VENTILATION) {
            MbusVent_t *pVent = (MbusVent_t *) pNode->value;
            if (pObj->localPoint == pVent->pSwitch->localPoint) {// 新风开关
                tyObj.dpid = (uint8_t) pVent->pSwitch->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pVent->pSwitch->type;
                pVent->pSwitch->value = pObj->value;
                pVent->pSwitch->rcvValue = pObj->rcvValue;
                if ((pVent->pSwitch->valueOld == pVent->pSwitch->value) && (pVent->pSwitch->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    logDbg ( COLOR_PURPLE "[mbus vent] %s repeat report switch=%d" COLOR_CLEAR, pObj->pPrimaryKey, pVent->pSwitch->value);
                    goto MBUS_HEARTBEAT;
                } else {
                    pVent->pSwitch->forceSync = 0;
                    logDbg ( COLOR_PURPLE "[mbus vent] %s report switch=%d" COLOR_CLEAR, pObj->pPrimaryKey, pVent->pSwitch->value);
                }
                pVent->pSwitch->valueOld = pVent->pSwitch->value;
            } else if (pObj->localPoint == pVent->pSpeed->localPoint) {// 新风风速
                tyObj.dpid = (uint8_t) pVent->pSpeed->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pVent->pSpeed->type;
                pVent->pSpeed->value = pObj->value;
                pVent->pSpeed->rcvValue = pObj->rcvValue;
                if ((pVent->pSpeed->valueOld == pVent->pSpeed->value) && (pVent->pSpeed->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pVent->pSpeed->forceSync = 0;
                    logDbg ( "[mbus vent] %s report speed=%d", pObj->pPrimaryKey, pVent->pSpeed->value);
                }
                pVent->pSpeed->valueOld = pVent->pSpeed->value;
            } else {
                logErr ( "the mbus vent(%s) mbus->plugin->feature(%d)(%s) not supported!", pObj->pPrimaryKey, pObj->localPoint, GetPointFeature (pObj->localPoint));
                continue;
            }
        } else if ((*ppBase)->element.primaryType == MODBUS_DEVICE_FLOORHEATING) {
            MbusHeat_t *pHeat = (MbusHeat_t *) pNode->value;
            if (pObj->localPoint == pHeat->pSwitch->localPoint) {// 新风开关
                tyObj.dpid = (uint8_t) pHeat->pSwitch->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pHeat->pSwitch->type;
                pHeat->pSwitch->value = pObj->value;
                pHeat->pSwitch->rcvValue = pObj->rcvValue;
                if ((pHeat->pSwitch->valueOld == pHeat->pSwitch->value) && (pHeat->pSwitch->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pHeat->pSwitch->forceSync = 0;
                    logDbg ( "mbus heat %s, report switch=%d", pObj->pPrimaryKey, pHeat->pSwitch->value);
                }
                pHeat->pSwitch->valueOld = pHeat->pSwitch->value;
            } else if (pObj->localPoint == pHeat->pTemp->localPoint) {// 新风风速
                tyObj.dpid = (uint8_t) pHeat->pTemp->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pHeat->pTemp->type;
                pHeat->pTemp->value = pObj->value;
                pHeat->pTemp->rcvValue = pObj->rcvValue;
                if ((pHeat->pTemp->valueOld == pHeat->pTemp->value) && (pHeat->pTemp->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pHeat->pTemp->forceSync = 0;
                    logDbg ( "mbus vent %s, report speed=%d", pObj->pPrimaryKey, pHeat->pTemp->value);
                }
                pHeat->pTemp->valueOld = pHeat->pTemp->value;
            } else if (pObj->localPoint == pHeat->pEnvTemp->localPoint) {// 空调环境温度
                tyObj.dpid = (uint8_t) pHeat->pEnvTemp->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pHeat->pEnvTemp->type;
                pHeat->pEnvTemp->value = pObj->value;
                pHeat->pEnvTemp->rcvValue = pObj->rcvValue;
                bReport = false;
                if ((pHeat->pEnvTemp->valueOld == pHeat->pEnvTemp->value) && (pHeat->pEnvTemp->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pHeat->pEnvTemp->forceSync = 0;
                    logDbg ( "mbus heat %s, report envtemp=%d", pObj->pPrimaryKey, pHeat->pEnvTemp->value);
                }
                pHeat->pEnvTemp->valueOld = pHeat->pEnvTemp->value;
            } else {
                logErr ( "the mbus heat(%s) mbus->plugin->feature(%d)(%s) not supported!", pObj->pPrimaryKey, pObj->localPoint, GetPointFeature (pObj->localPoint));

                free (pObj);
                pObj = NULL;
                continue;
            }
        } else if ((*ppBase)->element.primaryType == MODBUS_DEVICE_AQI) {
            MbusAqi_t *pAqi = (MbusAqi_t *) pNode->value;
            if (pObj->localPoint == pAqi->pHumi->localPoint) {// 新风开关
                tyObj.dpid = (uint8_t) pAqi->pHumi->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAqi->pHumi->type;
                pAqi->pHumi->value = pObj->value;
                pAqi->pHumi->rcvValue = pObj->rcvValue;
                if ((pAqi->pHumi->valueOld == pAqi->pHumi->value) && (pAqi->pHumi->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAqi->pHumi->forceSync = 0;
                    logDbg ( "mbus aqi %s, report humi=%d", pObj->pPrimaryKey, pAqi->pHumi->value);
                }
                pAqi->pHumi->valueOld = pAqi->pHumi->value;
            } else if (pObj->localPoint == pAqi->pTemp->localPoint) {// 新风风速
                tyObj.dpid = (uint8_t) pAqi->pTemp->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAqi->pTemp->type;
                pAqi->pTemp->value = pObj->value;
                pAqi->pTemp->rcvValue = pObj->rcvValue;
                if ((pAqi->pTemp->valueOld == pAqi->pTemp->value) && (pAqi->pTemp->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAqi->pTemp->forceSync = 0;
                    logDbg ( "mbus aqi %s, report temp=%d", pObj->pPrimaryKey, pAqi->pTemp->value);
                }
                pAqi->pTemp->valueOld = pAqi->pTemp->value;
            } else if (pObj->localPoint == pAqi->pPM25->localPoint) {// 空调环境温度
                tyObj.dpid = (uint8_t) pAqi->pPM25->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAqi->pPM25->type;
                pAqi->pPM25->value = pObj->value;
                pAqi->pPM25->rcvValue = pObj->rcvValue;
                bReport = false;
                if ((pAqi->pPM25->valueOld == pAqi->pPM25->value) && (pAqi->pPM25->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAqi->pPM25->forceSync = 0;
                    logDbg ( "mbus aqi %s, report pm2.5=%d", pObj->pPrimaryKey, pAqi->pPM25->value);
                }
                pAqi->pPM25->valueOld = pAqi->pPM25->value;
            } else if (pObj->localPoint == pAqi->pCO2->localPoint) {// 空调环境温度
                tyObj.dpid = (uint8_t) pAqi->pCO2->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pAqi->pCO2->type;
                pAqi->pCO2->value = pObj->value;
                pAqi->pCO2->rcvValue = pObj->rcvValue;
                bReport = false;
                if ((pAqi->pCO2->valueOld == pAqi->pCO2->value) && (pAqi->pCO2->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pAqi->pCO2->forceSync = 0;
                    logDbg ( "mbus aqi %s, report co2=%d", pObj->pPrimaryKey, pAqi->pCO2->value);
                }
                pAqi->pCO2->valueOld = pAqi->pCO2->value;
            } else {
                logErr ( "the mbus aqi(%s) localPoint(%d) not supported!", pObj->pPrimaryKey, pObj->localPoint);
                free (pObj);
                pObj = NULL;
                continue;
            }
        } else if ((*ppBase)->element.primaryType == MODBUS_DEVICE_HUMIDIFIER) {
            ModbusHumidifier_t *pHumidifier = (ModbusHumidifier_t *) pNode->value;
            if (pObj->localPoint == pHumidifier->pHumidity->localPoint) {// Humidifier humidity
                tyObj.dpid = (uint8_t) pHumidifier->pHumidity->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pHumidifier->pHumidity->type;
                pHumidifier->pHumidity->value = pObj->value;
                pHumidifier->pHumidity->rcvValue = pObj->rcvValue;
                if ((pHumidifier->pHumidity->valueOld == pHumidifier->pHumidity->value) && (pHumidifier->pHumidity->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pHumidifier->pHumidity->forceSync = 0;
                    logDbg ( "mbus humidifier %s, report humidity=%d", pObj->pPrimaryKey, pHumidifier->pHumidity->value);
                }
                pHumidifier->pHumidity->valueOld = pHumidifier->pHumidity->value;
            } else if (pObj->localPoint == pHumidifier->pSwitch->localPoint) {// Humidifier switch
                tyObj.dpid = (uint8_t) pHumidifier->pSwitch->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pHumidifier->pSwitch->type;
                pHumidifier->pSwitch->value = pObj->value;
                pHumidifier->pSwitch->rcvValue = pObj->rcvValue;
                if ((pHumidifier->pSwitch->valueOld == pHumidifier->pSwitch->value) && (pHumidifier->pSwitch->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pHumidifier->pSwitch->forceSync = 0;
                    logDbg ( "mbus humidifier %s, report switch=%d", pObj->pPrimaryKey, pHumidifier->pSwitch->value);
                }
                pHumidifier->pSwitch->valueOld = pHumidifier->pSwitch->value;
            } else {
                logErr ( "the mbus humidifier(%s) localPoint(%d) not supported!", pObj->pPrimaryKey, pObj->localPoint);
                free (pObj);
                pObj = NULL;
                continue;
            }
        } else if ((*ppBase)->element.primaryType == MODBUS_DEVICE_PURIFIER) {
            ModbusPurifier_t *pPurifier = (ModbusPurifier_t *) pNode->value;
            if (pObj->localPoint == pPurifier->pFlow->localPoint) {// Purifier flow
                tyObj.dpid = (uint8_t) pPurifier->pFlow->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pPurifier->pFlow->type;
                pPurifier->pFlow->value = pObj->value;
                pPurifier->pFlow->rcvValue = pObj->rcvValue;
                if ((pPurifier->pFlow->valueOld == pPurifier->pFlow->value) && (pPurifier->pFlow->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pPurifier->pFlow->forceSync = 0;
                    logDbg ( "mbus purifier %s, report flow=%d", pObj->pPrimaryKey, pPurifier->pFlow->value);
                }
                pPurifier->pFlow->valueOld = pPurifier->pFlow->value;
            } else if (pObj->localPoint == pPurifier->pTDS->localPoint) {// Purifier TDS
                tyObj.dpid = (uint8_t) pPurifier->pTDS->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pPurifier->pTDS->type;
                pPurifier->pTDS->value = pObj->value;
                pPurifier->pTDS->rcvValue = pObj->rcvValue;
                if ((pPurifier->pTDS->valueOld == pPurifier->pTDS->value) && (pPurifier->pTDS->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pPurifier->pTDS->forceSync = 0;
                    logDbg ( "mbus purifier %s, report TDS=%d", pObj->pPrimaryKey, pPurifier->pTDS->value);
                }
                pPurifier->pTDS->valueOld = pPurifier->pTDS->value;
            } else if (pObj->localPoint == pPurifier->pFilterTime->localPoint) {// Purifier filter time
                tyObj.dpid = (uint8_t) pPurifier->pFilterTime->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pPurifier->pFilterTime->type;
                pPurifier->pFilterTime->value = pObj->value;
                pPurifier->pFilterTime->rcvValue = pObj->rcvValue;
                if ((pPurifier->pFilterTime->valueOld == pPurifier->pFilterTime->value) && (pPurifier->pFilterTime->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pPurifier->pFilterTime->forceSync = 0;
                    logDbg ( "mbus purifier %s, report FilterTime=%d", pObj->pPrimaryKey, pPurifier->pFilterTime->value);
                }
                pPurifier->pFilterTime->valueOld = pPurifier->pFilterTime->value;
            } else if (pObj->localPoint == pPurifier->pReset->localPoint) {// Purifier reset
                tyObj.dpid = (uint8_t) pPurifier->pReset->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pPurifier->pReset->type;
                pPurifier->pReset->value = pObj->value;
                pPurifier->pReset->rcvValue = pObj->rcvValue;
                if ((pPurifier->pReset->valueOld == pPurifier->pReset->value) && (pPurifier->pReset->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pPurifier->pReset->forceSync = 0;
                    logDbg ( "mbus purifier %s, report reset=%d", pObj->pPrimaryKey, pPurifier->pReset->value);
                }
                pPurifier->pReset->valueOld = pPurifier->pReset->value;
            } else {
                logErr ( "the mbus humidifier(%s) localPoint(%d) not supported!", pObj->pPrimaryKey, pObj->localPoint);
                free (pObj);
                pObj = NULL;
                continue;
            }
        } else if ((*ppBase)->element.primaryType == MODBUS_DEVICE_CURTAIN) {
            MbusCurtain_t *pCurtain = (MbusCurtain_t *) pNode->value;
            if (pObj->localPoint == pCurtain->pSwitch->localPoint) {
                tyObj.dpid = (uint8_t) pCurtain->pSwitch->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pCurtain->pSwitch->type;
                pCurtain->pSwitch->value = pObj->value;
                pCurtain->pSwitch->rcvValue = pObj->rcvValue;
                if ((pCurtain->pSwitch->valueOld == pCurtain->pSwitch->value) && (pCurtain->pSwitch->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pCurtain->pSwitch->forceSync = 0;
                    logDbg ( "mbus curtain %s, report switch=%d", pObj->pPrimaryKey, pCurtain->pSwitch->value);
                }
                pCurtain->pSwitch->valueOld = pCurtain->pSwitch->value;
            } else {
                logErr ( "the mbus curtain(%s) localPoint(%d) not supported!", pObj->pPrimaryKey, pObj->localPoint);
                free (pObj);
                pObj = NULL;
                continue;
            }
        } else if ((*ppBase)->element.primaryType == MODBUS_DEVICE_PANNEL) {
            MbusPanel_t *pPanel = (MbusPanel_t *) pNode->value;
            if (pObj->localPoint == pPanel->pSwitch->localPoint) {
                tyObj.dpid = (uint8_t) pPanel->pSwitch->cloudPoint;
                tyObj.value.dp_value = (int) pObj->value;
                tyObj.type = (uint8_t) pPanel->pSwitch->type;
                pPanel->pSwitch->value = pObj->value;
                pPanel->pSwitch->rcvValue = pObj->rcvValue;
                if ((pPanel->pSwitch->valueOld == pPanel->pSwitch->value) && (pPanel->pSwitch->forceSync++ < MBUS_FORCE_SYNC_NUM)) {
                    goto MBUS_HEARTBEAT;
                } else {
                    pPanel->pSwitch->forceSync = 0;
                    logDbg ( "mbus panel %s, report switch=%d", pObj->pPrimaryKey, pPanel->pSwitch->value);
                }
                pPanel->pSwitch->valueOld = pPanel->pSwitch->value;
            } else {
                logErr ( "the mbus panel(%s) localPoint(%d) not supported!", pObj->pPrimaryKey, pObj->localPoint);
                free (pObj);
                pObj = NULL;
                continue;
            }
        } else {
            logErr ( COLOR_RED "mbus device %s not supported now!" COLOR_CLEAR, pObj->pPrimaryKey);
            free (pObj);
            pObj = NULL;
            continue;
        }

        //        logTrc ( "device:%s, dpid:%d, value:%d", (char *) pObj->pPrimaryKey, tyObj.dpid, tyObj.value.dp_value);
        if ((*ppBase)->element.bMaster && (*ppBase)->element.pvForeignKey) {
            KnxVent_t *pKnxVent = (KnxVent_t *) (*ppBase)->element.pvForeignKey;
            KnxGroupWrite (pKnxVent->base.element.pPrimaryKey, &tyObj);
            logDbg ( COLOR_BLUE "[mbus sync] %s(%s) will be synchronized to knx(%s)" COLOR_CLEAR, GetDeviceType ((*ppBase)->element.primaryType), pObj->pPrimaryKey, pKnxVent->base.element.pPrimaryKey);
        } else {
            logDbg ( COLOR_BLUE "[mbus sync] %s(%s) will not be synchronized to knx" COLOR_CLEAR, GetDeviceType ((*ppBase)->element.primaryType), pObj->pPrimaryKey);
        }

    MBUS_HEARTBEAT:
        if ((*ppBase)->element.bMaster && g_bActive && (tuya_user_iot_misc_dev_desc_get ((*ppBase)->element.pPrimaryKey) != NULL)) {
            if (bReport) {
                ret = tuya_user_iot_report_obj_dp (pObj->pPrimaryKey, &tyObj, 1);
                if (ret != OPRT_OK) {
                    if (tyObj.type == PROP_STR) {
                        logErr ( "Report mbus(%s) data to cloud error, dpid=%d, type=%d, value=%s", pObj->pPrimaryKey, tyObj.dpid, (int) tyObj.type, tyObj.value.dp_str);
                    } else {
                        logErr ( "Report mbus(%s) data to cloud error, dpid=%d, type=%d, value=%d", pObj->pPrimaryKey, tyObj.dpid, (int) tyObj.type, tyObj.value.dp_value);
                    }
                } else {
                    if (tyObj.type == PROP_STR) {
                        logDbg ( "Report mbus(%s) data to cloud success, dpid=%d, type=%d, value=%s", pObj->pPrimaryKey, tyObj.dpid, (int) tyObj.type, tyObj.value.dp_str);
                    } else {
                        logDbg ( "Report mbus(%s) data to cloud success, dpid=%d, type=%d, value=%d", pObj->pPrimaryKey, tyObj.dpid, (int) tyObj.type, tyObj.value.dp_value);
                    }
                }
            }
            ret = tuya_user_iot_misc_dev_hb_fresh (pObj->pPrimaryKey);
            if (ret != 0) {
                logErr ( "tuya_user_iot_misc_dev_hb_fresh err: %d", ret);
            }
        }
        free (pObj);
        pObj = NULL;
    }

    logErr ( "ModbusDevMonitor thread crashed");
}

void Modbus_DevHealth (void *param) {
    char *endptr;
    errno = 0;
    long val = strtol ((char *) param + 12, &endptr, 10);
    if (errno != 0 || endptr == (char *) param + 12) {
        logErr ("Invalid device ID format");
        return;
    }
    uint32_t devId = (uint32_t) val;

    listSetMatchMethod (pMbusDevList, ModbusDevIdMatch);
    listNode *pNode = listSearchKey (pMbusDevList, &devId);
    if (!pNode) {
        logErr ( COLOR_RED "mbus device %s not found in the local list!" COLOR_CLEAR, (char *) param);
        return;
    }
    MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
    if ((!ppBase) || (!*ppBase)) {
        logErr ( COLOR_RED "ppBase is NULL!" COLOR_CLEAR);
        return;
    }

    if ((*ppBase)->bOnlineCheck == false) {
        tuya_user_iot_misc_dev_hb_fresh ((*ppBase)->element.pPrimaryKey);
        logTrc ( "mbus %s device not support online check!", (*ppBase)->element.pPrimaryKey);
    }

    if ((*ppBase)->hbIgnore++ < 5) {
        return;
    }
    (*ppBase)->hbIgnore = 0;
    logTrc ( "mbus device %s get heartbeat!", (char *) param);

    if (!(*ppBase)->pIf || (!(*ppBase)->pIf->build && !(*ppBase)->pIf->buildExt)) {
        logErr ( COLOR_RED "mbus heartbeat pIf=null" COLOR_CLEAR);
        return;
    }
    Msg_t *pMsg = NULL;
    if ((*ppBase)->pIf->build) {
        pMsg = (*ppBase)->pIf->build ((*ppBase)->devAddr, (*ppBase)->regAddr, (*ppBase)->regAddr, (const void *) pNode->value, NULL);
    } else if ((*ppBase)->pIf->buildExt) {
        pMsg = (*ppBase)->pIf->buildExt ((*ppBase)->devAddr, (*ppBase)->regAddr, (*ppBase)->regAddr, NULL);
    }
    if (!pMsg) {
        logErr ( COLOR_RED "mbus heartbeat plugin->control call fail" COLOR_CLEAR);
        return;
    }
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "mbus heartbeat plugin->control call fail" COLOR_CLEAR);
        return;
    }
    if (!(*ppBase)->pSerial) {
        free (pMsg);
        logErr ( COLOR_RED "mbus heartbeat pVent->pBase->pSerial=null." COLOR_CLEAR);
        return;
    }
    int ret = (*ppBase)->pSerial->open ((*ppBase)->pSerial);
    if (ret == 0) {
        (*ppBase)->pSerial->send ((*ppBase)->pSerial, pMsg->pBuf, pMsg->len);
        (*ppBase)->pSerial->recv (&(*ppBase)->pSerial);
    }
    free (pMsg->pBuf);
    free (pMsg);
}

void Mbus_SyncAllDevStatus (void *pv) {
    (void) pv;
    struct timespec tim, tim2;

    listIter iter;
    listNode *pNode;
    listRewind (pMbusDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
        if (!(*ppBase)->pIf || (!(*ppBase)->pIf->build && !(*ppBase)->pIf->buildExt)) {
            logErr ( COLOR_RED "mbus device pIf is null" COLOR_CLEAR);
            continue;
        }
        Msg_t *pMsg = NULL;
        if ((*ppBase)->pIf->build) {
            pMsg = (*ppBase)->pIf->build ((*ppBase)->devAddr, (*ppBase)->regAddr, (*ppBase)->regAddr, (const void *) pNode->value, NULL);
        } else if ((*ppBase)->pIf->buildExt) {
            pMsg = (*ppBase)->pIf->buildExt ((*ppBase)->devAddr, (*ppBase)->regAddr, (*ppBase)->regAddr, NULL);
        }
        if (!pMsg) {
            logErr ( COLOR_RED "mbus heartbeat plugin->control call fail! devId=%d, regAddr=%d, groupAddr=%d" COLOR_CLEAR, (*ppBase)->devAddr, (*ppBase)->regAddr, (*ppBase)->regAddr);
            continue;
        }

        if (!pMsg->pBuf || (pMsg->len == 0)) {
            if (pMsg->pBuf) {
                free (pMsg->pBuf);
                pMsg->pBuf = NULL;
            }
            free (pMsg);
            pMsg = NULL;
            logErr ( COLOR_RED "mbus heartbeat plugin->control call fail" COLOR_CLEAR);
            continue;
        }

        if (!(*ppBase)->pSerial) {
            free (pMsg);
            logErr ( COLOR_RED "mbus heartbeat pVent->pBase->pSerial=null." COLOR_CLEAR);
            continue;
        }

        int ret = (*ppBase)->pSerial->open ((*ppBase)->pSerial);
        if (ret == 0) {
            (*ppBase)->pSerial->send ((*ppBase)->pSerial, pMsg->pBuf, pMsg->len);
            if ((*ppBase)->bFeedback) {
                (*ppBase)->pSerial->recv (&(*ppBase)->pSerial);
            } else if (!(*ppBase)->bFullDuplex) {
                struct timespec tim3, tim4;
                tim3.tv_sec = 0;
                tim3.tv_nsec = (1000 * 1000 * (10 + (*ppBase)->delay));
                nanosleep (&tim3, &tim4);
                (*ppBase)->pSerial->close ((*ppBase)->pSerial);
            }
        } else {
            logErr ( COLOR_RED "mbus device write serial open fail." COLOR_CLEAR);
        }
        free (pMsg->pBuf);
        pMsg->pBuf = NULL;
        free (pMsg);
        pMsg = NULL;

        tim.tv_sec = 0;
        tim.tv_nsec = (1000 * 1000 * 350);
        nanosleep (&tim, &tim2);
    }
    logDbg ( "All mbus dev status is synchronized!");
}

void Mbus_RequestHB (threadpool hModbusThpool, const char *pDevId) {
    int ret = thpool_add_work (hModbusThpool, Modbus_DevHealth, (char *) pDevId);
    if (ret != 0) {
        logErr ( COLOR_RED "thpool_add_work fail!" COLOR_CLEAR);
    }
}

void Mbus_SyncDevStatus (threadpool hModbusThpool) {
    int ret = thpool_add_work (hModbusThpool, Mbus_SyncAllDevStatus, NULL);
    if (ret != 0) {
        logErr ( COLOR_RED "thpool_add_work fail!" COLOR_CLEAR);
    }
}

void Mbus_DelaySyncAllDevStatus (void *pv) {
    threadpool hMbusThpool = (threadpool) pv;

    while (true) {
        if (g_bActive) {
            DelayMs (1000 * 60);
        } else {
            DelayMs (1000 * 30);
        }

        int ret = thpool_add_work (hMbusThpool, Mbus_SyncAllDevStatus, NULL);
        if (ret != 0) {
            logErr ( COLOR_RED "thpool_add_work fail!" COLOR_CLEAR);
        }
    }
}

void Mbus_ForceSyncDevStatus (threadpool hMbusThpool) {
    int ret = thpool_add_work (hMbusThpool, Mbus_DelaySyncAllDevStatus, hMbusThpool);
    if (ret != 0) {
        logErr ( COLOR_RED "thpool_add_work fail!" COLOR_CLEAR);
    }
}

int ModbusInit (threadpool hModbusThpool, list_t *pLst) {
    listIter iter;
    listNode *pNode;
    logDbg ( "mbus module init!");

    pMonitorQueue = queue_create_limited (100);
    if (!pMonitorQueue) {
        logErr ( COLOR_RED "[ModbusInit] create pMonitorQueue fail!" COLOR_CLEAR);
        return -1;
    }
    int ret = thpool_add_work (hModbusThpool, ModbusDevMonitor, pMonitorQueue);
    if (ret != 0) {
        logErr ( COLOR_RED "thpool_add_work fail!" COLOR_CLEAR);
    }

    listRewind (pLst, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        MbusBase_t **ppBase = (MbusBase_t **) pNode->value;
        (*ppBase)->pMbusDataArrived = pMonitorQueue;
    }

    atexit (ModbusDestroy);

    return 0;
}

void ModbusDestroy (void) {
    logDbg ( "mbus module destroy!");
    queue_destroy (pMonitorQueue);
    // todo: destroy all the mbus device and free the memory
}

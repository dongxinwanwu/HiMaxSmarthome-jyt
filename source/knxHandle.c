#include "knxHandle.h"
#include "eibclient.h"
#include "list.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "modbusHandle.h"
#include "parseXML.h"
#include "publicDef.h"
#include "queue.h"
#include "tuya_gw_dp_api.h"
#include "tuya_gw_misc_api.h"
#include "zlog.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <time.h>
#include <unistd.h>

#define KNX_HW_ADDRESS_MIN 0x1001
#define KNX_HW_ADDRESS_MAX 0x1008

typedef enum {
    THREAD_NO_KNX_BUS_MONITOR = 1,
    THREAD_NO_KNX_DEV_MONITOR,
    THREAD_NO_KNX_DEV_WRITE
} ThreadNo_e;

// typedef struct KnxDataType {
//     char hwAddr[EIB_ADDR + PREFIX_SIZE];
//     char groupAddr[EIB_ADDR + PREFIX_SIZE];
//     uint16_t phy;
//     bool bRejected;
//     uint16_t value;
//     char cmd[10];
//     struct timespec timestamp;
// } KnxDataType_t;

typedef struct {
    char pPrimaryKey[DEVICE_ID_LENGTH + PREFIX_SIZE];
    uint32_t devId;
    TY_OBJ_DP_S obj;
} KnxWriteType_t;

typedef struct {
    int id;
    char name[32 + PREFIX_SIZE];
    queue_t *pQueue;
    threadpool hThpool;
    // todo:
} ThreadDog_t;

static const int KNX_KEYWORD_WRITE_LEN = strlen ("Write");

static queue_t *pMonitorQueue;
static queue_t *pWriteQueue;
static queue_t *pDogQueue;
static queue_t *pHeartbeatQueue;
extern bool g_bOnline;
extern bool g_bActive;

ThreadDog_t threadDog[3];

eibaddr_t readgaddr (const char *addr) {
    uint32_t a, b, c, res;

    res = sscanf (addr, "%u/%u/%u", &a, &b, &c);
    if (res == 3 && a <= 0x1F && b <= 0x07 && c <= 0xFF) {
        return (a << 11) | (b << 8) | c;
    }
    if (res == 2 && a <= 0x1F && b <= 0x7FF) {
        return (a << 11) | (b & 0x7FF);
    }
    if (sscanf (addr, "%x", &a) == 1 && a <= 0xFFFF) {
        return a;
    }
    logErr ("invalid group address format %s", addr);

    return -1;
}

int readBlock (uint8_t *buf, uint16_t value) {
    int len = (value / 256 ? 2 : 1);

    if (len == 2) {
        buf[0] = value / 256;
        buf[1] = value % 256;
    } else {
        buf[0] = value % 256;
    }

    return len;
}

__attribute__ ((unused)) int readHex (const char *addr) {
    uint32_t i;

    if (sscanf (addr, "%x", &i) == 1) {
        return i;
    }

    return -1;
}

int isKnxGroupAddr (const char *pBuf) {
    if (!pBuf) return -1;
    int prime = 0;
    int middle = 0;
    int sub = 0;

    int ret = sscanf (pBuf, "%2d/%1d/%3d", &prime, &middle, &sub);
    if (ret != 3) {
        return ret;
    } else {
        if ((prime > 31) || (middle > 7) || (sub > 255)) {
            return -1;
        }
    }

    return 0;
}

uint16_t KnxGrpAddr (const char *addr) {
    int a, b, c, res;

    res = sscanf (addr, "%u/%u/%u", &a, &b, &c);// NOLINT(cert-err34-c)
    if (res == 3 && a <= 0x1F && b <= 0x07 && c <= 0xFF) {
        return (a << 11) | (b << 8) | c;
    }
    if (res == 2 && a <= 0x1F && b <= 0x7FF) {
        return (a << 11) | (b & 0x7FF);
    }
    if (sscanf (addr, "%x", &a) == 1 && a <= 0xFFFF) {// NOLINT(cert-err34-c)
        return a;
    }
    logErr ("invalid group address format %s", addr);

    return -1;
}

uint16_t KnxHwAddr (const char *pBuf) {
    if (!pBuf) return (uint16_t) -1;
    int prime = 0;
    int middle = 0;
    int sub = 0;

    int ret = sscanf (pBuf, "%2d.%2d.%3d", &prime, &middle, &sub);
    if (ret != 3) {
        return (uint16_t) -1;
    } else {
        if ((prime > 15) || (middle > 15) || (sub > 255)) {
            return (uint16_t) -1;
        }
    }
    uint16_t addr = prime << 12;
    addr += middle << 8;
    addr += sub;

    return addr;
}

int Knx_ComparePhy (const char *pPhyA, const char *pPhyB) {
    if (!pPhyA || !pPhyB) return -1;
    uint16_t phyA = KnxHwAddr (pPhyA);
    uint16_t phyB = KnxHwAddr (pPhyB);

    if (phyA != phyB) return -1;

    return 0;
}

int Knx_CompareGroup (const char *pGrpA, const char *pGrpB) {
    if (!pGrpA || !pGrpB) return -1;
    eibaddr_t grpA = readgaddr (pGrpA);
    eibaddr_t grpB = readgaddr (pGrpB);

    if (grpA != grpB) return -1;

    return 0;
}

void KNX_FakePacket (char *pGrp, char *pValue) {
    uint32_t h, l;
    int ret = -1;

    KnxDataType_t *pData = (KnxDataType_t *) calloc (1, sizeof (KnxDataType_t));
    if (!pData) {
        logErr ("KNX_BusMonitorEntry calloc KnxDataType_t failed!");
        return;
    }
    snprintf (pData->groupAddr, EIB_ADDR, "%s", pGrp);

    ret = sscanf (pValue, "%02x %02x", &h, &l);
    if (ret == 2) {
        pData->value = (uint16_t) (h * 256 + l);
    } else if (ret == 1) {
        pData->value = (uint16_t) h;
    } else {
        logErr ("parse value error!");
        goto KNX_FAKE_PACKET_FAIL;
    }
    strcpy (pData->cmd, "Write");

    queue_put (pMonitorQueue, (void *) pData);

    return;

KNX_FAKE_PACKET_FAIL:
    free (pData);
}

int KNX_ParsePacket (const char *pBuf, KnxDataType_t *pKnxData) {
    if (!pBuf) return -1;
    UNUSED static const char *pFromKey = "from ";
    UNUSED static const char *pToKey = " to ";
    UNUSED static const char *pHopsKey = " hops";
    UNUSED static const char *pGroupWriteSmallKey = "A_GroupValue_Write (small) ";
    UNUSED static const char *pGroupWriteKey = "A_GroupValue_Write ";
    UNUSED static const char *pGroupReadKey = "A_GroupValue_Read";
    UNUSED static const char *pGroupResponseSmallKey = "A_GroupValue_Response (small) ";
    UNUSED static const char *pGroupResponseKey = "A_GroupValue_Response ";
    UNUSED static const char *pRepeated = "repeated";

    UNUSED const int KEYWORD_FROM_KEY_LEN = strlen ("from ");
    UNUSED static const int KEYWORD_TO_KEY_LEN = strlen (" to ");
    UNUSED static const int KEYWORD_HOPS_KEY_LEN = strlen (" hops");
    UNUSED static const int KEYWORD_GROUP_WRITE_SMALL_KEY_LEN = strlen ("A_GroupValue_Write (small) ");
    UNUSED static const int KEYWORD_GROUP_WRITE_KEY_LEN = strlen ("A_GroupValue_Write ");
    UNUSED static const int KEYWORD_GROUP_READ_KEY_LEN = strlen ("A_GroupValue_Read");
    UNUSED static const int KEYWORD_GROUP_RESPONSE_SMALL_KEY_LEN = strlen ("A_GroupValue_Response (small) ");
    UNUSED static const int KEYWORD_GROUP_RESPONSE_KEY_LEN = strlen ("A_GroupValue_Response ");
    UNUSED static const int KEYWORD_REPEATED_LEN = strlen ("repeated");

    uint32_t h, l;
    char *ret1 = strstr (pBuf, pFromKey);
    char *ret2 = strstr (pBuf, pToKey);
    char *ret3 = strstr (pBuf, pHopsKey);
    char *pValue = NULL;
    int ret = EOF;

    memset (pKnxData->cmd, 0, sizeof (pKnxData->cmd));
    memcpy (pKnxData->hwAddr, ret1 + KEYWORD_FROM_KEY_LEN, ret2 - ret1 - KEYWORD_FROM_KEY_LEN);
    memcpy (pKnxData->groupAddr, ret2 + KEYWORD_TO_KEY_LEN, ret3 - ret2 - KEYWORD_TO_KEY_LEN);

    if ((pValue = strstr (pBuf, pGroupWriteSmallKey))) {
        ret = sscanf (pValue + KEYWORD_GROUP_WRITE_SMALL_KEY_LEN, "%02x", &l);
        if (ret == 1) {
            pKnxData->value = (uint16_t) l;
        } else {
            logErr ("parse %s error!", pGroupWriteSmallKey);
            return -1;
        }
        strcpy (pKnxData->cmd, "Write");
    } else if ((pValue = strstr (pBuf, pGroupWriteKey))) {
        ret = sscanf (pValue + KEYWORD_GROUP_WRITE_KEY_LEN, "%02x %02x", &h, &l);
        if (ret == 2) {
            pKnxData->value = (uint16_t) (h * 256 + l);
        } else if (ret == 1) {
            pKnxData->value = (uint16_t) h;
        } else {
            logErr ("parse %s error!", pGroupWriteKey);
            return -1;
        }
        strcpy (pKnxData->cmd, "Write");
    }

    if ((pValue = strstr (pBuf, pGroupReadKey))) {
        strcpy (pKnxData->cmd, "Read");
        return -2;
    } else if ((pValue = strstr (pBuf, pRepeated))) {
        strcpy (pKnxData->cmd, "Repeated");
        return -3;
    }

    if ((pValue = strstr (pBuf, pGroupResponseSmallKey))) {
        ret = sscanf (pValue + KEYWORD_GROUP_RESPONSE_SMALL_KEY_LEN, "%02x", &l);
        if (ret == 1) {
            pKnxData->value = (uint16_t) l;
        } else {
            logErr ("parse %s error!", pGroupResponseSmallKey);
            return -1;
        }
        strcpy (pKnxData->cmd, "Response");
    } else if ((pValue = strstr (pBuf, pGroupResponseKey))) {
        ret = sscanf (pValue + KEYWORD_GROUP_RESPONSE_KEY_LEN, "%02x %02x", &h, &l);
        if (ret == 2) {
            pKnxData->value = (uint16_t) (h * 256 + l);
        } else if (ret == 1) {
            pKnxData->value = (uint16_t) h;
        } else {
            logErr ("parse %s error!", pGroupResponseKey);
            return -1;
        }
        strcpy (pKnxData->cmd, "Response");
    }

    pKnxData->phy = KnxHwAddr (pKnxData->hwAddr);
    if (pKnxData->phy >= KNX_HW_ADDRESS_MIN && pKnxData->phy <= KNX_HW_ADDRESS_MAX) return -4;

    return 0;
}

int Knx_AirconditionCtrl (KnxWriteType_t *pKnxData) {
    if (!pKnxData) {
        logErr ("Knx_AirconditionCtrl invalid param");
        return -1;
    }
    listIter iter;
    listNode *pNode;
    eibaddr_t gAddr = 0;

    int dataLen = 0;
    int ret = -1;

    /* KNX Aircondition traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (pKnx->element.primaryType != KNX_DEVICE_AIRCONDITION) continue;
        if (pKnxData->devId != pKnx->element.devId) continue;

        KnxAcd_t *pAC = (KnxAcd_t *) pNode->value;
        if (!pAC->base.pIf || !pAC->base.pIf->control) {
            logErr ("knx acd ctrl: pAir->base.pIf is invalid!");
            continue;
        }

        gAddr = 0xff;
        uint8_t knxBuf[10] = {0, 0x80};
        if (pAC->pSwitch && (pAC->pSwitch->cloudPoint == pKnxData->obj.dpid)) {
            pAC->base.pIf->control ((ty_obj_dp_s *) &pKnxData->obj, pAC->pSwitch);
            gAddr = readgaddr (pAC->pSwitch->addr);
            knxBuf[1] |= pAC->pSwitch->value & 0x3f;
            dataLen = 0;
        } else if (pAC->pSpeed && (pAC->pSpeed->cloudPoint == pKnxData->obj.dpid)) {
            pAC->base.pIf->control ((ty_obj_dp_s *) &pKnxData->obj, pAC->pSpeed);
            gAddr = readgaddr (pAC->pSpeed->addr);
            dataLen = readBlock (knxBuf + 2, pAC->pSpeed->value);
        } else if (pAC->pMode && (pAC->pMode->cloudPoint == pKnxData->obj.dpid)) {
            pAC->base.pIf->control ((ty_obj_dp_s *) &pKnxData->obj, pAC->pMode);
            gAddr = readgaddr (pAC->pMode->addr);
            dataLen = readBlock (knxBuf + 2, pAC->pMode->value);
        } else if (pAC->pTemp && (pAC->pTemp->cloudPoint == pKnxData->obj.dpid)) {
            pAC->base.pIf->control ((ty_obj_dp_s *) &pKnxData->obj, pAC->pTemp);
            gAddr = readgaddr (pAC->pTemp->addr);
            dataLen = readBlock (knxBuf + 2, pAC->pTemp->value);
        } else if (pAC->pErrorCode && (pAC->pErrorCode->cloudPoint == pKnxData->obj.dpid)) {
            // todo: error code
        }
        if (gAddr == 0xff) {
            continue;
        }
        if (g_bActive && (pAC->base.element.attri == DEVICE_ATTRI_ACTUATOR)) {
            ret = tuya_user_iot_report_obj_dp (pAC->base.element.pPrimaryKey, &pKnxData->obj, 1);
            if (ret != 0) {
                logErr ("Report knx acd-controller state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pAC->base.element.pPrimaryKey, pKnxData->obj.dpid, (int) pKnxData->obj.type, pKnxData->obj.value.dp_value);
                ret = -1;
            } else {
                logDbg ("Report knx acd-controller state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pAC->base.element.pPrimaryKey, pKnxData->obj.dpid, (int) pKnxData->obj.type, pKnxData->obj.value.dp_value);
            }
            tuya_user_iot_misc_dev_hb_fresh (pAC->base.element.pPrimaryKey);
        }

        EIBConnection *connect = EIBSocketURL ("ip:localhost");
        if (!connect) {
            logErr ("EIBSocketURL failed!");
            continue;
        }

        if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
            logErr ("EIBOpenT_Group failed!");
            EIBClose (connect);
            continue;
        }

        ret = EIBSendAPDU (connect, dataLen + 2, knxBuf);
        EIBClose (connect);
        if (ret == -1) {
            logErr ("EIBSendAPDU failed!");
        } else {
            logDbg (COLOR_GREEN "[knx acd ctrl] group:%d/%d/%d, value:%d is send." COLOR_CLEAR, gAddr >> 11, (gAddr >> 8) & 0x7, gAddr & 0xff, pKnxData->obj.value.dp_value);
            break;
        }
    }

    return ret;
}

int Knx_VentilationCtrl (KnxWriteType_t *pKnxData) {
    if (!pKnxData) {
        logErr ("Knx_VentilationCtrl invalid param");
        return -1;
    }
    listIter iter;
    listNode *pNode;
    eibaddr_t gAddr = 0;
    int dataLen = 0;
    int ret = -1;

    /* KNX Ventilation traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (pKnx->element.primaryType != KNX_DEVICE_VENTILATION) continue;
        if (pKnxData->devId != pKnx->element.devId) continue;

        KnxVent_t *pVen = (KnxVent_t *) pNode->value;
        if (!pVen->base.pIf || !pVen->base.pIf->control) {
            logDbg ("knx vent ctrl: pVen->base.pIf is invalid!");
            continue;
        }

        gAddr = 0xff;
        uint8_t knxBuf[10] = {0, 0x80};
        if (pVen->pSwitch && (pVen->pSwitch->cloudPoint == pKnxData->obj.dpid)) {
            pVen->base.pIf->control ((ty_obj_dp_s *) &pKnxData->obj, pVen->pSwitch);
            gAddr = readgaddr (pVen->pSwitch->addr);
            knxBuf[1] |= pVen->pSwitch->value & 0x3f;
            dataLen = 0;
        } else if (pVen->pSpeed && (pVen->pSpeed->cloudPoint == pKnxData->obj.dpid)) {
            pVen->base.pIf->control ((ty_obj_dp_s *) &pKnxData->obj, pVen->pSpeed);
            gAddr = readgaddr (pVen->pSpeed->addr);
            dataLen = readBlock (knxBuf + 2, pVen->pSpeed->value);
        } else if (pVen->pErrorCode && (pVen->pErrorCode->cloudPoint == pKnxData->obj.dpid)) {
            // todo: error code
        }

        if (gAddr == 0xff) {
            continue;
        }

        if (g_bActive && (pVen->base.element.attri == DEVICE_ATTRI_ACTUATOR)) {
            ret = tuya_user_iot_report_obj_dp (pVen->base.element.pPrimaryKey, &pKnxData->obj, 1);
            if (ret != 0) {
                logErr ("Report knx vent-controller state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pVen->base.element.pPrimaryKey, pKnxData->obj.dpid, (int) pKnxData->obj.type, pKnxData->obj.value.dp_value);
                ret = -1;
            } else {
                logDbg ("Report knx vent-controller state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pVen->base.element.pPrimaryKey, pKnxData->obj.dpid, (int) pKnxData->obj.type, pKnxData->obj.value.dp_value);
            }
            tuya_user_iot_misc_dev_hb_fresh (pVen->base.element.pPrimaryKey);
        }

        EIBConnection *connect = EIBSocketURL ("ip:localhost");
        if (!connect) {
            logErr ("EIBSocketURL failed!");
            continue;
        }

        if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
            logErr ("EIBOpenT_Group failed!");
            EIBClose (connect);
            continue;
        }

        ret = EIBSendAPDU (connect, dataLen + 2, knxBuf);
        EIBClose (connect);
        if (ret == -1) {
            logErr ("EIBSendAPDU failed!");
        } else {
            logDbg ("[knx vent ctrl] group:%d/%d/%d, value:%d is send.", gAddr >> 11, (gAddr >> 8) & 0x7, gAddr & 0xff, pKnxData->obj.value.dp_value);
            break;
        }
    }

    return ret;
}

int Knx_FloorHeatingCtrl (KnxWriteType_t *pKnxData) {
    if (!pKnxData) {
        logErr ("Knx_FloorHeatingCtrl invalid param");
        return -1;
    }
    listIter iter;
    listNode *pNode;
    eibaddr_t gAddr = 0;
    int dataLen = 0;
    int ret = -1;

    /* KNX Thermostat traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (pKnx->element.primaryType != KNX_DEVICE_FLOORHEATING) continue;
        if (pKnxData->devId != pKnx->element.devId) continue;

        KnxHeat_t *pHeat = (KnxHeat_t *) pNode->value;
        if (!pHeat->base.pIf || !pHeat->base.pIf->control) {
            logDbg ("knx heat ctrl: pHeat->base.pIf is invalid!");
            continue;
        }
        gAddr = 0xff;
        uint8_t knxBuf[10] = {0, 0x80};
        if (pHeat->pSwitch && (pHeat->pSwitch->cloudPoint == pKnxData->obj.dpid)) {
            if (!pHeat->base.pIf->control) {
                logDbg ("knx heat ctrl: pHeat->base.pIf->control is invalid!");
                continue;
            }
            pHeat->base.pIf->control ((ty_obj_dp_s *) &pKnxData->obj, pHeat->pSwitch);
            gAddr = readgaddr (pHeat->pSwitch->addr);
            knxBuf[1] |= pHeat->pSwitch->value & 0x3f;
            dataLen = 0;
        } else if (pHeat->pTemp && (pHeat->pTemp->cloudPoint == pKnxData->obj.dpid)) {
            pHeat->base.pIf->control ((ty_obj_dp_s *) &pKnxData->obj, pHeat->pTemp);
            gAddr = readgaddr (pHeat->pTemp->addr);
            dataLen = readBlock (knxBuf + 2, pHeat->pTemp->value);
        } else if (pHeat->pErrorCode && (pHeat->pErrorCode->cloudPoint == pKnxData->obj.dpid)) {
            // todo: error code
        }
        if (gAddr == 0xff) {
            continue;
        }

        if (g_bActive && (pHeat->base.element.attri == DEVICE_ATTRI_ACTUATOR)) {
            ret = tuya_user_iot_report_obj_dp (pHeat->base.element.pPrimaryKey, &pKnxData->obj, 1);
            if (ret != 0) {
                logErr ("Report knx heat-controller state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pHeat->base.element.pPrimaryKey, pKnxData->obj.dpid, (int) pKnxData->obj.type,
                        pKnxData->obj.value.dp_value);
                ret = -1;
            } else {
                logDbg ("Report knx heat-controller state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pHeat->base.element.pPrimaryKey, pKnxData->obj.dpid, (int) pKnxData->obj.type,
                        pKnxData->obj.value.dp_value);
            }
            tuya_user_iot_misc_dev_hb_fresh (pHeat->base.element.pPrimaryKey);
        }
        EIBConnection *connect = EIBSocketURL ("ip:localhost");
        if (!connect) {
            logErr ("EIBSocketURL failed!");
            continue;
        }

        if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
            logErr ("EIBOpenT_Group failed!");
            EIBClose (connect);
            continue;
        }
        ret = EIBSendAPDU (connect, dataLen + 2, knxBuf);
        EIBClose (connect);
        if (ret == -1) {
            logErr ("EIBSendAPDU failed!");
        } else {
            logDbg (COLOR_GREEN "[knx heat ctrl] group:%d/%d/%d, value:%d is send." COLOR_CLEAR, gAddr >> 11, (gAddr >> 8) & 0x7, gAddr & 0xff, pKnxData->obj.value.dp_value);
            break;
        }
    }

    return ret;
}

int Knx_PanelCtrl (KnxWriteType_t *pKnxData) {
    if (!pKnxData) {
        logErr ("Knx_PanelCtrl invalid param");
        return -1;
    }
    listIter iter;
    listNode *pNode;
    eibaddr_t gAddr = 0;
    int ret = -1;

    /* KNX panel traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if ((pKnx->element.primaryType < KNX_PANEL_ONE_KEY) || (pKnx->element.primaryType > KNX_PANEL_EIGHT_KEY)) continue;
        if (pKnxData->devId != pKnx->element.devId) continue;

        KnxPanel_t *pPanel = (KnxPanel_t *) pNode->value;
        bool bFound = false;

        for (int i = 0; i < (int) pPanel->keySum; i++) {
            if (pPanel->keys[i].point.id == pKnxData->obj.dpid) {
                pPanel->keys[i].point.value.b1 = (bool) pKnxData->obj.value.dp_bool;
                gAddr = readgaddr (pPanel->keys[i].pCtrl->addr);
                uint8_t knxBuf[3] = {0, 0x80};
                knxBuf[1] |= pPanel->keys[i].point.value.b1;

                if (g_bActive && (pPanel->keys[i].point.mode == POINT_ATTR_WRITE_ONLY)) {
                    ty_obj_dp_s obj = {0};
                    obj.dpid = pPanel->keys[i].point.id;
                    obj.type = PROP_BOOL;
                    obj.value.dp_bool = pPanel->keys[i].point.value.b1;

                    ret = tuya_user_iot_report_obj_dp (pPanel->base.element.pPrimaryKey, (const ty_dp_s *) &obj, 1);
                    if (ret != OPRT_OK) {
                        logErr ("Report panel state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pPanel->base.element.pPrimaryKey, obj.dpid, (int) obj.type, obj.value.dp_value);
                    } else {
                        logDbg ("Report panel state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pPanel->base.element.pPrimaryKey, obj.dpid, (int) obj.type, obj.value.dp_value);
                    }

                    ret = tuya_user_iot_misc_dev_hb_fresh (pPanel->base.element.pPrimaryKey);
                    if (ret != 0) {
                        logErr ("tuya_user_iot_misc_dev_hb_fresh err: %d", ret);
                    }
                }

                EIBConnection *connect = EIBSocketURL ("ip:localhost");
                if (!connect) {
                    logErr ("EIBSocketURL failed!");
                    continue;
                }
                if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
                    logErr ("EIBOpenT_Group failed!");
                    EIBClose (connect);
                    continue;
                }
                ret = EIBSendAPDU (connect, 2, knxBuf);
                EIBClose (connect);
                if (ret == -1) {
                    logErr ("EIBSendAPDU failed!");
                } else {
                    bFound = true;
                    logDbg (COLOR_GREEN "[knx pannel ctrl] group:%d/%d/%d, value:%d is send." COLOR_CLEAR, gAddr >> 11, (gAddr >> 8) & 0x7, gAddr & 0xff, pKnxData->obj.value.dp_value);
                    break;
                }
            }
        }
        if (bFound) {
            break;
        }
    }

    return ret;
}

int Knx_SmartPanelModeReport (void) {
    listIter iter;
    listNode *pNode;
    int ret = -1;
    /* KNX panel traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if ((pKnx->element.primaryType < KNX_MISC_PANEL_ONE_KEY) || (pKnx->element.primaryType > KNX_MISC_PANEL_FOUR_KEY))
            continue;

        KnxSmartPanel_t *pPanel = (KnxSmartPanel_t *) pNode->value;
        for (int i = 0; i < (int) pPanel->keySum; i++) {
            TY_OBJ_DP_S Obj;
            Obj.value.dp_enum = pPanel->chl[i].mode.point.value.u8;
            Obj.type = PROP_ENUM;
            Obj.dpid = (uint8_t) pPanel->chl[i].mode.point.id;

            if (g_bActive) {
                ret = tuya_user_iot_report_obj_dp (pPanel->base.element.pPrimaryKey, &Obj, 1);
                if (ret != OPRT_OK) {
                    logErr ("Report knx smart panel state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pPanel->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                    ret = -1;
                } else {
                    logDbg ("Report knx smart panel state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pPanel->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                }
                tuya_user_iot_misc_dev_hb_fresh (pPanel->base.element.pPrimaryKey);
            }
        }
    }

    return ret;
}

int Knx_SmartPanelCtrl (KnxWriteType_t *pKnxData) {
    if (!pKnxData) {
        logErr ("Knx_PanelCtrl invalid param");
        return -1;
    }
    listIter iter;
    listNode *pNode;
    eibaddr_t gAddr = 0;
    int ret = -1;
    /* KNX panel traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;

        if ((pKnx->element.primaryType < KNX_MISC_PANEL_ONE_KEY) || (pKnx->element.primaryType > KNX_MISC_PANEL_FOUR_KEY)) continue;
        if (pKnxData->devId != pKnx->element.devId) continue;
        KnxSmartPanel_t *pPanel = (KnxSmartPanel_t *) pNode->value;
        bool bFound = false;

        for (int i = 0; i < (int) pPanel->keySum; i++) {
            uint8_t knxBuf[10] = {0, 0x80};
            int dataLen = 0;

            if (pPanel->chl[i].key.point.id == pKnxData->obj.dpid) {
                if (pPanel->chl[i].mode.point.value.u8 == 0) {
                    pPanel->chl[i].key.point.value.b1 = (bool) pKnxData->obj.value.dp_bool;
                    gAddr = readgaddr (pPanel->chl[i].key.pCtrl->addr);
                    knxBuf[1] |= pPanel->chl[i].key.point.value.b1;
                    logDbg ("[smart panel ctrl] %s %s chl%d switch->%s", pPanel->base.element.pPrimaryKey, GetDeviceType (pKnx->element.primaryType), i, (pPanel->chl[i].key.point.value.b1 ? "on" : "off"));
                } else {
                    logErr (COLOR_RED "knx smart panel key function is not enabled!" COLOR_CLEAR);
                    break;
                }
            } else if (pPanel->chl[i].scene.point.id == pKnxData->obj.dpid) {
                if (pPanel->chl[i].mode.point.value.u8 == 1) {
                    gAddr = readgaddr (pPanel->chl[i].scene.pCtrl->addr);
                    dataLen = readBlock (knxBuf + 2, pPanel->chl[i].scene.point.value.u8);
                    logDbg ("[smart panel ctrl] %s %s chl%d scene->%d", pPanel->base.element.pPrimaryKey, GetDeviceType (pKnx->element.primaryType), i, pPanel->chl[i].scene.point.value.u8);
                } else {
                    logErr (COLOR_RED "knx smart panel scene function is not enabled!" COLOR_CLEAR);
                    break;
                }
            } else if (pPanel->chl[i].mode.point.id == pKnxData->obj.dpid) {
                pPanel->chl[i].mode.point.value.u8 = pKnxData->obj.value.dp_value;// 枚举值: switch_1, scene_1
                char *endptr;
                errno = 0;
                int id = strtol (pPanel->base.element.pPrimaryKey + 13, &endptr, 10);
                if (errno != 0 || endptr == (char *) pPanel->base.element.pPrimaryKey + 13) {
                    logErr ("Invalid device ID format");
                    exit (EXIT_FAILURE); /* force exit */
                }
                UpdateKnxSmartPanelMode (XML_OBJECT_MODEL_PATH, id, i + 1, (const uint8_t *) (pPanel->chl[i].mode.point.value.u8 ? "scene" : "switch"));
                logDbg ("[smart panel ctrl] %s %s chl%d mode->%s", pPanel->base.element.pPrimaryKey, GetDeviceType (pKnx->element.primaryType), i, (pPanel->chl[i].mode.point.value.u8 ? "scene" : "switch"));

                ty_dp_s Obj;
                Obj.value.dp_enum = pKnxData->obj.value.dp_enum;
                Obj.type = PROP_ENUM;
                Obj.dpid = pKnxData->obj.dpid;

                if (tuya_user_iot_report_obj_dp (pKnxData->pPrimaryKey, &Obj, 1) != OPRT_OK) {
                    logErr ("[smart panel report] pPrimaryKey=%s, dpid=%d, mode=%s to cloud failed!", pKnxData->pPrimaryKey, pKnxData->obj.dpid, (pKnxData->obj.value.dp_enum ? "scene" : "switch"));
                } else {
                    logDbg ("[knx smart panel] report pPrimaryKey=%s, dpid=%d, mode=%s to cloud success!", pKnxData->pPrimaryKey, pKnxData->obj.dpid, (pKnxData->obj.value.dp_enum ? "scene" : "switch"));
                }

                return 0;
            } else {
                continue;
            }

            EIBConnection *connect = EIBSocketURL ("ip:localhost");
            if (!connect) {
                logErr ("EIBSocketURL failed!");
                continue;
            }
            if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
                logErr ("EIBOpenT_Group failed!");
                EIBClose (connect);
                continue;
            }
            ret = EIBSendAPDU (connect, dataLen + 2, knxBuf);
            EIBClose (connect);
            if (ret == -1) {
                logErr ("EIBSendAPDU failed!");
            } else {
                bFound = true;
                logDbg (COLOR_GREEN "[knx smart pannel ctrl] group:%d/%d/%d, value:%d is send." COLOR_CLEAR, gAddr >> 11, (gAddr >> 8) & 0x7, gAddr & 0xff, pKnxData->obj.value.dp_value);

                break;
            }
        }
        if (bFound) {
            break;
        }
    }

    return ret;
}

int Knx_DimmerCtrl (KnxWriteType_t *pKnxData) {
    if (!pKnxData) {
        logErr ("Knx_DimmerCtrl invalid param");
        return -1;
    }
    listIter iter;
    listNode *pNode;
    eibaddr_t gAddr = 0;
    int ret = -1;

    /* KNX dimmer traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if ((pKnx->element.primaryType < KNX_DIMMER_ONE_KEY) || (pKnx->element.primaryType > KNX_DIMMER_FOUR_KEY)) continue;
        if (pKnxData->devId != pKnx->element.devId) continue;

        KnxDimmer_t *pDimmer = (KnxDimmer_t *) pNode->value;
        bool bFound = false;

        int dataLen = 0;
        uint8_t knxBuf[10] = {0, 0x80};
        for (int i = 0; i < (int) pDimmer->keySum; i++) {
            if (pDimmer->chl[i].bright.point.id == pKnxData->obj.dpid) {
                pDimmer->chl[i].bright.point.value.u8 = pKnxData->obj.value.dp_value * 255 / pDimmer->chl[i].bright.point.attr.attr.value.max.u16;
                gAddr = readgaddr (pDimmer->chl[i].bright.pCtrl->addr);
                dataLen = readBlock (knxBuf + 2, pDimmer->chl[i].bright.point.value.u8);

                if (g_bActive && (pDimmer->chl[i].bright.point.mode == POINT_ATTR_WRITE_ONLY)) {
                    ty_obj_dp_s obj = {0};
                    obj.dpid = pDimmer->chl[i].bright.point.id;
                    obj.type = PROP_VALUE;
                    obj.value.dp_value = pDimmer->chl[i].bright.point.value.u8;

                    ret = tuya_user_iot_report_obj_dp (pDimmer->base.element.pPrimaryKey, (const ty_dp_s *) &obj, 1);
                    if (ret != OPRT_OK) {
                        logErr ("Report dimmer state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                                pDimmer->base.element.pPrimaryKey, obj.dpid, (int) obj.type, obj.value.dp_value);
                    } else {
                        logDbg ("Report dimmer state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                                pDimmer->base.element.pPrimaryKey, obj.dpid, (int) obj.type, obj.value.dp_value);
                    }

                    ret = tuya_user_iot_misc_dev_hb_fresh (pDimmer->base.element.pPrimaryKey);
                    if (ret != 0) {
                        logErr ("tuya_user_iot_misc_dev_hb_fresh err: %d", ret);
                    }
                }

                logDbg (COLOR_GREEN "[knx dimmer] bright%d->ctrl: %d/%d/%d, value:%d is requested." COLOR_CLEAR, i, gAddr >> 11, (gAddr >> 8) & 0x7, gAddr & 0xff, pDimmer->chl[i].bright.point.value.u8);
            } else if (pDimmer->chl[i].key.point.id == pKnxData->obj.dpid) {
                pDimmer->chl[i].key.point.value.b1 = (bool) pKnxData->obj.value.dp_bool;
                gAddr = readgaddr (pDimmer->chl[i].key.pCtrl->addr);
                knxBuf[1] |= (uint8_t) pDimmer->chl[i].key.point.value.b1 & 0x3f;

                if (g_bActive && (pDimmer->chl[i].key.point.mode == POINT_ATTR_WRITE_ONLY)) {
                    ty_obj_dp_s obj = {0};
                    obj.dpid = pDimmer->chl[i].key.point.id;
                    obj.type = PROP_BOOL;
                    obj.value.dp_bool = pKnxData->obj.value.dp_bool;

                    ret = tuya_user_iot_report_obj_dp (pDimmer->base.element.pPrimaryKey, (const ty_dp_s *) &obj, 1);
                    if (ret != OPRT_OK) {
                        logErr ("Report dimmer state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                                pDimmer->base.element.pPrimaryKey, obj.dpid, (int) obj.type, obj.value.dp_value);
                    } else {
                        logDbg ("Report dimmer state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d",
                                pDimmer->base.element.pPrimaryKey, obj.dpid, (int) obj.type, obj.value.dp_value);
                    }

                    ret = tuya_user_iot_misc_dev_hb_fresh (pDimmer->base.element.pPrimaryKey);
                    if (ret != 0) {
                        logErr ("tuya_user_iot_misc_dev_hb_fresh err: %d", ret);
                    }
                }

                logDbg (COLOR_GREEN "[knx dimmer]switch%d->ctrl: %d/%d/%d, value:%d is requested." COLOR_CLEAR, i, gAddr >> 11, (gAddr >> 8) & 0x7, gAddr & 0xff, pKnxData->obj.value.dp_value);
            } else {
                continue;
            }

            EIBConnection *connect = EIBSocketURL ("ip:localhost");
            if (!connect) {
                logErr ("EIBSocketURL failed!");
                continue;
            }
            if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
                logErr ("EIBOpenT_Group failed!");
                EIBClose (connect);
                continue;
            }
            ret = EIBSendAPDU (connect, dataLen + 2, knxBuf);
            EIBClose (connect);
            if (ret == -1) {
                logErr ("EIBSendAPDU failed!");
            } else {
                bFound = true;
                break;
            }
        }
        if (bFound) {
            break;
        }
    }

    return ret;
}

int Knx_CurtainCtrl (KnxWriteType_t *pKnxData) {
    if (!pKnxData) {
        logErr ("Knx_CurtainCtrl invalid param");
        return -1;
    }
    listIter iter;
    listNode *pNode;
    int ret = -1;

    /* KNX curtain traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (pKnx->element.primaryType != KNX_DEVICE_CURTAIN) continue;

        KnxCurtain_t *pCurtain = (KnxCurtain_t *) pNode->value;
        if (pKnxData->devId != pKnx->element.devId) continue;
        logDbg ("[knx curtain ctrl] pKnxData->obj.dpid=%d", pKnxData->obj.dpid);
        char addr[EIB_ADDR + PREFIX_SIZE];
        uint8_t value = 0;

        if (pKnxData->obj.value.dp_enum == 0) {// open
            if (pCurtain->pOpen->point.id == pKnxData->obj.dpid) {
                pCurtain->pOpen->point.value.i32 = value = 0;
                snprintf (addr, sizeof (addr), "%s", pCurtain->pOpen->pCtrl->addr);
            }
        } else if (pKnxData->obj.value.dp_enum == 2) {// close
            if (pCurtain->pClose->point.id == pKnxData->obj.dpid) {
                pCurtain->pClose->point.value.i32 = value = 1;
                snprintf (addr, sizeof (addr), "%s", pCurtain->pClose->pCtrl->addr);
            }
        } else if (pKnxData->obj.value.dp_enum == 1) {// stop
            if (pCurtain->pStop->point.id == pKnxData->obj.dpid) {
                pCurtain->pStop->point.value.i32 = value = 0;
                snprintf (addr, sizeof (addr), "%s", pCurtain->pStop->pCtrl->addr);
            }
        } else
            continue;

        eibaddr_t gAddr = readgaddr (addr);
        uint8_t knxBuf[3] = {0, 0x80};
        knxBuf[1] |= value & 0x3f;
        logDbg (COLOR_GREEN "knx curtain ctrl: group:%d/%d/%d, value:%d is requested." COLOR_CLEAR, gAddr >> 11, (gAddr >> 8) & 0x7, gAddr & 0xff, value);

        EIBConnection *connect = EIBSocketURL ("ip:localhost");
        if (!connect) {
            logErr ("EIBSocketURL failed!");
            continue;
        }
        if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
            logErr ("EIBOpenT_Group failed!");
            EIBClose (connect);
            continue;
        }
        ret = EIBSendAPDU (connect, 2, knxBuf);
        EIBClose (connect);
        if (ret == -1) {
            logErr ("EIBSendAPDU failed!");
        }
    }

    return ret;
}

int Knx_AcdReport (KnxDataType_t *pData) {
    if (!pData) {
        logErr ("Knx_Aircondition Report invalid param");
        return -1;
    }

    listIter iter;
    listNode *pNode;
    TY_OBJ_DP_S Obj;
    struct timespec ts;
    int64_t diffMs = 0;
    int ret = -1;

    clock_gettime (CLOCK_MONOTONIC, &ts);
    /* KNX Airconditon traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (pKnx->element.primaryType != KNX_DEVICE_AIRCONDITION) continue;
        KnxAcd_t *pAC = (KnxAcd_t *) pNode->value;
        KnxObj_t *pKnxObj = NULL;
        if ((pAC->base.element.bMaster) && (tuya_user_iot_misc_dev_desc_get (pAC->base.element.pPrimaryKey) == NULL)) break;

        memset (&Obj, 0, sizeof (Obj));
        bool bFlag = false;
        if ((pKnxObj = pAC->pSwitchStatus) && (Knx_CompareGroup (pAC->pSwitchStatus->addr, pData->groupAddr) == 0)) {
            pAC->pSwitchStatus->timestamp = ts;
            pAC->pSwitchStatus->value = pData->value;
            Obj.dpid = (uint8_t) pAC->pSwitchStatus->cloudPoint;
            Obj.value.dp_value = pData->value;
        } else if ((pKnxObj = pAC->pSpeedStatus) && (Knx_CompareGroup (pAC->pSpeedStatus->addr, pData->groupAddr) == 0)) {
            pAC->pSpeedStatus->timestamp = ts;
            logDbg ("[knx acd report] speed->groupAddr=%s, speed->value=0x%x", pData->groupAddr, pData->value);
            pAC->pSpeedStatus->value = pData->value;
            Obj.dpid = (uint8_t) pAC->pSpeedStatus->cloudPoint;
            Obj.value.dp_value = pData->value;
        } else if ((pKnxObj = pAC->pModeStatus) && (Knx_CompareGroup (pAC->pModeStatus->addr, pData->groupAddr) == 0)) {
            pAC->pModeStatus->timestamp = ts;
            logDbg ("[knx acd report] mode->groupAddr=%s, mode->value=0x%x", pData->groupAddr, pData->value);
            pAC->pModeStatus->value = pData->value;
            Obj.dpid = (uint8_t) pAC->pModeStatus->cloudPoint;
            Obj.value.dp_value = pData->value;
        } else if ((pKnxObj = pAC->pTempStatus) && (Knx_CompareGroup (pAC->pTempStatus->addr, pData->groupAddr) == 0)) {
            pAC->pTempStatus->timestamp = ts;
            logDbg ("[knx acd report] temp->groupAddr=%s, temp->value=0x%x", pData->groupAddr, pData->value);
            pAC->pTempStatus->value = pData->value;
            Obj.dpid = (uint8_t) pAC->pTempStatus->cloudPoint;
            Obj.value.dp_value = pData->value;
        } else if ((pKnxObj = pAC->pEnvTemp) && (Knx_CompareGroup (pAC->pEnvTemp->addr, pData->groupAddr) == 0)) {
            pAC->pEnvTemp->timestamp = ts;
            logDbg ("[knx acd report] env->groupAddr=%s, env->value=0x%x", pData->groupAddr, pData->value);
            pAC->pEnvTemp->value = pData->value;
            Obj.dpid = (uint8_t) pAC->pEnvTemp->cloudPoint;
            Obj.value.dp_value = pData->value;
            bFlag = true;
        } else if ((pKnxObj = pAC->pErrorCode) && (Knx_CompareGroup (pAC->pErrorCode->addr, pData->groupAddr) == 0)) {
            logDbg ("[knx acd report] error->groupAddr=%s, error->value=0x%x", pData->groupAddr, pData->value);
            pAC->pErrorCode->value = pData->value;
            Obj.dpid = (uint8_t) pAC->pErrorCode->cloudPoint;
            bFlag = true;
        } else {
            continue;
        }

        if (!pAC->base.pIf || !pAC->base.pIf->report) {
            logErr ("[knx acd] pAC->base.pIf is invalid!");
            continue;
        }
        pAC->base.pIf->report (pKnxObj, (ty_obj_dp_s *) &Obj);

        logTrc ("[knx acd report] cloudPoint=%d, value=%d", Obj.dpid, Obj.value.dp_value);

        if (pKnx->element.bMaster) {
            if (g_bActive) {
                ret = tuya_user_iot_report_obj_dp (pAC->base.element.pPrimaryKey, &Obj, 1);
                if (ret != OPRT_OK) {
                    logErr ("Report knx acd state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pAC->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                    ret = -1;
                } else {
                    logDbg ("Report knx acd state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pAC->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                }
                tuya_user_iot_misc_dev_hb_fresh (pAC->base.element.pPrimaryKey);
            }
        } else {
            MbusBase_t **ppBase = (MbusBase_t **) pKnx->element.pvForeignKey;
            if (!bFlag) {
                logDbg ("knx acd(%s) status will be synchronized to mbus acd(%s), dpid=%d, value=%d", pAC->base.element.pPrimaryKey, (*ppBase)->element.pPrimaryKey, Obj.dpid, Obj.value.dp_value);
                ret = ModbusDevWrite ((*ppBase)->element.pPrimaryKey, &Obj);
            } else {
                if (g_bActive && (tuya_user_iot_misc_dev_desc_get ((*ppBase)->element.pPrimaryKey) != NULL)) {
                    ret = tuya_user_iot_report_obj_dp ((*ppBase)->element.pPrimaryKey, &Obj, 1);
                    if (ret != OPRT_OK) {
                        logErr ("Report mbus acd state(knx ro) to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", (*ppBase)->element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                        ret = -1;
                    } else {
                        logDbg ("Report mbus acd state(knx ro) to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", (*ppBase)->element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                    }
                }
            }
        }
    }

    return ret;
}

int Knx_VentReport (KnxDataType_t *pData) {
    if (!pData) {
        logErr ("Knx_VentReport invalid param");
        return -1;
    }

    listIter iter;
    listNode *pNode;
    TY_OBJ_DP_S Obj;
    struct timespec ts;
    int64_t diffMs = 0;
    int ret = -1;

    clock_gettime (CLOCK_MONOTONIC, &ts);

    /* KNX Ventilation traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (pKnx->element.primaryType != KNX_DEVICE_VENTILATION) continue;
        if ((pKnx->element.bMaster) && (tuya_user_iot_misc_dev_desc_get (pKnx->element.pPrimaryKey) == NULL)) continue;
        KnxVent_t *pVen = (KnxVent_t *) pNode->value;
        KnxObj_t *pKnxObj = NULL;

        memset (&Obj, 0, sizeof (Obj));
        bool bFlag = false;
        if ((pKnxObj = pVen->pSwitchStatus) && (Knx_CompareGroup (pVen->pSwitchStatus->addr, pData->groupAddr) == 0)) {
            pVen->pSwitchStatus->timestamp = ts;
            pVen->pSwitchStatus->value = pData->value;
            Obj.dpid = (uint8_t) pVen->pSwitchStatus->cloudPoint;
            Obj.value.dp_value = pData->value;
        } else if ((pKnxObj = pVen->pSpeedStatus) && (Knx_CompareGroup (pVen->pSpeedStatus->addr, pData->groupAddr) == 0)) {
            pVen->pSpeedStatus->timestamp = ts;
            pVen->pSpeedStatus->value = pData->value;
            Obj.dpid = (uint8_t) pVen->pSpeedStatus->cloudPoint;
            Obj.value.dp_value = pData->value;
        } else if ((pKnxObj = pVen->pErrorCode) && (Knx_CompareGroup (pVen->pErrorCode->addr, pData->groupAddr) == 0)) {
            pVen->pErrorCode->value = pData->value;
            Obj.dpid = (uint8_t) pVen->pErrorCode->cloudPoint;
            bFlag = true;
        } else if ((pKnxObj = pVen->pEnvTemp) && (Knx_CompareGroup (pVen->pEnvTemp->addr, pData->groupAddr) == 0)) {
            pVen->pEnvTemp->value = pData->value;
            Obj.dpid = (uint8_t) pVen->pEnvTemp->cloudPoint;
            Obj.value.dp_value = pData->value;
            bFlag = true;
        } else {
            continue;
        }
        if (!pVen->base.pIf || !pVen->base.pIf->report) {
            logErr ("knx vent report: pVen->base.pIf is invalid");
            continue;
        }
        pVen->base.pIf->report (pKnxObj, (ty_obj_dp_s *) &Obj);

        if (!pVen->base.element.bMaster) {
            MbusBase_t **ppBase = (MbusBase_t **) pVen->base.element.pvForeignKey;
            if (!bFlag) {
                logDbg (COLOR_RED "knx vent(%s) status will be synchronized to mbus vent(%s)" COLOR_CLEAR, pVen->base.element.pPrimaryKey, (*ppBase)->element.pPrimaryKey);
                ModbusDevWrite ((*ppBase)->element.pPrimaryKey, &Obj);
            } else {
                if (g_bActive && (tuya_user_iot_misc_dev_desc_get ((*ppBase)->element.pPrimaryKey) != NULL)) {
                    ret = tuya_user_iot_report_obj_dp ((*ppBase)->element.pPrimaryKey, &Obj, 1);
                    if (ret != OPRT_OK) {
                        logErr ("Report mbus vent state(knx ro) to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", (*ppBase)->element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                        ret = -1;
                    } else {
                        logDbg ("Report mbus vent state(knx ro) to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", (*ppBase)->element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                    }
                }
            }
        } else {
            if (g_bActive) {
                ret = tuya_user_iot_report_obj_dp (pVen->base.element.pPrimaryKey, &Obj, 1);
                if (ret != 0) {
                    logErr ("[knx vent]report cloud failure, primaryKey=%s, dpid=%d, type=%d, value=%d", pVen->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                    ret = -1;
                } else {
                    logDbg ("[knx vent]report cloud success, primaryKey=%s, dpid=%d, type=%d, value=%d", pVen->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                }
                tuya_user_iot_misc_dev_hb_fresh (pVen->base.element.pPrimaryKey);
            }
        }
    }

    return ret;
}

int Knx_HeatReport (KnxDataType_t *pData) {
    if (!pData) {
        logErr ("Knx_HeatReport invalid param");
        return -1;
    }

    listIter iter;
    listNode *pNode;
    TY_OBJ_DP_S Obj;
    struct timespec ts;
    int64_t diffMs = 0;
    int ret = -1;

    clock_gettime (CLOCK_MONOTONIC, &ts);

    /* KNX FloorHeating traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (pKnx->element.primaryType != KNX_DEVICE_FLOORHEATING) continue;

        KnxHeat_t *pHeat = (KnxHeat_t *) pNode->value;
        if ((pHeat->base.element.bMaster) && (tuya_user_iot_misc_dev_desc_get (pHeat->base.element.pPrimaryKey) == NULL)) break;

        if (pData->bRejected && (pHeat->base.element.attri == DEVICE_ATTRI_CONTROLLER)) {
            logErr (COLOR_RED "[knx heat report] %s %s is rejected!" COLOR_CLEAR, pHeat->base.element.pPrimaryKey, pData->groupAddr);
            continue;
        }

        memset (&Obj, 0, sizeof (Obj));
        if (pHeat->pSwitchStatus && (Knx_CompareGroup (pHeat->pSwitchStatus->addr, pData->groupAddr) == 0)) {
            pHeat->pSwitchStatus->timestamp = ts;
            pHeat->pSwitchStatus->value = pData->value;
            Obj.dpid = (uint8_t) pHeat->pSwitchStatus->cloudPoint;
            pHeat->base.pIf->report (pHeat->pSwitchStatus, (ty_obj_dp_s *) &Obj);
        } else if (pHeat->pTempStatus && (Knx_CompareGroup (pHeat->pTempStatus->addr, pData->groupAddr) == 0)) {
            pHeat->pTempStatus->timestamp = ts;
            pHeat->pTempStatus->value = pData->value;
            Obj.dpid = (uint8_t) pHeat->pTempStatus->cloudPoint;
            pHeat->base.pIf->report (pHeat->pTempStatus, (ty_obj_dp_s *) &Obj);
        } else if (pHeat->pEnvTemp && (Knx_CompareGroup (pHeat->pEnvTemp->addr, pData->groupAddr) == 0)) {
            pHeat->pEnvTemp->value = pData->value;
            Obj.dpid = (uint8_t) pHeat->pEnvTemp->cloudPoint;
            pHeat->base.pIf->report (pHeat->pEnvTemp, (ty_obj_dp_s *) &Obj);
        } else if (pHeat->pValve && (Knx_CompareGroup (pHeat->pValve->addr, pData->groupAddr) == 0)) {
            pHeat->pValve->value = pData->value;
            Obj.dpid = (uint8_t) pHeat->pValve->cloudPoint;
            pHeat->base.pIf->report (pHeat->pValve, (ty_obj_dp_s *) &Obj);
        } else if (pHeat->pErrorCode && (Knx_CompareGroup (pHeat->pErrorCode->addr, pData->groupAddr) == 0)) {
            pHeat->pErrorCode->value = pData->value;
            Obj.dpid = (uint8_t) pHeat->pErrorCode->cloudPoint;
            pHeat->base.pIf->report (pHeat->pErrorCode, (ty_obj_dp_s *) &Obj);
        } else {
            continue;
        }

        if (pKnx->element.bMaster && g_bActive) {
            ret = tuya_user_iot_report_obj_dp (pHeat->base.element.pPrimaryKey, &Obj, 1);
            if (ret != OPRT_OK) {
                logErr ("Report knx heat state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pHeat->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                ret = -1;
            } else {
                logDbg ("Report knx heat state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pHeat->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
            }
            tuya_user_iot_misc_dev_hb_fresh (pHeat->base.element.pPrimaryKey);
        }
    }

    return ret;
}

int Knx_PanelReport (KnxDataType_t *pData) {
    if (!pData) {
        logErr ("Knx_PanelReport invalid param");
        return -1;
    }

    listIter iter;
    listNode *pNode;
    TY_OBJ_DP_S Obj;
    struct timespec ts;
    int64_t diffMs = 0;
    int ret = -1;

    clock_gettime (CLOCK_MONOTONIC, &ts);

    /* KNX panel traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if ((pKnx->element.bMaster) && (tuya_user_iot_misc_dev_desc_get (pKnx->element.pPrimaryKey) == NULL)) break;

        if ((pKnx->element.primaryType < KNX_PANEL_ONE_KEY) || (pKnx->element.primaryType > KNX_PANEL_EIGHT_KEY))
            continue;

        KnxPanel_t *pPanel = (KnxPanel_t *) pNode->value;

        for (int i = 0; i < (int) pPanel->keySum; i++) {
            memset (&Obj, 0, sizeof (Obj));
            if (!pPanel->keys[i].pFeedback) continue;
            //            uint32_t maxLen = MAX (strlen (pPanel->keys[i].pFeedback->addr), strlen (pData->groupAddr));
            if (Knx_CompareGroup (pPanel->keys[i].pFeedback->addr, pData->groupAddr) == 0) {
                pPanel->keys[i].point.timestamp = ts;
                pPanel->keys[i].point.value.b1 = (bool) pData->value;
                Obj.value.dp_bool = pData->value;
                Obj.type = PROP_BOOL;
                Obj.dpid = (uint8_t) pPanel->keys[i].point.id;
                if (!pPanel->base.element.bMaster) {// 输入设备    执行输出设备
                    MbusBase_t **ppBase = (MbusBase_t **) pPanel->base.element.pvForeignKey;
#if 1
                    /* note:零时代码 */
                    if (pPanel->keys[i].point.value.b1 == false) {      // panel.close
                        Obj.value.dp_enum = 2;                          // tuya.close
                    } else if (pPanel->keys[i].point.value.b1 == true) {// panel.open
                        Obj.value.dp_enum = 0;                          // tuya.open
                    }
#endif
                    logDbg ("The knx panel %s is input, point=%d, value=%d, the mbus vent %s is output.", pPanel->base.element.pPrimaryKey, Obj.dpid, Obj.value.dp_value, (*ppBase)->element.pPrimaryKey);
                    ModbusDevWrite ((*ppBase)->element.pPrimaryKey, &Obj);
                } else {
                    if (g_bActive) {
                        ret = tuya_user_iot_report_obj_dp (pPanel->base.element.pPrimaryKey, &Obj, 1);
                        if (ret != OPRT_OK) {
                            logErr ("[knx panel]report cloud failure, primaryKey=%s, dpid=%d, type=%d, value=%d", pPanel->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                            ret = -1;
                        } else {
                            logDbg ("[knx panel]report cloud success, primaryKey=%s, dpid=%d, type=%d, value=%d", pPanel->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                        }
                        tuya_user_iot_misc_dev_hb_fresh (pPanel->base.element.pPrimaryKey);
                    }
                }
            }
        }
    }

    return ret;
}

int Knx_SmartPanelReport (KnxDataType_t *pData) {
    if (!pData) {
        logErr ("Knx_PanelReport invalid param");
        return -1;
    }

    listIter iter;
    listNode *pNode;
    TY_OBJ_DP_S Obj;
    struct timespec ts;
    int64_t diffMs = 0;
    int ret = -1;

    clock_gettime (CLOCK_MONOTONIC, &ts);

    /* KNX panel traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;
        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if ((pKnx->element.bMaster) && (tuya_user_iot_misc_dev_desc_get (pKnx->element.pPrimaryKey) == NULL)) break;

        if ((pKnx->element.primaryType < KNX_MISC_PANEL_ONE_KEY) || (pKnx->element.primaryType > KNX_MISC_PANEL_FOUR_KEY)) continue;

        KnxSmartPanel_t *pPanel = (KnxSmartPanel_t *) pNode->value;

        for (int i = 0; i < (int) pPanel->keySum; i++) {
            memset (&Obj, 0, sizeof (Obj));
            if (!pPanel->chl[i].key.pFeedback) continue;
            //            uint32_t maxLen = MAX (strlen (pPanel->chl[i].key.pFeedback->addr), strlen (pData->groupAddr));
            if (Knx_CompareGroup (pPanel->chl[i].key.pFeedback->addr, pData->groupAddr) == 0) {
                pPanel->chl[i].key.point.timestamp = ts;
                Obj.type = PROP_BOOL;
                Obj.dpid = (uint8_t) pPanel->chl[i].key.point.id;
                pPanel->chl[i].key.point.value.b1 = (bool) pData->value;
                Obj.value.dp_bool = pData->value;

                if (g_bActive) {
                    ret = tuya_user_iot_report_obj_dp (pPanel->base.element.pPrimaryKey, &Obj, 1);
                    if (ret != OPRT_OK) {
                        logErr ("[knx smart panel]report cloud failure, primaryKey=%s, dpid=%d, type=%d, value=%d", pPanel->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                        ret = -1;
                    } else {
                        logDbg ("[knx smart panel]report cloud success, primaryKey=%s, dpid=%d, type=%d, value=%d", pPanel->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                    }
                    tuya_user_iot_misc_dev_hb_fresh (pPanel->base.element.pPrimaryKey);
                }
            }
        }
    }

    return ret;
}

int Knx_DimmerReport (KnxDataType_t *pData) {
    if (!pData) {
        logErr ("Knx_DimmerReport invalid param");
        return -1;
    }

    listIter iter;
    listNode *pNode;
    TY_OBJ_DP_S Obj;
    int ret = -1;

    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;

        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if ((pKnx->element.bMaster) && (tuya_user_iot_misc_dev_desc_get (pKnx->element.pPrimaryKey) == NULL)) break;
        if ((pKnx->element.primaryType < KNX_DIMMER_ONE_KEY) || (pKnx->element.primaryType > KNX_DIMMER_FOUR_KEY)) continue;
        KnxDimmer_t *pDimmer = (KnxDimmer_t *) pNode->value;

        for (int i = 0; i < (int) pDimmer->keySum; i++) {
            memset (&Obj, 0, sizeof (Obj));
            if (pDimmer->chl[i].bright.pFeedback) {
                //                uint32_t maxLen = MAX (strlen (pDimmer->chl[i].bright.pFeedback->addr), strlen (pData->groupAddr));
                if (Knx_CompareGroup (pDimmer->chl[i].bright.pFeedback->addr, pData->groupAddr) == 0) {
                    Obj.type = PROP_VALUE;
                    Obj.dpid = (uint8_t) pDimmer->chl[i].bright.point.id;
                    pDimmer->chl[i].bright.point.value.u16 = (uint16_t) (pData->value * pDimmer->chl[i].bright.point.attr.attr.value.max.u16 / 255.0);
                    Obj.value.dp_value = pDimmer->chl[i].bright.point.value.u16;

                    if (pDimmer->base.element.attri == DEVICE_ATTRI_ACTUATOR) {
                        if (!g_bActive) continue;
                        ret = tuya_user_iot_report_obj_dp (pDimmer->base.element.pPrimaryKey, &Obj, 1);
                        if (ret != OPRT_OK) {
                            logErr ("Report knx dimmer state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pDimmer->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                            ret = -1;
                        } else {
                            logDbg ("Report knx dimmer state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pDimmer->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                        }
                        tuya_user_iot_misc_dev_hb_fresh (pDimmer->base.element.pPrimaryKey);
                        break;
                    }
                }
            }
            if (pDimmer->chl[i].key.pFeedback) {
                //                uint32_t maxLen = MAX (strlen (pDimmer->chl[i].key.pFeedback->addr), strlen (pData->groupAddr));
                if (Knx_CompareGroup (pDimmer->chl[i].key.pFeedback->addr, pData->groupAddr) == 0) {
                    Obj.type = PROP_BOOL;
                    Obj.dpid = (uint8_t) pDimmer->chl[i].key.point.id;
                    if (pData->bRejected) {
                        Obj.value.dp_bool = pData->value;
                        KnxGroupWrite (pDimmer->chl[i].bright.pFeedback->addr, &Obj);
                        logErr (COLOR_RED "The knx dimmer switch %s-%s is rejected." COLOR_CLEAR, pDimmer->base.element.pPrimaryKey, pDimmer->chl[i].bright.pFeedback->addr);
                        break;
                    }
                    pDimmer->chl[i].key.point.value.b1 = (bool) pData->value;
                    Obj.value.dp_bool = pData->value;

                    if (pDimmer->base.element.attri == DEVICE_ATTRI_ACTUATOR) {
                        if (!g_bActive) continue;
                        ret = tuya_user_iot_report_obj_dp (pDimmer->base.element.pPrimaryKey, &Obj, 1);
                        if (ret != OPRT_OK) {
                            logErr ("Report knx dimmer state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pDimmer->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                            ret = -1;
                        } else {
                            logDbg ("Report knx dimmer state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pDimmer->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                        }
                        tuya_user_iot_misc_dev_hb_fresh (pDimmer->base.element.pPrimaryKey);
                        break;
                    }
                }
            }
        }
    }

    return ret;
}

int Knx_CurtainReport (KnxDataType_t *pData) {
    if (!pData) {
        logErr ("Knx_CurtainReport invalid param");
        return -1;
    }

    listIter iter;
    listNode *pNode;
    TY_OBJ_DP_S Obj;
    int ret = -1;

    /* KNX curtain traversal */
    listRewind (pKnxDevList, &iter);
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) continue;

        KnxBase_t *pKnx = (KnxBase_t *) pNode->value;
        if (pKnx->element.primaryType != KNX_DEVICE_CURTAIN) continue;
        KnxCurtain_t *pCurtain = (KnxCurtain_t *) pKnx;
        Obj.type = PROP_ENUM;

        memset (&Obj, 0, sizeof (Obj));
        if (pCurtain->pOpen && pCurtain->pOpen->pGroup && (Knx_CompareGroup (pCurtain->pOpen->pGroup->addr, pData->groupAddr) == 0)) {
            if (pData->value == 1) {// open
                logDbg ("[Curtain] pCurtain->pOpen->pGroup->addr=%s", pCurtain->pOpen->pGroup->addr);
                pCurtain->pOpen->point.value.i32 = pData->value;
                Obj.value.dp_value = 0;// open
                Obj.dpid = (uint8_t) pCurtain->pOpen->point.id;
            }
        }
        if (pCurtain->pClose && pCurtain->pClose->pGroup && (Knx_CompareGroup (pCurtain->pClose->pGroup->addr, pData->groupAddr) == 0)) {
            if (pData->value == 0) {// close
                logDbg ("[Curtain] pCurtain->pClose->pGroup->addr=%s", pCurtain->pClose->pGroup->addr);
                pCurtain->pClose->point.value.i32 = pData->value;
                Obj.value.dp_value = 2;// close
                Obj.dpid = (uint8_t) pCurtain->pClose->point.id;
            }
        }
        if (pCurtain->pStop && pCurtain->pStop->pGroup && (Knx_CompareGroup (pCurtain->pStop->pGroup->addr, pData->groupAddr) == 0)) {
            logDbg ("[Curtain] pCurtain->pStop->pGroup->addr=%s", pCurtain->pStop->pGroup->addr);
            pCurtain->pStop->point.value.i32 = pData->value;
            Obj.value.dp_value = 1;// stop
            Obj.dpid = (uint8_t) pCurtain->pStop->point.id;
        }
        if (Obj.dpid == 0) {
            continue;
        }

        if (pCurtain->base.element.attri == DEVICE_ATTRI_CONTROLLER) {
            MbusBase_t **ppBase = (MbusBase_t **) pKnx->element.pvForeignKey;
            logDbg ("knx curtain(%s) status will be synchronized to mbus curtain(%s), dpid=%d, value=%d", pKnx->element.pPrimaryKey, (*ppBase)->element.pPrimaryKey, Obj.dpid, Obj.value.dp_value);
            ret = ModbusDevWrite ((*ppBase)->element.pPrimaryKey, &Obj);
        } else if (pCurtain->base.element.attri == DEVICE_ATTRI_ACTUATOR) {
            if (g_bActive && pKnx->element.bMaster) {
                ret = tuya_user_iot_report_obj_dp (pCurtain->base.element.pPrimaryKey, &Obj, 1);
                if (ret != OPRT_OK) {
                    logErr ("Report knx curtain state to cloud error, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pCurtain->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                    ret = -1;
                } else {
                    logDbg ("Report knx curtain state to cloud success, pPrimaryKey=%s, dpid=%d, type=%d, value=%d", pCurtain->base.element.pPrimaryKey, Obj.dpid, (int) Obj.type, Obj.value.dp_value);
                }
                tuya_user_iot_misc_dev_hb_fresh (pCurtain->base.element.pPrimaryKey);
            }
        }
    }

    return ret;
}

void KNX_BusMonitorEntry (void *param) {
    if (!param) {
        logErr ("KNX_BusMonitorEntry invalid param");
        return;
    }
    uint8_t knxBuf[255];
    fd_set read;
    queue_t *pQueue = (queue_t *) param;

    if (!g_bOnline) {
        sleep (5);
    }
    static int disconnect = 0;
    EIBConnection *connect = EIBSocketURL ("ip:localhost");
    if (!connect) {
        if (disconnect++ < 3) {
            logErr ("KNX_BusMonitorEntry EIBSocketURL failed!");
        }
        queue_put (threadDog[0].pQueue, (void *) &threadDog[0]);
        return;
    }
    disconnect = 0;

    if (EIBOpenVBusmonitorText (connect) == -1) {
        logErr ("KNX_BusMonitorEntry EIBOpenVBusmonitorText failed!");
        EIBClose (connect);
        queue_put (threadDog[0].pQueue, (void *) &threadDog[0]);
        return;
    }

    while (true) {
        FD_ZERO (&read);
        FD_SET (EIB_Poll_FD (connect), &read);
        if (select (EIB_Poll_FD (connect) + 1, &read, 0, 0, 0) == -1) {
            logErr ("KNX_BusMonitorEntry select failed!");
            break;
        }
        int len = EIB_Poll_Complete (connect);
        if (len == -1) {
            logErr ("KNX_BusMonitorEntry EIB_Poll_Complete failed!");
            break;
        }
        if (len == 0) {
            continue;
        }

        len = EIBGetBusmonitorPacket (connect, sizeof (knxBuf), knxBuf);
        if (len == -1) {
            logErr ("KNX_BusMonitorEntry EIBGetBusmonitorPacket failed!");
            break;
        }

        /* all message */
        logTrc ("raw --> %s", knxBuf);
        KnxDataType_t *pData = (KnxDataType_t *) calloc (1, sizeof (KnxDataType_t));
        if (!pData) {
            logErr ("KNX_BusMonitorEntry calloc KnxDataType_t failed!");
            break;
        }

        int ret = KNX_ParsePacket ((char *) knxBuf, pData);
        if (ret != 0) {
            if (ret == -1) {
                logErr ("KNX_BusMonitorEntry KNX_ParsePacket failed!");
            }
            free (pData);
            continue;
        }
        clock_gettime (CLOCK_MONOTONIC, &pData->timestamp);

        /* valid knx message */
        logDbg ("[KNX BusMonitor] phy:%s grp:%s value:0x%x", pData->hwAddr, pData->groupAddr, pData->value);
        bool bFound = kv_find (pData->groupAddr);
        if (bFound) {
            // logDbg (COLOR_RED"knx acd group%s be found"COLOR_CLEAR, pData->groupAddr);
            kv_update (pData->groupAddr, pData);
        } else {
            ret = queue_put (pQueue, (void *) pData);
            if (ret != 0) {
                logErr ("KNX_BusMonitorEntry queue_put failed! ret=%d", ret);
                free (pData);
                continue;
            }
        }
    }
    EIBClose (connect);

    logErr ("KNX BusMonitor Thread crash!");
    queue_put (threadDog[0].pQueue, (void *) &threadDog[0]);
}

void KNX_DevMonitorEntry (void *param) {
    if (!param) {
        logErr ("KNX_DevMonitorEntry invalid param");
        return;
    }
    queue_t *pQueue = (queue_t *) param;
    KnxDataType_t *pData = NULL;

    while (true) {
        int ret = queue_get_wait (pQueue, (void **) &pData);
        if (ret != 0) {
            continue;
        }
        if (!pData) {
            logErr ("KNX_DevMonitorEntry queue_get_wait get pData is NULL!");
            continue;
        }
        if (memcmp (pData->cmd, "Write", KNX_KEYWORD_WRITE_LEN) == 0) {
            logTrc ("[KNX DevMonitor] %s %s %s 0x%x", pData->cmd, pData->hwAddr, pData->groupAddr, pData->value);
        } else {
            // note: not print Response
            logTrc ("[KNX DevMonitor] %s %s %s 0x%x", pData->cmd, pData->hwAddr, pData->groupAddr, pData->value);
        }

        Knx_AcdReport (pData);
        Knx_VentReport (pData);
        Knx_HeatReport (pData);
        Knx_PanelReport (pData);
        Knx_SmartPanelReport (pData);
        Knx_DimmerReport (pData);
        Knx_CurtainReport (pData);

        free (pData);
        pData = NULL;
    }

    logErr ("KNX DevMonitor Thread crash!");
    queue_put (threadDog[1].pQueue, (void *) &threadDog[1]);
}

void KNX_DevWriteEntry (void *param) {
    if (!param) {
        logErr ("KNX_DevWriteEntry invalid param");
        return;
    }

    queue_t *pQueue = (queue_t *) param;
    KnxWriteType_t *pKnxData = NULL;

    while (true) {
        int ret = queue_get_wait (pQueue, (void **) &pKnxData);
        if (ret != 0) {
            continue;
        }
        if (!pKnxData) {
            logErr ("KNX_DevWriteEntry queue_get_wait get pData is NULL!");
            continue;
        }
        logTrc ("knx device %s write!", pKnxData->pPrimaryKey);

        ret = Knx_AirconditionCtrl (pKnxData);
        if (ret == 0) goto KNX_WRITE_DEV_FOUND;
        ret = Knx_VentilationCtrl (pKnxData);
        if (ret == 0) goto KNX_WRITE_DEV_FOUND;
        ret = Knx_FloorHeatingCtrl (pKnxData);
        if (ret == 0) goto KNX_WRITE_DEV_FOUND;
        ret = Knx_PanelCtrl (pKnxData);
        if (ret == 0) goto KNX_WRITE_DEV_FOUND;
        ret = Knx_SmartPanelCtrl (pKnxData);
        if (ret == 0) goto KNX_WRITE_DEV_FOUND;
        ret = Knx_DimmerCtrl (pKnxData);
        if (ret == 0) goto KNX_WRITE_DEV_FOUND;
        ret = Knx_CurtainCtrl (pKnxData);
        if (ret == 0) goto KNX_WRITE_DEV_FOUND;

    KNX_WRITE_DEV_FOUND:
        free (pKnxData);
        pKnxData = NULL;
    }

    logErr ("KNX_DevWriteEntry Thread crash!");
    queue_put (threadDog[2].pQueue, (void *) &threadDog[2]);
}

int KnxGroupWrite (const char *pDevId, const TY_OBJ_DP_S *pObj) {
    if (!pDevId || !pObj) {
        logErr ("KnxGroupWrite invalid param");
        return -1;
    }

    KnxWriteType_t *pKnxData = (KnxWriteType_t *) calloc (1, sizeof (KnxWriteType_t));
    if (!pKnxData) {
        logErr ("KnxGroupWrite calloc pKnxData error!");
        return -1;
    }
    strncpy (pKnxData->pPrimaryKey, pDevId, MIN (DEVICE_ID_LENGTH, strlen (pDevId)));
    memcpy (&pKnxData->obj, pObj, sizeof (TY_OBJ_DP_S));
    pKnxData->devId = strtol (pDevId + 12, NULL, 10);

    int8_t ret = queue_put (pWriteQueue, pKnxData);
    if (ret != 0) {
        logErr ("KnxGroupWrite queue_put fail! ret=%d", ret);
        return -1;
    }

    return 0;
}

int KnxGroupRead (const char *pGroup) {
    if (!pGroup) {
        return -1;
    }
    uint8_t buf[2] = {0, 0};

    EIBConnection *connect = EIBSocketURL ("ip:localhost");
    if (!connect) {
        logErr ("KnxGroupRead EIBSocketURL failed!");
        return -1;
    }
    eibaddr_t gAddr = readgaddr (pGroup);

    if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
        logErr ("KnxGroupRead EIBOpenT_Group failed!");
        return -1;
    }

    int len = EIBSendAPDU (connect, 2, buf);
    if (len == -1) {
        logErr ("KnxGroupRead EIBSendAPDU failed!");
        return -1;
    }
    EIBClose (connect);

    return 0;
}

void KNX_DevStateSync (void *param) {
    listIter iter;
    struct timespec tim, tim2;
    queue_t *pQueue = (queue_t *) param;

    listRewind (pKnxDevList, &iter);
    listNode *pNode;
    while ((pNode = listNext (&iter)) != NULL) {
        if (!pNode->value) {
            continue;
        }
        KnxBase_t *pBase = (KnxBase_t *) pNode->value;
        if (!pBase->element.bMaster) {
            tim.tv_sec = 0;
            tim.tv_nsec = (1000 * 1000 * 100);
            nanosleep (&tim, &tim2);
            continue;
        }

        if (pBase->element.primaryType == KNX_DEVICE_AIRCONDITION) {
            KnxAcd_t *pAir = (KnxAcd_t *) pNode->value;
            if (pAir->pSwitchStatus) {

                char *pGroup = (char *) calloc (1, strlen (pAir->pSwitchStatus->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pAir->pSwitchStatus->addr, strlen (pAir->pSwitchStatus->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pAir->pSpeedStatus) {
                char *pGroup = (char *) calloc (1, strlen (pAir->pSpeedStatus->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pAir->pSpeedStatus->addr, strlen (pAir->pSpeedStatus->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pAir->pModeStatus) {
                char *pGroup = (char *) calloc (1, strlen (pAir->pModeStatus->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pAir->pModeStatus->addr, strlen (pAir->pModeStatus->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pAir->pTempStatus) {
                char *pGroup = (char *) calloc (1, strlen (pAir->pTempStatus->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pAir->pTempStatus->addr, strlen (pAir->pTempStatus->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pAir->pEnvTemp) {
                char *pGroup = (char *) calloc (1, strlen (pAir->pEnvTemp->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pAir->pEnvTemp->addr, strlen (pAir->pEnvTemp->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pAir->pErrorCode) {
                char *pGroup = (char *) calloc (1, strlen (pAir->pErrorCode->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pAir->pErrorCode->addr, strlen (pAir->pErrorCode->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
        } else if (pBase->element.primaryType == KNX_DEVICE_VENTILATION) {
            KnxVent_t *pVent = (KnxVent_t *) pNode->value;
            if (pVent->pSwitchStatus) {
                char *pGroup = (char *) calloc (1, strlen (pVent->pSwitchStatus->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pVent->pSwitchStatus->addr, strlen (pVent->pSwitchStatus->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pVent->pSpeedStatus) {
                char *pGroup = (char *) calloc (1, strlen (pVent->pSpeedStatus->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pVent->pSpeedStatus->addr, strlen (pVent->pSpeedStatus->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pVent->pErrorCode) {
                char *pGroup = (char *) calloc (1, strlen (pVent->pErrorCode->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pVent->pErrorCode->addr, strlen (pVent->pErrorCode->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
        } else if (pBase->element.primaryType == KNX_DEVICE_FLOORHEATING) {
            KnxHeat_t *pFloor = (KnxHeat_t *) pNode->value;
            if (pFloor->pSwitchStatus) {
                char *pGroup = (char *) calloc (1, strlen (pFloor->pSwitchStatus->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pFloor->pSwitchStatus->addr, strlen (pFloor->pSwitchStatus->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pFloor->pTempStatus) {
                char *pGroup = (char *) calloc (1, strlen (pFloor->pTempStatus->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pFloor->pTempStatus->addr, strlen (pFloor->pTempStatus->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pFloor->pEnvTemp) {
                char *pGroup = (char *) calloc (1, strlen (pFloor->pEnvTemp->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pFloor->pEnvTemp->addr, strlen (pFloor->pEnvTemp->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pFloor->pErrorCode) {
                char *pGroup = (char *) calloc (1, strlen (pFloor->pErrorCode->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pFloor->pErrorCode->addr, strlen (pFloor->pErrorCode->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
        } else if ((pBase->element.primaryType >= KNX_PANEL_ONE_KEY) && (pBase->element.primaryType <= KNX_PANEL_EIGHT_KEY)) {
            KnxPanel_t *pPanel = (KnxPanel_t *) pNode->value;
            for (int i = 0; i < (int) pPanel->keySum; i++) {
                if (pPanel->keys[i].pFeedback && pPanel->keys[i].point.mode != POINT_ATTR_WRITE_ONLY) {
                    char *pGroup = (char *) calloc (1, strlen (pPanel->keys[i].pFeedback->addr) + 1);
                    if (pGroup) {
                        strncpy (pGroup, pPanel->keys[i].pFeedback->addr, strlen (pPanel->keys[i].pFeedback->addr));
                        int ret = queue_put (pQueue, (void *) pGroup);
                        if (ret != 0) {
                            free (pGroup);
                            logErr ("queue_put fail, ret=%d", ret);
                        }
                    }
                }
            }
        } else if ((pBase->element.primaryType >= KNX_MISC_PANEL_ONE_KEY) && (pBase->element.primaryType <= KNX_MISC_PANEL_FOUR_KEY)) {
            KnxSmartPanel_t *pPanel = (KnxSmartPanel_t *) pNode->value;
            for (int i = 0; i < (int) pPanel->keySum; i++) {
                if (pPanel->chl[i].key.pFeedback && pPanel->chl[i].key.point.mode != POINT_ATTR_WRITE_ONLY) {
                    char *pGroup = (char *) calloc (1, strlen (pPanel->chl[i].key.pFeedback->addr) + 1);
                    if (pGroup) {
                        strncpy (pGroup, pPanel->chl[i].key.pFeedback->addr, strlen (pPanel->chl[i].key.pFeedback->addr));
                        int ret = queue_put (pQueue, (void *) pGroup);
                        if (ret != 0) {
                            free (pGroup);
                            logErr ("queue_put fail, ret=%d", ret);
                        }
                    }
                }
            }
        } else if ((pBase->element.primaryType >= KNX_DIMMER_ONE_KEY) && (pBase->element.primaryType <= KNX_DIMMER_FOUR_KEY)) {
            KnxDimmer_t *pDimmer = (KnxDimmer_t *) pNode->value;
            for (int i = 0; i < (int) pDimmer->keySum; i++) {
                if (pDimmer->chl[i].key.pFeedback && (pDimmer->chl[i].key.point.mode != POINT_ATTR_WRITE_ONLY)) {
                    char *pGroup = (char *) calloc (1, strlen (pDimmer->chl[i].key.pFeedback->addr) + 1);
                    if (pGroup) {
                        strncpy (pGroup, pDimmer->chl[i].key.pFeedback->addr, strlen (pDimmer->chl[i].key.pFeedback->addr));
                        int ret = queue_put (pQueue, (void *) pGroup);
                        if (ret != 0) {
                            free (pGroup);
                            logErr ("queue_put fail, ret=%d", ret);
                        }
                    }
                }
                if (pDimmer->chl[i].bright.pFeedback && (pDimmer->chl[i].bright.point.mode != POINT_ATTR_WRITE_ONLY)) {
                    char *pGroup = (char *) calloc (1, strlen (pDimmer->chl[i].bright.pFeedback->addr) + 1);
                    if (pGroup) {
                        strncpy (pGroup, pDimmer->chl[i].bright.pFeedback->addr, strlen (pDimmer->chl[i].bright.pFeedback->addr));
                        int ret = queue_put (pQueue, (void *) pGroup);
                        if (ret != 0) {
                            free (pGroup);
                            logErr ("queue_put fail, ret=%d", ret);
                        }
                    }
                }
            }
        } else if (pBase->element.primaryType == KNX_DEVICE_CURTAIN) {
            KnxCurtain_t *pCurtain = (KnxCurtain_t *) pNode->value;
            if (pCurtain->pOpen->pGroup) {
                char *pGroup = (char *) calloc (1, strlen (pCurtain->pOpen->pGroup->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pCurtain->pOpen->pGroup->addr, strlen (pCurtain->pOpen->pGroup->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pCurtain->pClose->pGroup) {
                char *pGroup = (char *) calloc (1, strlen (pCurtain->pClose->pGroup->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pCurtain->pClose->pGroup->addr, strlen (pCurtain->pClose->pGroup->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
            if (pCurtain->pStop->pGroup) {
                char *pGroup = (char *) calloc (1, strlen (pCurtain->pStop->pGroup->addr) + 1);
                if (pGroup) {
                    strncpy (pGroup, pCurtain->pStop->pGroup->addr, strlen (pCurtain->pStop->pGroup->addr));
                    int ret = queue_put (pQueue, (void *) pGroup);
                    if (ret != 0) {
                        free (pGroup);
                        logErr ("queue_put fail, ret=%d", ret);
                    }
                }
            }
        }
    }
}

void KNX_DevHealth (void *param) {
    queue_t *pQueue = (queue_t *) param;
    char *pGroupAddr = NULL;
    struct timeval delay;

    while (true) {
        int ret = queue_get_wait (pQueue, (void **) &pGroupAddr);
        if (ret != 0) {
            continue;
        }
        if (!pGroupAddr) {
            logErr (COLOR_BLUE "KNX_DevHealth group null!" COLOR_CLEAR);
            continue;
        }

        KnxGroupRead (pGroupAddr);
        free (pGroupAddr);
        delay.tv_sec = 0;
        delay.tv_usec = 1000 * 50;
        select (0, NULL, NULL, NULL, &delay);
    }
    logErr ("KNX_DevHealth thread crashed");
}

void ThreadDogEntry (void *param) {
    if (!param) {
        logErr ("thread dog Entry invalid param!");
        return;
    }
    queue_t *pQueue = (queue_t *) param;
    ThreadDog_t *pDog = (ThreadDog_t *) param;

    while (true) {
        int ret = queue_get_wait (pQueue, (void **) &pDog);
        if (ret != 0) {
            //            zlog_error(pLogCat, "ThreadDogEntry queue_get_wait error!");
            continue;
        }
        if (!pDog) continue;

        //        logDbg ( "thread \"%s\" will be restart!", pDog->name);
        sleep (2);
        if (pDog->id == THREAD_NO_KNX_BUS_MONITOR) {
            ret = thpool_add_work (pDog->hThpool, KNX_BusMonitorEntry, pMonitorQueue);
        } else if (pDog->id == THREAD_NO_KNX_DEV_MONITOR) {
            ret = thpool_add_work (pDog->hThpool, KNX_DevMonitorEntry, pMonitorQueue);
        } else if (pDog->id == THREAD_NO_KNX_DEV_WRITE) {
            ret = thpool_add_work (pDog->hThpool, KNX_DevWriteEntry, pMonitorQueue);
        }
        if (ret != 0) {
            logErr (COLOR_RED "thpool_add_work fail!" COLOR_CLEAR);
        }
    }

    logErr ("thread dog crash!");
}

int Knx_RequestHB (const char *pDevId) {
    if (!pDevId) {
        logErr ("Knx_RequestHB: pDevId==NULL\n");
        return -1;
    }

    listSetMatchMethod (pKnxDevList, KnxDevIdMatch);
    listNode *pKnxNode = listSearchKey (pKnxDevList, (char *) pDevId);
    if (!pKnxNode) {
        logErr ("knx %s device not exist!", pDevId);
        return -1;
    }

    KnxBase_t *pKnx = (KnxBase_t *) pKnxNode->value;
    if (pKnx->bOnlineCheck == false) {
        tuya_user_iot_misc_dev_hb_fresh (pKnx->element.pPrimaryKey);
        logTrc ("knx %s device not support online check!", pKnx->element.pPrimaryKey);
    }

    if (pKnx->hbIgnore++ < 5) {
        //        logTrc ( "knx device %s ignore heartbeat get!", pDevId);
        return 0;
    }
    pKnx->hbIgnore = 0;
    logTrc ("KNX device %s get heartbeat!", (char *) pDevId);

    if (pKnx->element.primaryType == KNX_DEVICE_AIRCONDITION) {
        KnxAcd_t *pAir = (KnxAcd_t *) pKnx;
        if (pAir->pSwitchStatus) {
            char *pGroup = (char *) calloc (1, strlen (pAir->pSwitchStatus->addr) + 1);
            if (pGroup) {
                strncpy (pGroup, pAir->pSwitchStatus->addr, strlen (pAir->pSwitchStatus->addr));
                int ret = queue_put (pHeartbeatQueue, (void *) pGroup);
                if (ret != 0) {
                    free (pGroup);
                    logErr ("queue_put fail, ret=%d", ret);
                }
            }
        }
    } else if (pKnx->element.primaryType == KNX_DEVICE_VENTILATION) {
        KnxVent_t *pVent = (KnxVent_t *) pKnx;
        if (pVent->pSwitchStatus) {
            char *pGroup = (char *) calloc (1, strlen (pVent->pSwitchStatus->addr) + 1);
            if (pGroup) {
                strncpy (pGroup, pVent->pSwitchStatus->addr, strlen (pVent->pSwitchStatus->addr));
                int ret = queue_put (pHeartbeatQueue, (void *) pGroup);
                if (ret != 0) {
                    free (pGroup);
                    logErr ("queue_put fail, ret=%d", ret);
                }
            }
        }
    } else if (pKnx->element.primaryType == KNX_DEVICE_FLOORHEATING) {
        KnxHeat_t *pFloor = (KnxHeat_t *) pKnx;
        if (pFloor->pSwitchStatus) {
            char *pGroup = (char *) calloc (1, strlen (pFloor->pSwitchStatus->addr) + 1);
            if (pGroup) {
                strncpy (pGroup, pFloor->pSwitchStatus->addr, strlen (pFloor->pSwitchStatus->addr));
                int ret = queue_put (pHeartbeatQueue, (void *) pGroup);
                if (ret != 0) {
                    free (pGroup);
                    logErr ("queue_put fail, ret=%d", ret);
                }
            }
        }
    } else if ((pKnx->element.primaryType >= KNX_PANEL_ONE_KEY) && (pKnx->element.primaryType <= KNX_PANEL_EIGHT_KEY)) {
        KnxPanel_t *pPanel = (KnxPanel_t *) pKnx;
        if (pPanel->keys[0].pFeedback) {
            char *pGroup = (char *) calloc (1, strlen (pPanel->keys[0].pFeedback->addr) + 1);
            if (pGroup) {
                strncpy (pGroup, pPanel->keys[0].pFeedback->addr, strlen (pPanel->keys[0].pFeedback->addr));
                int ret = queue_put (pHeartbeatQueue, (void *) pGroup);
                if (ret != 0) {
                    free (pGroup);
                    logErr ("queue_put fail, ret=%d", ret);
                }
            }
        }
    } else if ((pKnx->element.primaryType >= KNX_MISC_PANEL_ONE_KEY) && (pKnx->element.primaryType <= KNX_MISC_PANEL_FOUR_KEY)) {
        KnxSmartPanel_t *pPanel = (KnxSmartPanel_t *) pKnx;
        if (pPanel->chl[0].key.pFeedback) {
            char *pGroup = (char *) calloc (1, strlen (pPanel->chl[0].key.pFeedback->addr) + 1);
            if (pGroup) {
                strncpy (pGroup, pPanel->chl[0].key.pFeedback->addr, strlen (pPanel->chl[0].key.pFeedback->addr));
                int ret = queue_put (pHeartbeatQueue, (void *) pGroup);
                if (ret != 0) {
                    free (pGroup);
                    logErr ("queue_put fail, ret=%d", ret);
                }
            }
        }
    } else if ((pKnx->element.primaryType >= KNX_DIMMER_ONE_KEY) && (pKnx->element.primaryType <= KNX_DIMMER_FOUR_KEY)) {
        KnxDimmer_t *pDimmer = (KnxDimmer_t *) pKnx;
        if (pDimmer->chl[0].bright.pFeedback) {
            char *pGroup = (char *) calloc (1, strlen (pDimmer->chl[0].bright.pFeedback->addr) + 1);
            if (pGroup) {
                strncpy (pGroup, pDimmer->chl[0].bright.pFeedback->addr, strlen (pDimmer->chl[0].bright.pFeedback->addr));
                int ret = queue_put (pHeartbeatQueue, (void *) pGroup);
                if (ret != 0) {
                    free (pGroup);
                    logErr ("queue_put fail, ret=%d", ret);
                }
            }
        }
    } else if (pKnx->element.primaryType == KNX_DEVICE_CURTAIN) {
        KnxCurtain_t *pCurtain = (KnxCurtain_t *) pKnx;
        if (pCurtain->pOpen->pGroup) {
            char *pGroup = (char *) calloc (1, strlen (pCurtain->pOpen->pGroup->addr) + 1);
            if (pGroup) {
                strncpy (pGroup, pCurtain->pOpen->pGroup->addr, strlen (pCurtain->pOpen->pGroup->addr));
                int ret = queue_put (pHeartbeatQueue, (void *) pGroup);
                if (ret != 0) {
                    free (pGroup);
                    logErr ("queue_put fail, ret=%d", ret);
                }
            }
        }
    } else {
        logErr ("Heartbeat get from the device of unknown type");
    }

    return 0;
}

void Knx_SyncDevStatus (threadpool hKnxThpool) {
    int ret = thpool_add_work (hKnxThpool, KNX_DevStateSync, pHeartbeatQueue);
    if (ret != 0) {
        logErr (COLOR_RED "thpool_add_work fail!" COLOR_CLEAR);
    }
}

int Knx_Write1Byte (char *pGroupAddr, uint8_t value) {
    uint8_t knxBuf[10] = {0, 0x80};
    logTrc ("Knx_Write1Byte %s 0x%x", pGroupAddr, value);

    eibaddr_t gAddr = readgaddr (pGroupAddr);
    int dataLen = readBlock (knxBuf + 2, value);

    EIBConnection *connect = EIBSocketURL ("ip:localhost");
    if (!connect) {
        logErr ("EIBSocketURL failed!");
        return -1;
    }
    if (EIBOpenT_Group (connect, gAddr, 1) == -1) {
        logErr ("EIBOpenT_Group failed!");
        EIBClose (connect);
        return -2;
    }
    int ret = EIBSendAPDU (connect, dataLen + 2, knxBuf);
    EIBClose (connect);
    if (ret == -1) {
        logErr ("EIBSendAPDU failed!");
        return -3;
    }

    return 0;
}

int KnxInit (threadpool hKnxThpool) {
    logDbg ("knx module init!");

    pDogQueue = queue_create_limited (10);
    if (!pDogQueue) {
        logErr ("create pDogQueue fail!");
        return -1;
    }

    pMonitorQueue = queue_create_limited (200);
    if (!pMonitorQueue) {
        logErr ("create pMonitorQueue fail!");
        return -1;
    }
    pWriteQueue = queue_create_limited (200);
    if (!pWriteQueue) {
        logErr ("create pWriteQueue fail!");
        return -1;
    }

    pHeartbeatQueue = queue_create_limited (200);
    if (!pHeartbeatQueue) {
        logErr ("create pHeartbeatQueue fail!");
        return -1;
    }

    threadDog[0].id = THREAD_NO_KNX_BUS_MONITOR;
    strcpy (threadDog[0].name, "KNX_BusMonitorEntry");
    threadDog[0].pQueue = pDogQueue;
    threadDog[0].hThpool = hKnxThpool;

    threadDog[1].id = THREAD_NO_KNX_DEV_MONITOR;
    strcpy (threadDog[1].name, "KNX_DevMonitorEntry");
    threadDog[1].pQueue = pDogQueue;
    threadDog[0].hThpool = hKnxThpool;

    threadDog[2].id = THREAD_NO_KNX_DEV_WRITE;
    strcpy (threadDog[2].name, "KNX_DevWriteEntry");
    threadDog[2].pQueue = pDogQueue;
    threadDog[0].hThpool = hKnxThpool;

    thpool_add_work (hKnxThpool, ThreadDogEntry, pDogQueue);
    thpool_add_work (hKnxThpool, KNX_BusMonitorEntry, pMonitorQueue);
    thpool_add_work (hKnxThpool, KNX_DevMonitorEntry, pMonitorQueue);
    thpool_add_work (hKnxThpool, KNX_DevDebounce, pMonitorQueue);
    thpool_add_work (hKnxThpool, KNX_DevWriteEntry, pWriteQueue);
    //    thpool_add_work(hKnxThpool, KNX_DevStateSync, pHeartbeatQueue);
    thpool_add_work (hKnxThpool, KNX_DevHealth, pHeartbeatQueue);
    atexit (KnxDestroy);

    return 0;
}

void KnxDestroy (void) {
    logDbg ("Knx module destroy!");
    queue_destroy (pMonitorQueue);
    queue_destroy (pWriteQueue);
    queue_destroy (pDogQueue);
    queue_destroy (pHeartbeatQueue);
    // todo: destroy thread pool and other resource
}
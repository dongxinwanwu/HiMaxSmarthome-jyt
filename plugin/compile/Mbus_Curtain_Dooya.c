#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include "zlog.h"
#include <netinet/in.h>

/*
 * 非Modbus协议，特殊处理
 * 杜亚官方配置软件，ID_L==ID_H
 * */

/*
PID="aleeszi3hjcch9ae"

DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关             control          rw         enum        0 open，1 stop， 2 close
12    故障             fault            ro         Fault
*/

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

typedef enum {
    CONTROL= 0,
    MAX_IDX
} Idx_e;


/* 调度方做内存释放 */
Msg_t *buildExt (uint16_t devId, uint16_t regAddr, uint16_t groupAddr, ty_obj_dp_s *pObj) {
    (void) regAddr;
    (void) groupAddr;
    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ( COLOR_RED "[mbus Plugin] Dooya curtain alloc fail!" COLOR_CLEAR);
        return NULL;
    }

    if (!pObj) {
        pMsg->len = 8;
        pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
        if (!pMsg->pBuf) {
            free (pMsg);
            logErr ( COLOR_RED "[mbus Plugin] Dooya curtain alloc fail!" COLOR_CLEAR);
            return NULL;
        }

        *((uint8_t *) (pMsg->pBuf + 0)) = 0x55;
        *((uint16_t *) (pMsg->pBuf + 1)) = htons (devId);
        *((uint8_t *) (pMsg->pBuf + 3)) = 0x1;
        *((uint8_t *) (pMsg->pBuf + 4)) = 0x5;
        *((uint8_t *) (pMsg->pBuf + 5)) = 0x1;
        uint16_t tmp = crc16 (pMsg->pBuf, pMsg->len - 2);
        *((uint16_t *) (pMsg->pBuf + pMsg->len - 2)) = htons (tmp);

        return pMsg;
    }

    pMsg->len = 7;
    pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "[mbus Plugin] Dooya curtain alloc fail!" COLOR_CLEAR);
        return NULL;
    }
    /* 01 10 00 00 01 00 02 00 00 01 00 */
    *((uint8_t *) (pMsg->pBuf + 0)) = 0x55;
    *((uint16_t *) (pMsg->pBuf + 1)) = htons (devId);
    *((uint8_t *) (pMsg->pBuf + 3)) = 0x3;

    switch (pObj->dpid) {
        case 1:
            /*0 open，1 stop， 2 close*/
            if (pObj->value.dp_value == 0) {
                *((uint8_t *) (pMsg->pBuf + 4)) = 1;
            } else if (pObj->value.dp_value == 1) {
                *((uint8_t *) (pMsg->pBuf + 4)) = 3;
            } else if (pObj->value.dp_value == 2) {
                *((uint8_t *) (pMsg->pBuf + 4)) = 2;
            }

            break;
        default:
            logErr ( COLOR_RED "[mbus Plugin] localPoint(%d) not be supported!" COLOR_CLEAR, pObj->dpid);
            break;
    }
    uint16_t tmp = crc16 ((uint8_t *) pMsg->pBuf, pMsg->len - 2);
    *((uint16_t *) (pMsg->pBuf + pMsg->len - 2)) = htons (tmp);

    return pMsg;
}

/**
 * @brief
 * @note
 * @param  modbusId:
 * @param  *pBuf:
 * @param  len:
 * @retval 返回modbus数据节点个数
 */
MbusObj_t **parseExt (uint16_t devId, uint16_t regAddr, const uint8_t *pBuf, uint16_t len) {
    (void) regAddr;

    if (!pBuf || (len < 7)) {
        logErr ( COLOR_RED "[mbus Plugin] Dooya curtain parse invalid parameter! len=%d" COLOR_CLEAR, len);
        return NULL;
    }

    uint8_t *pos = memmatch ((uint8_t *) pBuf, devId, len);
    if (!pos || (len + pBuf - pos < 7)) {
        logErr ( "mbus plugin report: invalid command");
        return NULL;
    }

    MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX + 1);
    if (!ppMbusObj) {
        logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }
    ppMbusObj[MAX_IDX] = NULL;

    if (pos[2] == 1) {
        ppMbusObj[CONTROL]->value = pos[4]; /* 0 stop，1 open， 2 close, 3 setting */
        ppMbusObj[CONTROL]->localPoint = CURTAIN_SWITCH;

        if (ppMbusObj[CONTROL]->value == 0) {
            logTrc ( "Dooya curtain stop status");
        } else if (ppMbusObj[CONTROL]->value == 1) {
            logTrc ( "Dooya curtain open status");
        } else if (ppMbusObj[CONTROL]->value == 2) {
            logTrc ( "Dooya curtain close status");
        } else if (ppMbusObj[CONTROL]->value == 3) {
            logTrc ( "Dooya curtain setting status");
        }
    } else if (pos[2] == 3) {
        ppMbusObj[CONTROL]->value = pos[3]; /* 0 stop，1 open， 2 close */
        ppMbusObj[CONTROL]->localPoint = CURTAIN_SWITCH;
        if (ppMbusObj[CONTROL]->value == 0) {
            logTrc ( "Dooya curtain stop cmd ack");
        } else if (ppMbusObj[CONTROL]->value == 1) {
            logTrc ( "Dooya curtain open cmd ack");
        } else if (ppMbusObj[CONTROL]->value == 2) {
            logTrc ( "Dooya curtain close cmd ack");
        }
    }

    return ppMbusObj;
}

const MbusIf Mbus_Curtain_Dooya = {
    .buildExt = buildExt,
    .parseExt = parseExt};

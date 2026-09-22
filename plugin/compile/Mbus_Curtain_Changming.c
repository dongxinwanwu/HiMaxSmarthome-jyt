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
PID="vwiyrxjaq03y9xjc"

DP    功能点名称        标识符                  传输类型    数据类型    功能点属性
1     开关             control                rw         enum        0 open，1 stop， 2 close
12    故障             fault                  ro         Fault
101   功能反转          bypass_function        wr         bool
199   点位上报          device_obj_conf_id     ro         string
*/

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

#define MESSAGE_MAX_LEN 9
#define MESSAGE_MIN_LEN 7

typedef enum {
    CONTROL = 0,
    MAX_IDX
} Idx_e;

/* 调度方做内存释放 */
Msg_t *buildExt (uint16_t devId, UNUSED uint16_t regAddr, UNUSED uint16_t groupAddr, ty_obj_dp_s *pObj) {
    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ( COLOR_RED "[mbus Changming Curtain Plugin]  alloc fail!" COLOR_CLEAR);
        return NULL;
    }

    if (!pObj) {// 查询位置
        pMsg->len = 8;
        pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
        if (!pMsg->pBuf) {
            free (pMsg);
            logErr ( COLOR_RED "[mbus Changming Curtain Plugin] alloc fail!" COLOR_CLEAR);
            return NULL;
        }

        *((uint8_t *) (pMsg->pBuf + 0)) = 0xAA;
        *((uint8_t *) (pMsg->pBuf + 1)) = devId;
        *((uint8_t *) (pMsg->pBuf + 2)) = devId;
        *((uint8_t *) (pMsg->pBuf + 3)) = 0x1;
        *((uint8_t *) (pMsg->pBuf + 4)) = 0x2;
        *((uint8_t *) (pMsg->pBuf + 5)) = 0x1;
        uint16_t tmp = crc16 (pMsg->pBuf, pMsg->len - 2);
        *((uint16_t *) (pMsg->pBuf + pMsg->len - 2)) = htons (tmp);

        return pMsg;
    }

    pMsg->len = 8;
    pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "[mbus Changming Curtain Plugin] alloc fail!" COLOR_CLEAR);
        return NULL;
    }

    *((uint8_t *) (pMsg->pBuf + 0)) = 0xAA;
    *((uint8_t *) (pMsg->pBuf + 1)) = devId;
    *((uint8_t *) (pMsg->pBuf + 2)) = devId;

    switch (pObj->dpid) {
        case 1:// control
            /* TUYA: 0 open，1 stop， 2 close*/
            /* Changming: 1-open, 2-close, 3-stop */
            pMsg->len = 7;
            *((uint8_t *) (pMsg->pBuf + 3)) = 0x3;

            if (pObj->value.dp_value == 0) {
                *((uint8_t *) (pMsg->pBuf + 4)) = 1;
            } else if (pObj->value.dp_value == 1) {
                *((uint8_t *) (pMsg->pBuf + 4)) = 3;
            } else if (pObj->value.dp_value == 2) {
                *((uint8_t *) (pMsg->pBuf + 4)) = 2;
            }
            logDbg (  "Curtain%d plugin ctrl=%d" , devId, pObj->value.dp_value);
            break;
        case 101:// 换向
            *((uint8_t *) (pMsg->pBuf + 3)) = 0x1;
            *((uint8_t *) (pMsg->pBuf + 4)) = 0x3;
            *((uint8_t *) (pMsg->pBuf + 5)) = 0x1;
            logDbg (  "Curtain%d plugin set direction" , devId);

            break;
        default:
            logErr ( COLOR_RED "[mbus Changming Curtain Plugin] localPoint(%d) not be supported!" COLOR_CLEAR, pObj->dpid);
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
MbusObj_t **parseExt (uint16_t devId, UNUSED uint16_t regAddr, const uint8_t *pBuf, uint16_t len) {
    if (!pBuf || (len < MESSAGE_MIN_LEN)) {
        logErr ( COLOR_RED "[mbus Changming Curtain Plugin] parse invalid parameter!pBuf=%p, len=%d" COLOR_CLEAR, pBuf, len);
        return NULL;
    }
    uint16_t devv = devId;
    devv = devv << 8;
    devv = devv | devId;

    uint8_t *pos = memmatch ((uint8_t *) pBuf, devv, len);
    if (!pos || (len + pBuf - pos < MESSAGE_MIN_LEN - 1)) {
        logErr ( "[mbus Changming Curtain Plugin] no match key(0x%04x) or too few len(%d)!", devv, len + pBuf - pos);
        return NULL;
    }

    MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX + 1);
    if (!ppMbusObj) {
        logErr ( "[mbus Changming Curtain Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }
    ppMbusObj[MAX_IDX] = NULL;

    if (pos[2] == 1) { // 读电机位置
        /* TUYA: 0 open，1 stop， 2 close */
        ppMbusObj[CONTROL]->localPoint = CURTAIN_SWITCH;
        switch (pos[4]) {
            case 0:
                ppMbusObj[CONTROL]->value = 2;
                logDbg (  "[mbus Changming Curtain Plugin] close status" );
                break;
            case 100:
                ppMbusObj[CONTROL]->value = 0;
                logDbg (  "[mbus Changming Curtain Plugin] open status" );
                break;
            default:
                break;
        }
    } else if (pos[2] == 3) {                                                    // 控制电机状态
        /* TUYA: 0 open，1 stop， 2 close*/
        /* Changming: 1-open, 2-close, 3-stop */
        ppMbusObj[CONTROL]->value = pos[3];
        ppMbusObj[CONTROL]->localPoint = CURTAIN_SWITCH;

        switch (pos[3]) {
            case 1:
                ppMbusObj[CONTROL]->value = 0;
                logDbg (  "[mbus Changming Curtain Plugin] open cmd ack" );
                break;
            case 2:
                ppMbusObj[CONTROL]->value = 2;
                logDbg (  "[mbus Changming Curtain Plugin] close cmd ack" );
                break;
            case 3:
                ppMbusObj[CONTROL]->value = 1;
                logDbg (  "[mbus Changming Curtain Plugin] stop cmd ack" );
                break;
            default:
                break;
        }
    }

    return ppMbusObj;
}

const MbusIf Mbus_Curtain_Changming = {
    .buildExt = buildExt,
    .parseExt = parseExt};

#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include "zlog.h"
#include <netinet/in.h>

/*
 * 奥科电机
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

#define MESSAGE_MAX_LEN 9
#define MESSAGE_MIN_LEN 10

typedef enum {
    CONTROL = 0,
    MAX_IDX
} Idx_e;

/* 调度方做内存释放 */
Msg_t *build (uint16_t devId, uint16_t regAddr, uint16_t groupAddr, ty_obj_dp_s *pObj) {
    (void) regAddr;
    (void) groupAddr;
    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ( COLOR_RED "[mbus aok Curtain Plugin]  alloc fail!" COLOR_CLEAR);
        return NULL;
    }

    pMsg->len = 7;
    pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "[mbus aok Curtain Plugin] alloc fail!" COLOR_CLEAR);
        return NULL;
    }

    *((uint8_t *) (pMsg->pBuf + 0)) = 0x9A;
    *((uint8_t *) (pMsg->pBuf + 1)) = devId;
    *((uint8_t *) (pMsg->pBuf + 2)) = 0x80;
    *((uint8_t *) (pMsg->pBuf + 3)) = 0x00;

    if (!pObj) {// 查询位置
        *((uint8_t *) (pMsg->pBuf + 4)) = 0xCC;
        *((uint8_t *) (pMsg->pBuf + 5)) = 0x00;
        *((uint8_t *) (pMsg->pBuf + 6)) = BCC8 (pMsg->pBuf + 1, 5);

        return pMsg;
    }

    /* 9a 09 80 00 0a dd 5e */
    switch (pObj->dpid) {
        case 1:// control
            /* TUYA: 0 open，1 stop， 2 close*/

            *((uint8_t *) (pMsg->pBuf + 4)) = 0x0A;
            if (pObj->value.dp_value == 0) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 0xDD;
            } else if (pObj->value.dp_value == 1) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 0xCC;
            } else if (pObj->value.dp_value == 2) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 0xEE;
            }
            logDbg ( "Curtain%d plugin ctrl=%d", devId, pObj->value.dp_value);

            break;
        case 2:// 取反
            *((uint8_t *) (pMsg->pBuf + 4)) = 0x0A;
            *((uint8_t *) (pMsg->pBuf + 5)) = 0x02;
            break;
        default:
            logErr ( COLOR_RED "[mbus aok Curtain Plugin] localPoint(%d) not be supported!" COLOR_CLEAR, pObj->dpid);
            break;
    }
    *((uint8_t *) (pMsg->pBuf + 6)) = BCC8 (pMsg->pBuf + 1, 5);

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
MbusObj_t **parse (uint16_t devId, uint16_t regAddr, const uint8_t *pBuf, uint16_t len) {
    (void) regAddr;

    if (!pBuf || (len < MESSAGE_MIN_LEN)) {
        logErr ( COLOR_RED "[mbus aok Curtain Plugin] parse invalid parameter!pBuf=%p, len=%d" COLOR_CLEAR, pBuf, len);
        return NULL;
    }
    uint16_t devv = 0xD800 + devId;
    uint8_t *pos = memmatch ((uint8_t *) pBuf, devv, len);
    if (!pos || (len + pBuf - pos < MESSAGE_MIN_LEN - 1)) {
        logErr ( "[mbus aok Curtain Plugin] no match key(0x%04x) or too few len(%d)!", devv, len + pBuf - pos);
        return NULL;
    }

    MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX);
    if (!ppMbusObj) {
        logErr ( "[mbus aok Curtain Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }

    /* TUYA: 0 open，1 stop， 2 close */
    ppMbusObj[CONTROL]->localPoint = CURTAIN_SWITCH;
    switch (pos[6]) {
        case 0:
            ppMbusObj[CONTROL]->value = 0;
            logDbg ( "[mbus aok Curtain Plugin] close status");
            break;
        case 100:
            ppMbusObj[CONTROL]->value = 2;
            logDbg ( "[mbus aok Curtain Plugin] open status");
            break;
        default:
            break;
    }

    return ppMbusObj;
}

const MbusIf Mbus_Curtain_AOK = {
    .buildExt = build,
    .parseExt = parse};

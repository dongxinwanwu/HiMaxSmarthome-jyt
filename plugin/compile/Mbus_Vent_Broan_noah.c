#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include "zlog.h"
#include <netinet/in.h>
#include "../../source/common.h"

/* 非Modbus协议，特殊处理 */

/*
PID="ityiaeb3uzapmahe"

DP    功能点名称        标识符            传输类型    数据类型    功能点属性
39    开关             switch          rw         bool
28/101   风速设置          wind            rw         enum       枚举值: off, low, middle, high
*/

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

typedef enum {
    SWITCH = 0,
    SPEED = 1,
    MAX_IDX
} Idx_e;

/* 调度方做内存释放 */
// aa 5a 00 00 00 00 00 00 00 00 04
// 01 03 00 00 00 05 85 C9
Msg_t *build (uint8_t devId, UNUSED uint16_t regAddr, UNUSED uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj) {
    if (!pvMbusNode) {
        logErr ( COLOR_RED "[mbus Plugin] Broan-noah-noah Vent control invalid parameter!" COLOR_CLEAR);
        return NULL;
    }
    MbusVent_t *pVent = (MbusVent_t *) pvMbusNode;
    if (!pVent->pSwitch || !pVent->pSpeed) {
        logErr ( COLOR_RED "[mbus Plugin] Broan-noah-noah Vent control invalid parameter!" COLOR_CLEAR);
        return NULL;
    }

    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ( COLOR_RED "[mbus Plugin] Broan-noah-noah Vent control alloc fail!" COLOR_CLEAR);
        return NULL;
    }
    pMsg->len = 11;
    pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "[mbus Plugin] Broan-noah-noah Vent control alloc fail!" COLOR_CLEAR);
        return NULL;
    }
    *((uint8_t *) (pMsg->pBuf + 0)) = 0xAA;
    *((uint8_t *) (pMsg->pBuf + 1)) = 0x5A;
    *((uint8_t *) (pMsg->pBuf + 2)) = devId;

    if (!pObj) { /* read */
        *((uint8_t *) (pMsg->pBuf + 10)) = cs (pMsg->pBuf, pMsg->len - 1);
        return pMsg;
    }

    *((uint8_t *) (pMsg->pBuf + 3)) = 1;
    switch (pObj->dpid) { /* write */
        case 39:          /* switch */
            if (pObj->value.dp_bool) {
                *((uint8_t *) (pMsg->pBuf + 4)) = 1;
                *((uint8_t *) (pMsg->pBuf + 5)) = 1;
            }

            break;
        case 28: /* speed */
        case 101:
            // Modbus枚举值: 0关闭、1低风、2中风、3高风、4、5
            // 枚举值: off, low, middle, high
            *((uint8_t *) (pMsg->pBuf + 4)) = 1;

            if (pObj->value.dp_enum == 1) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 1;
            } else if (pObj->value.dp_enum == 2) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 3;
            } else if (pObj->value.dp_enum == 3) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 5;
            }

            break;
        default:
            logErr ( COLOR_RED "[mbus Plugin] localPoint(%d) not be supported!" COLOR_CLEAR, pObj->dpid);
            break;
    }
    *((uint8_t *) (pMsg->pBuf + 10)) = cs (pMsg->pBuf, pMsg->len - 1);

    return pMsg;
}

// AA 5A 00 01 00 00 00 00 00 00 05
// AA A5 00 5A A0 01 02 1D 05 80 21 00 00 0F 00 63 03 BB 04 0A 00 06 53
/**
 * @brief
 * @note
 * @param  modbusId:
 * @param  *pBuf:
 * @param  len:
 * @retval 返回modbus数据节点个数
 */
MbusObj_t **parse (UNUSED uint8_t devId, UNUSED uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    if (!pBuf || (len < 23)) {
        logErr ( COLOR_RED "[mbus Plugin] Broan-noah Vent report invalid parameter!" COLOR_CLEAR);
        return NULL;
    }
#if 0
    MbusVent_t *pVent = container_of (pvMbusNode, MbusVent_t, MbusBase_t);
#else
    MbusVent_t **ppVent = (MbusVent_t **) &pvMbusNode;
#endif
    if (!(*ppVent)->pSwitch || !(*ppVent)->pSpeed) {
        logErr ( COLOR_RED "[mbus Plugin] Broan-noah Vent control invalid parameter!" COLOR_CLEAR);
        return NULL;
    }

    uint16_t key = 0xAAA5;
    uint8_t *pos = memmatch ((uint8_t *) pBuf, key, len);
    if (!pos || (len + pBuf - pos < 22)) {
        logErr ( "mbus plugin report: invalid command");
        return NULL;
    }

    MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX + 1);
    if (!ppMbusObj) {
        logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }
    ppMbusObj[MAX_IDX] = NULL;

    ppMbusObj[SWITCH]->value = (pos[6] == 0 ? 0 : 1); /* 开关机：1->开机，0->关机 */
    ppMbusObj[SWITCH]->localPoint = VENT_SWITCH;

    /* todo: 需要485线验证 */
    if (pos[6] <= 2) {
        ppMbusObj[SPEED]->value = 1;
    } else if (pos[6] == 3) {
        ppMbusObj[SPEED]->value = 2;
    } else {
        ppMbusObj[SPEED]->value = 3;
    }
    ppMbusObj[SPEED]->localPoint = VENT_SPEED;

    return ppMbusObj;
}

const MbusIf Mbus_Vent_Broan_noah = {
    .build = build,
    .parse = parse};

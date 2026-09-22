#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include "zlog.h"
#include <netinet/in.h>

/*
 * 除湿新风一体机线控器  D-Touch
 *
 */

/*
 * PID="yr7tk84mvjknmnzr"
 * DP ID 功能点名称 标识符 数据传输类型 数据类型 功能点属性
 * 28 风速	supply_fan_speed 可下发可上报（rw） enum 枚举值: low, high
 * 39 开关	switch 可下发可上报（rw） bool
 * 101 模式 mode 可下发可上报（rw） enum 枚举值: fresh_air, dehumidifier
*/

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

typedef enum {
    SWITCH = 0,
    SPEED = 1,
    MODE = 2,
    MAX_IDX
} Idx_e;

#define READ_CODE 3
#define WRITE_CODE 6

/* 调度方做内存释放 */
Msg_t *build (uint8_t devId, UNUSED uint16_t regAddr, UNUSED uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj) {
    if (!pvMbusNode) {
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent control invalid parameter!" COLOR_CLEAR);
        return NULL;
    }
    MbusVent_t *pVent = (MbusVent_t *) pvMbusNode;
    if (!pVent->pSwitch || !pVent->pSpeed) {
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent control invalid parameter!" COLOR_CLEAR);
        return NULL;
    }

    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent control alloc fail!" COLOR_CLEAR);
        return NULL;
    }

    pMsg->len = 8;
    pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent control alloc fail!" COLOR_CLEAR);
        return NULL;
    }
    *((uint8_t *) (pMsg->pBuf + 0)) = devId;

    if (!pObj) { /* read */
        *((uint8_t *) (pMsg->pBuf + 1)) = READ_CODE;
        *((uint8_t *) (pMsg->pBuf + 3)) = 1;
        *((uint8_t *) (pMsg->pBuf + 5)) = 0x1f;
        uint16_t tmp = crc16 (pMsg->pBuf, pMsg->len - 2);
        *((uint16_t *) (pMsg->pBuf + pMsg->len - 2)) = htons (tmp);

        return pMsg;
    }

    /* 01 06 00 01 00 01 19 CA */
    *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CODE;
    switch (pObj->dpid) { /* write */
        case 39:          /* switch */
            *((uint8_t *) (pMsg->pBuf + 3)) = 1;
            if (pObj->value.dp_bool) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 1;
            } else {
                *((uint8_t *) (pMsg->pBuf + 5)) = 0;
            }

            break;

        case 28: /* speed */
            // Modbus枚举值: 1低风、2中风、3高风
            // 枚举值: low, high
            *((uint8_t *) (pMsg->pBuf + 3)) = 3;
            if (pObj->value.dp_enum == 0) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 1;
            } else if (pObj->value.dp_enum == 1) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 3;
            }

            break;
        case 101: /* mode */
            // 枚举值: fresh_air, dehumidifier
            *((uint8_t *) (pMsg->pBuf + 3)) = 2;
            if (pObj->value.dp_enum == 0) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 1;
            } else if (pObj->value.dp_enum == 1) {
                *((uint8_t *) (pMsg->pBuf + 5)) = 3;
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
MbusObj_t **parse (uint8_t devId, UNUSED uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    if (!pBuf || (len < 8)) {
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent report invalid parameter!" COLOR_CLEAR);
        return NULL;
    }

    MbusVent_t **ppVent = (MbusVent_t **) &pvMbusNode;
    if (!(*ppVent)->pSwitch || !(*ppVent)->pSpeed) {
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent control invalid parameter!" COLOR_CLEAR);
        return NULL;
    }
    uint16_t key1 = devId * 256 + READ_CODE;
    uint16_t key2 = devId * 256 + WRITE_CODE;
    uint8_t invalid = 0;
    uint8_t *pos = memmatch ((uint8_t *) pBuf, key1, len);
    if (!pos || (len + pBuf - pos < 14)) {
        invalid++;
        pos = memmatch ((uint8_t *) pBuf, key2, len);
        if (!pos || (len + pBuf - pos < 14)) {
            invalid++;
        }
    }
    if (invalid == 2) {
        logErr ( "mbus plugin report: invalid command");
        return NULL;
    }

    if (pos[1] != READ_CODE) return NULL;

    MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX + 1);
    if (!ppMbusObj) {
        logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }
    ppMbusObj[MAX_IDX] = NULL;

    ppMbusObj[SWITCH]->localPoint = 39;
    ppMbusObj[SPEED]->localPoint = 28;
    ppMbusObj[MODE]->localPoint = 101;
    if (pos[4] == 1) {
        ppMbusObj[SWITCH]->value = 1;
    } else {
        ppMbusObj[SWITCH]->value = 0;
    }

    if (pos[6] == 1) {
        ppMbusObj[SPEED]->value = 0;
    } else if (pos[6] == 3) {
        ppMbusObj[SPEED]->value = 1;
    }

    if (pos[8] == 3) {
        ppMbusObj[MODE]->value = 1;
    } else {
        ppMbusObj[MODE]->value = 0;
    }

    return ppMbusObj;
}

const MbusIf Mbus_Vent_Broan_DTouch = {
    .build = build,
    .parse = parse};

#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include "zlog.h"
#include <netinet/in.h>

/*
 * ERDH除湿新风一体机
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

#define READ_CODE 0x10
#define WRITE_CODE 0x11


static MbusObj_t MbusObj[MAX_IDX];

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

    if (!pObj) { /* read */
        pMsg->len = 8;
        pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
        if (!pMsg->pBuf) {
            free (pMsg);
            logErr ( COLOR_RED "[mbus Plugin] Broan Vent control alloc fail!" COLOR_CLEAR);
            return NULL;
        }

        *((uint8_t *) (pMsg->pBuf + 0)) = devId;
        *((uint8_t *) (pMsg->pBuf + 1)) = READ_CODE;
        uint16_t tmp = crc16 (pMsg->pBuf, pMsg->len - 2);
        *((uint16_t *) (pMsg->pBuf + pMsg->len - 2)) = htons (tmp);

        return pMsg;
    }
    pMsg->len = 12;

    pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent control alloc fail!" COLOR_CLEAR);
        return NULL;
    }

/*
 * DATA0: 0b##[设定湿度] [除湿]   [风量][风阀]# [开关机]
 * DATA1: 0b### [1 除湿/0 送风]   [风量1 高0 低][风阀 1 开0 管] #[开关机1 开0 关]
 */
    *((uint8_t *) (pMsg->pBuf + 0)) = devId;
    *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CODE;
    *((uint16_t *) (pMsg->pBuf + 4)) = htons (0x4);

    switch (pObj->dpid) { /* write */
        case 8:           /* humidity_set */
            *((uint8_t *) (pMsg->pBuf + 6)) = 2;
            *((uint8_t *) (pMsg->pBuf + 8)) = pObj->value.dp_value;
            break;
        case 22: /* fresh_air_valve */
            *((uint8_t *) (pMsg->pBuf + 6)) = 4;
            *((uint8_t *) (pMsg->pBuf + 7)) = pObj->value.dp_bool ? 4 : 0;
            break;
        case 28: /* speed */
            *((uint8_t *) (pMsg->pBuf + 6)) = 8;
            *((uint8_t *) (pMsg->pBuf + 7)) = pObj->value.dp_value ? 8 : 0;
            break;
        case 39: /* switch */
            *((uint8_t *) (pMsg->pBuf + 6)) = 5;
            *((uint8_t *) (pMsg->pBuf + 7)) = (pObj->value.dp_bool ? 5 : 0);
            break;
        case 101: /*模式*/
            *((uint8_t *) (pMsg->pBuf + 6)) = 0x10;
            if (pObj->value.dp_enum == 0) {
                *((uint8_t *) (pMsg->pBuf + 7)) = 0; // 送风
            } else if (pObj->value.dp_enum == 1) {
                *((uint8_t *) (pMsg->pBuf + 7)) = 0x10; // 除湿
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
        if (!pos || (len + pBuf - pos < 7)) {
            invalid++;
        }
    }
    if (invalid == 2) {
        logErr ( COLOR_RED"mbus plugin report: invalid command"COLOR_CLEAR);
        return NULL;
    }

    //  01 11 00 00 00 00 fd c9
    // 01 10 00 00 00 09 08 02 3f 36 13 00 00 ff 00 a8 ff
    MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX + 1);
    if (!ppMbusObj) {
        logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }
    ppMbusObj[MAX_IDX] = NULL;

    if (pos[1] != READ_CODE) {
        /*
         * DATA0: 0b##[设定湿度] [除湿]   [风量][风阀]# [开关机]
         * DATA1: 0b### [1 除湿/0 送风]   [风量1 高0 低][风阀 1 开0 管] #[开关机1 开0 关]
         */
        ppMbusObj[SWITCH]->value = pos[6] & 0x01;
        ppMbusObj[SWITCH]->localPoint = VENT_SWITCH;

        ppMbusObj[SPEED]->value = (pos[6] & 0x04) >> 2;
        ppMbusObj[SPEED]->localPoint = VENT_SPEED;

        ppMbusObj[MODE]->value = (pos[6] & 0x08) >> 3;
        ppMbusObj[MODE]->localPoint = VENT_MODE;

        MbusObj[SWITCH].value = ppMbusObj[SWITCH]->value;
        MbusObj[SPEED].value = ppMbusObj[SPEED]->value;
        MbusObj[MODE].value = ppMbusObj[MODE]->value;
    } else if (pos[1] != WRITE_CODE) {
        ppMbusObj[SWITCH]->value = MbusObj[SWITCH].value;
        ppMbusObj[SWITCH]->localPoint = VENT_SWITCH;

        ppMbusObj[SPEED]->value = MbusObj[SPEED].value;
        ppMbusObj[SPEED]->localPoint = VENT_SPEED;

        ppMbusObj[MODE]->value = MbusObj[MODE].value;
        ppMbusObj[MODE]->localPoint = VENT_MODE;
    }

    return ppMbusObj;
}

const MbusIf Mbus_Vent_Broan_ERDH = {
    .build = build,
    .parse = parse};

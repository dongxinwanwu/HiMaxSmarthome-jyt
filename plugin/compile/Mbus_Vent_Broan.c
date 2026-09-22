#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include "zlog.h"
#include <netinet/in.h>

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

static MbusObj_t MBusVentLocalPoint[MAX_IDX] = {
    {.localPoint = VENT_SWITCH, .type = MODBUS_DATA_TYPE_BOOL, .value = 0, .sndValue = 0},
    {.localPoint = VENT_SPEED, .type = MODBUS_DATA_TYPE_ENUM, .value = 0, .sndValue = 0},
};

#define READ_CODE 0x3
#define WRITE_CODE 0x10

/* 调度方做内存释放 */
// 01 10 00 00 00 00 00 00 90 06
// 01 03 00 00 00 05 85 C9
Msg_t *build (uint8_t devId, uint16_t regAddr, uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj) {
    (void) regAddr;
    (void) groupAddr;
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
        *((uint16_t *) (pMsg->pBuf + 2)) = htons (0x100);
        *((uint16_t *) (pMsg->pBuf + 4)) = htons (0x2);
        uint16_t tmp = crc16 (pMsg->pBuf, pMsg->len - 2);
        *((uint16_t *) (pMsg->pBuf + pMsg->len - 2)) = htons (tmp);

        return pMsg;
    }
    pMsg->len = 13;

    pMsg->pBuf = (uint8_t *) calloc (pMsg->len, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent control alloc fail!" COLOR_CLEAR);
        return NULL;
    }
    /* 01 10 01 00 00 02 04 00 00 00 00 FE 3F */
    *((uint8_t *) (pMsg->pBuf + 0)) = devId;
    *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CODE;

    switch (pObj->dpid) { /* write */
        case 39:          /* switch */
            *((uint16_t *) (pMsg->pBuf + 2)) = htons (0x100);
            *((uint16_t *) (pMsg->pBuf + 4)) = htons (0x2);
            *((uint8_t *) (pMsg->pBuf + 6)) = 4;
            *((uint16_t *) (pMsg->pBuf + 7)) = htons (pObj->value.dp_bool);
            *((uint16_t *) (pMsg->pBuf + 9)) = htons (MBusVentLocalPoint[SPEED].value);
            MBusVentLocalPoint[SWITCH].value = pObj->value.dp_bool;
            logDbg ( "[mbus Plugin] Broan Vent control: switch(%d) speed(%d)", MBusVentLocalPoint[SWITCH].value, MBusVentLocalPoint[SPEED].value);

            break;

        case 28: /* speed */
        case 101:
            // Modbus枚举值: 0关闭、1低风、2中风、3高风
            // 枚举值: off, low, middle, high
            *((uint16_t *) (pMsg->pBuf + 2)) = htons (0x100);
            *((uint16_t *) (pMsg->pBuf + 4)) = htons (0x2);
            *((uint8_t *) (pMsg->pBuf + 6)) = 4;
            *((uint16_t *) (pMsg->pBuf + 7)) = htons (1);
            *((uint16_t *) (pMsg->pBuf + 9)) = htons (pObj->value.dp_enum);
            MBusVentLocalPoint[SWITCH].value = 1;
            MBusVentLocalPoint[SPEED].value = pObj->value.dp_enum;
            logDbg ( "[mbus Plugin] Broan Vent control: switch(%d) speed(%d)", MBusVentLocalPoint[SWITCH].value, MBusVentLocalPoint[SPEED].value);

            break;
        default:
            logErr ( COLOR_RED "[mbus Plugin] localPoint(%d) not be supported!" COLOR_CLEAR, pObj->dpid);
            break;
    }
    uint16_t tmp = crc16 ((uint8_t *) pMsg->pBuf, pMsg->len - 2);
    *((uint16_t *) (pMsg->pBuf + pMsg->len - 2)) = htons (tmp);

    return pMsg;
}

// 01 10 01 03 2C 0F 00 00 20 03 00 00 8E B9
// 01 03 01 01 2C 10 00 00 20 03 00 00 22 9C
/**
 * @brief
 * @note
 * @param  modbusId:
 * @param  *pBuf:
 * @param  len:
 * @retval 返回modbus数据节点个数
 */
MbusObj_t **parse (uint8_t devId, uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    (void) regAddr;
    if (!pBuf || (len < 7)) {
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent report invalid parameter!" COLOR_CLEAR);
        return NULL;
    }
#if 0
    MbusVent_t *pVent = container_of (pvMbusNode, MbusVent_t, MbusBase_t);
#else
    MbusVent_t **ppVent = (MbusVent_t **) &pvMbusNode;
#endif
    if (!(*ppVent)->pSwitch || !(*ppVent)->pSpeed) {
        logErr ( COLOR_RED "[mbus Plugin] Broan Vent control invalid parameter!" COLOR_CLEAR);
        return NULL;
    }
    uint16_t key1 = devId * 256 + READ_CODE;
    uint16_t key2 = devId * 256 + WRITE_CODE;
    uint8_t invalid = 0;
    uint8_t *pos = memmatch ((uint8_t *) pBuf, key1, len);
    if (!pos || (len + pBuf - pos < 7)) {
        invalid++;
        pos = memmatch ((uint8_t *) pBuf, key2, len);
        if (!pos || (len + pBuf - pos < 7)) {
            invalid++;
        }
    }
    if (invalid == 2) {
        logErr ( "mbus plugin report: invalid command");
        return NULL;
    }

    MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX + 1);
    if (!ppMbusObj) {
        logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }
    ppMbusObj[MAX_IDX] = NULL;

    if (pos[1] == READ_CODE) {
        ppMbusObj[SWITCH]->value = pos[4]; /* 开关机：1->开机，0->关机 */
        ppMbusObj[SWITCH]->localPoint = VENT_SWITCH;
        ppMbusObj[SWITCH]->sndValue = pos[4];
        MBusVentLocalPoint[SWITCH].value = pos[4];
        if (ppMbusObj[SWITCH]->value > 0) { /*  */
            ppMbusObj[SPEED]->localPoint = VENT_SPEED;
            ppMbusObj[SPEED]->value = pos[6]; /* 百朗: 0关闭, 1抵挡，2中档，3高档 */
            ppMbusObj[SPEED]->sndValue = pos[6];
            MBusVentLocalPoint[SPEED].value = pos[6];

            char *pSpeed = NULL;
            if (ppMbusObj[SPEED]->value == 0) {
                pSpeed = (char *) "off";
            } else if (ppMbusObj[SPEED]->value == 1) {
                pSpeed = (char *) "low";
            } else if (ppMbusObj[SPEED]->value == 2) {
                pSpeed = (char *) "middle";
            } else if (ppMbusObj[SPEED]->value == 3) {
                pSpeed = (char *) "high";
            }
            logDbg ( COLOR_PURPLE "[mbus vent read ack] switch(%s) speed(%s)" COLOR_CLEAR, (ppMbusObj[SWITCH]->value ? "on" : "off"), pSpeed);
        }
    } else if (pos[1] == WRITE_CODE) {
        /* 01 10 01 00 00 02 40 34 */
        if ((pos[2] == 0x1) && (pos[3] == 0x0) && (pos[5] == 2)) {
            ppMbusObj[SWITCH]->value = MBusVentLocalPoint[SWITCH].value;
            ppMbusObj[SWITCH]->localPoint = VENT_SWITCH;
            ppMbusObj[SPEED]->value = MBusVentLocalPoint[SPEED].value;
            ppMbusObj[SPEED]->localPoint = VENT_SPEED;
            logDbg ( "[mbus vent write ack] switch(%d) speed(%d)", ppMbusObj[SWITCH]->value, ppMbusObj[SPEED]->value);
            char *pSpeed = NULL;
            if (ppMbusObj[SPEED]->value == 0) {
                pSpeed = (char *) "off";
            } else if (ppMbusObj[SPEED]->value == 1) {
                pSpeed = (char *) "low";
            } else if (ppMbusObj[SPEED]->value == 2) {
                pSpeed = (char *) "middle";
            } else if (ppMbusObj[SPEED]->value == 3) {
                pSpeed = (char *) "high";
            }
            logDbg ( COLOR_PURPLE "[mbus vent write ack] %s(%s) %s(%s)" COLOR_CLEAR,
                        GetPointFeature (ppMbusObj[SWITCH]->localPoint), (ppMbusObj[SWITCH]->value ? "on" : "off"), GetPointFeature (ppMbusObj[SPEED]->localPoint), pSpeed);
        }
    }

    return ppMbusObj;
}

const MbusIf Mbus_Vent_Broan = {
    .build = build,
    .parse = parse};

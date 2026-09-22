#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include "zlog.h"

/* 非Modbus协议，特殊处理 */

/*
PID="ityiaeb3uzapmahe"

DP    功能点名称        标识符            传输类型    数据类型    功能点属性
39    开关             switch          rw         bool
101   风速设置          wind            rw         enum       枚举值: off, low, middle, high
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

#define READ_CODE 0x3
#define WRITE_CODE 0x10

/*
 0: {.localPoint = VENT_SWITCH, .type = MODBUS_DATA_TYPE_BOOL},
 1: {.localPoint = VENT_SPEED, .type = MODBUS_DATA_TYPE_ENUM}
 */

/* 调度方做内存释放 */
// 01 10 00 00 00 00 00 00 90 06
// 01 03 00 00 00 00 00 00 B2 C7
Msg_t *build (uint8_t devId, uint16_t regAddr, uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj) {
    (void) regAddr;
    (void)groupAddr;
    (void)pvMbusNode;

    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ( COLOR_RED "[mbus Plugin] TuoLi_TLCAA301R Ventilation control alloc fail!\n" COLOR_CLEAR);
        return NULL;
    }
    pMsg->pBuf = (uint8_t *) calloc (10, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "[mbus Plugin] TuoLi_TLCAA301R Ventilation control alloc fail!\n" COLOR_CLEAR);
        return NULL;
    }
    pMsg->len = 10;
    *((uint8_t *) pMsg->pBuf + 0) = devId;
    if (!pObj) {
        *((uint8_t *) pMsg->pBuf + 1) = READ_CODE;
        uint16_t tmp = crc16 (pMsg->pBuf, 8);
        *((uint8_t *) pMsg->pBuf + 8) = tmp >> 8;
        *((uint8_t *) pMsg->pBuf + 9) = tmp >> 0;

        return pMsg;
    }

    *((uint8_t *) pMsg->pBuf + 1) = WRITE_CODE;
    switch (pObj->dpid) {
        case 39: /* switch */
            *((uint8_t *) pMsg->pBuf + 2) = pObj->value.dp_bool;

            break;

        case 101: /* speed */
            // Modbus枚举值: 0关闭、1低风、2中风、3高风
            // 枚举值: off, low, middle, high
            *((uint8_t *) pMsg->pBuf + 2) = 0x1;
            *((uint8_t *) pMsg->pBuf + 3) = pObj->value.dp_enum;

            break;
        default:
            logErr ( COLOR_RED "[mbus Plugin] localPoint(%d) not be supported!\n" COLOR_CLEAR, pObj->dpid);
            break;
    }
    uint16_t tmp = crc16 ((uint8_t *) pMsg->pBuf, 8);
    *((uint8_t *) pMsg->pBuf + 8) = tmp >> 8;
    *((uint8_t *) pMsg->pBuf + 9) = tmp >> 0;

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
    (void)pvMbusNode;
    if (!pBuf || (len < 14)) {
        logErr ( COLOR_RED "[mbus Plugin] TuoLi_TLCAA301R Ventilation report invalid parameter!\n" COLOR_CLEAR);
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
        logErr ( "mbus plugin report: invalid command\n");
        return NULL;
    }

    MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX);
    if (!ppMbusObj) {
        logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }
    ppMbusObj[SWITCH]->value = pos[2]; /* 开关机：1->开机，0->关机 */
    ppMbusObj[SPEED]->value = pos[3];  /* 拓力: 1抵挡，2中档，3高档 */
    ppMbusObj[SWITCH]->localPoint = VENT_SWITCH;
    ppMbusObj[SPEED]->localPoint = VENT_SPEED;
    char *pSpeed = NULL;
    if (ppMbusObj[SPEED]->value == 1) {
        pSpeed = (char *)"low";
    } else if (ppMbusObj[SPEED]->value == 2) {
        pSpeed = (char *)"middle";
    } else if (ppMbusObj[SPEED]->value == 3) {
        pSpeed = (char *)"high";
    }
    if (pos[1] == READ_CODE) {
        logTrc ( "read mbus vent%d switch(%s) speed(%s)", regAddr, (ppMbusObj[SWITCH]->value ? "on" : "off"), pSpeed);
    } else if (pos[1] == WRITE_CODE) {
        logTrc ( "write mbus vent%d switch(%s) speed(%s)", regAddr, (ppMbusObj[SWITCH]->value ? "on" : "off"), pSpeed);
    }

    return ppMbusObj;
}

const MbusIf Modbus_Ventilation_TuoLi_TLCAA301R = {
    .build = build,
    .parse = parse};

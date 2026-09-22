#include "common.h"
#include "crc16.h"
#include "misc_dev_plugins.h"
#include <netinet/in.h>
#include "zlog.h"
#include "main.h"
#include "utils.h"

/*
producct id: ilpuchpli94eigt7

DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关            switch_1          rw        bool
*/

#define READ_CODE 0x3
#define WRITE_CODE 0x6


#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

#define MODBUS_MAX_LEN 256
#define MODBUS_MIN_LEN 8

typedef enum {
    SWITCH,
    TEMP,
    ROOM_TEMP,
    MAX_IDX
} Idx_e;

Msg_t *build(uint8_t devId, uint16_t regAddr, uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj) {
    (void ) regAddr;
    (void ) groupAddr;
    (void ) pvMbusNode;
    Msg_t *pMsg = (Msg_t *) calloc(1, sizeof(Msg_t));
    if (!pMsg) {
        logErr ( COLOR_RED "mbus Plugin control alloc fail!\n" COLOR_CLEAR);
        return NULL;
    }
    pMsg->pBuf = (uint8_t *) calloc(8, 1);
    if (!pMsg->pBuf) {
        free(pMsg);
        logErr ( COLOR_RED "mbus Plugin control alloc fail!\n" COLOR_CLEAR);
        return NULL;
    }
    pMsg->len = 8;
    *((uint8_t *) pMsg->pBuf + 0) = devId;
    if (!pObj) {
        *((uint8_t *) pMsg->pBuf + 1) = READ_CODE;
        *((uint16_t *) (pMsg->pBuf + 2)) = htons(0x100B);
        *((uint16_t *) (pMsg->pBuf + 4)) = htons(0x0001);
        uint16_t tmp = crc16(pMsg->pBuf, 6);
        *((uint16_t *) (pMsg->pBuf + 6)) = htons(tmp);

        return pMsg;
    }
    *((uint8_t *) pMsg->pBuf + 1) = WRITE_CODE;

    switch (pObj->dpid) {
        case 1: /* Switch_1 */
            *((uint16_t *) (pMsg->pBuf + 2)) = htons(0x1008);
            *((uint16_t *) (pMsg->pBuf + 4)) = htons(pObj->value.dp_bool);

            break;
        default:
            logErr ( COLOR_RED "[mbus Plugin] localPoint(%d) not be supported!\n" COLOR_CLEAR, pObj->value.dp_bool);
            break;
    }
    uint16_t tmp = crc16((uint8_t *) pMsg->pBuf, 6);
    *((uint16_t *) (pMsg->pBuf + 6)) = htons(tmp);

    return pMsg;
}

MbusObj_t **parse(uint8_t devId, uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    (void) regAddr;
    (void)pvMbusNode;
    if (!pBuf || (len < MODBUS_MIN_LEN)) {
        logErr ( COLOR_RED "[mbus Plugin] report() invalid parameter!\n" COLOR_CLEAR);
        return NULL;
    }

    uint16_t key1 = devId * 256 + READ_CODE;
    uint16_t key2 = devId * 256 + WRITE_CODE;
    uint8_t invalid = 0;
    uint8_t *pos = memmatch((uint8_t *) pBuf, key1, len);
    if (!pos || (len + pBuf - pos < MODBUS_MIN_LEN)) {
        invalid++;
        pos = memmatch((uint8_t *) pBuf, key2, len);
        if (!pos || (len + pBuf - pos < MODBUS_MIN_LEN)) {
            invalid++;
        }
    }
    if (invalid == 2) {
        logErr ( "mbus plugin report: invalid command");
        return NULL;
    }

    if (pos[1] == READ_CODE) {
        MbusObj_t **ppMbusObj = NewMbusObjs(1);
        if (!ppMbusObj) {
            logErr ( COLOR_RED "[mbus Plugin] report alloc fail!" COLOR_CLEAR);
            return NULL;
        }

        ppMbusObj[0]->localPoint = PANEL_SWITCH;
        ppMbusObj[0]->value = pos[5];

        logTrc ( "read mbus panel%d switch=%d", devId, ppMbusObj[0]->value);

        return ppMbusObj;
    } else if (pos[1] == WRITE_CODE) {
        MbusObj_t **ppMbusObj = NewMbusObjs(1);
        if (!ppMbusObj) {
            logErr ( COLOR_RED "[mbus Plugin] report alloc fail!\n" COLOR_CLEAR);
            return NULL;
        }
        ppMbusObj[0]->localPoint = PANEL_SWITCH;
        ppMbusObj[0]->value = pos[5];

        logTrc ( "write mbus heat%d switch=%s", devId, (ppMbusObj[0]->value ? "on" : "off"));

        return ppMbusObj;
    }

    return NULL;
}

const MbusIf Mbus_Panel_Ohosure = {
        .build = build,
        .parse = parse};

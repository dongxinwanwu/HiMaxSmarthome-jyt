#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "zlog.h"
#include <arpa/inet.h>
#include "utils.h"

/*
 * 产品ID: iqkpbovlhaxkekh1
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关            switch            rw        bool
2     温度设置         temp_set          rw        value     数值范围: 16-30, 间距: 1, 倍数: 0, 单位:
3     当前温度         temp_current      ro        value     数值范围: -20-55, 间距: 1, 倍数: 0, 单位:
4     工作模式         mode              rw        enum      枚举值: cold, hot, wet, wind
5     风速            fan_speed_enum    rw        enum      枚举值: auto, high, middle, low
*/

/*
指令下发：
--------------------------------------------------------------------
|网关地址 |功能码    |控制值  |暖通设备数量  |暖通设备地址          | 校验  |
--------------------------------------------------------------------
|1byte  |1byte    |1byte  |1byte       |暖通设备数量 × 2byte  |1byte |
--------------------------------------------------------------------

数据反馈：
--------------------------------------------------------------------
|网关地址 |功能码    |控制值  |暖通设备数量  |暖通设备地址 + 状态值  | 校验  |
--------------------------------------------------------------------
|1byte  |1byte    |1byte  |1byte       |暖通设备数量 × 10byte |1byte |
--------------------------------------------------------------------
*/

#define WRITE_CMD_SWITCH    0x31
#define WRITE_CMD_TEMP      0x32
#define WRITE_CMD_MODE      0x33
#define WRITE_CMD_SPEED     0x34

#define READ_CMD_STATUS     0x50

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

#define MODBUS_MAX_LEN 256
#define MODBUS_MIN_LEN 7

typedef enum {
    SWITCH,
    TEMP,
    MODE,
    SPEED,
    ROOM_TEMP,
    MAX_IDX
} Idx_e;

/* Warning: The caller must free memory */
Msg_t *build(uint8_t devId, uint16_t regAddr, uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj) {
    (void )groupAddr;
    (void )pvMbusNode;
    Msg_t *pMsg = (Msg_t *) calloc(1, sizeof(Msg_t));
    if (!pMsg) {
        logErr ( "mbus Plugin control alloc fail!\n");
        return NULL;
    }
    pMsg->pBuf = (uint8_t *) calloc(7, 1);
    if (!pMsg->pBuf) {
        free(pMsg);
        logErr ( "mbus Plugin control alloc fail!\n");
        return NULL;
    }
    pMsg->len = 7;
    *((uint8_t *) (pMsg->pBuf + 0)) = devId;
    if (!pObj) {
        *((uint8_t *) (pMsg->pBuf + 1)) = READ_CMD_STATUS;
        *((uint8_t *) (pMsg->pBuf + 2)) = 1;
        *((uint8_t *) (pMsg->pBuf + 3)) = 1;
        *((uint16_t *) (pMsg->pBuf + 4)) = htons (regAddr);
        *((uint8_t *) (pMsg->pBuf + 6)) = cs((uint8_t *) pMsg->pBuf, 6);

        return pMsg;
    }

    switch (pObj->dpid) {
        case 1: /* Switch */
            *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CMD_SWITCH;
            *((uint8_t *) (pMsg->pBuf + 2)) = pObj->value.dp_bool;
            *((uint8_t *) (pMsg->pBuf + 3)) = 1;
            *((uint16_t *) (pMsg->pBuf + 4)) = htons (regAddr);
            break;
        case 2: /* Temp */
            *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CMD_TEMP;
            *((uint8_t *) (pMsg->pBuf + 2)) = pObj->value.dp_value;
            *((uint8_t *) (pMsg->pBuf + 3)) = 1;
            *((uint16_t *) (pMsg->pBuf + 4)) = htons (regAddr);
            break;
        case 4: /* Mode */
            /*
             * tuya : cold, hot, wet, wind
             * zhonghong: cold(1),wet(2),wind(4),hot(8)
             * */
            *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CMD_MODE;
            *((uint8_t *) (pMsg->pBuf + 3)) = 1;
            *((uint16_t *) (pMsg->pBuf + 4)) = htons (regAddr);

            if (pObj->value.dp_enum == 0) {
                *((uint8_t *) (pMsg->pBuf + 2)) = 1;
            } else if (pObj->value.dp_enum == 1) {
                *((uint8_t *) (pMsg->pBuf + 2)) = 8;
            } else if (pObj->value.dp_enum == 2) {
                *((uint8_t *) (pMsg->pBuf + 2)) = 2;
            } else if (pObj->value.dp_enum == 3) {
                *((uint8_t *) (pMsg->pBuf + 2)) = 4;
            }

            break;
        case 5: /* Speed */
            /*
             * tuya : auto, high, middle, low
             * zhonghong: auto(0),high(1),middle(2),low(4)
             * */
            *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CMD_SPEED;
            *((uint8_t *) (pMsg->pBuf + 3)) = 1;
            *((uint16_t *) (pMsg->pBuf + 4)) = htons (regAddr);
            if (pObj->value.dp_enum == 3) {
                *((uint8_t *) (pMsg->pBuf + 2)) = 4;
            } else {
                *((uint8_t *) (pMsg->pBuf + 2)) = pObj->value.dp_enum;
            }

            break;
        default:
            logErr ( "[mbus Plugin] localPoint(%d) not be supported!\n", pObj->dpid);
            break;
    }
    *((uint8_t *) (pMsg->pBuf + 6)) = cs((uint8_t *) pMsg->pBuf, 6);

    return pMsg;
}


/*
*   [ttymxc1 snd] --> 01 31 00 01 01 08 3c
    [ttymxc1 rcv] --> 01 31 00 01 01 08 3c
 *  [ttymxc1 rcv] --> 01 50 01 01 01 09 00 1c 04 00 0a 00 00 00 87
 */
MbusObj_t **parse(uint8_t devId, uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    (void)pvMbusNode;
    if (!pBuf || (len < 7)) {
        logErr ( "[mbus Plugin] report() invalid parameter!");
        return NULL;
    }

    uint16_t key1 = devId * 256 + WRITE_CMD_SWITCH;
    uint16_t key2 = devId * 256 + WRITE_CMD_SPEED;
    uint16_t key3 = devId * 256 + WRITE_CMD_TEMP;
    uint16_t key4 = devId * 256 + WRITE_CMD_MODE;
    uint16_t key5 = devId * 256 + READ_CMD_STATUS;

    uint8_t invalid = 0;
    uint8_t *pos = memmatch((uint8_t *) pBuf, key1, len);
    if (!pos || (len + pBuf - pos < 7)) {
        invalid++;
    } else goto KEY_FOUND;
    pos = memmatch((uint8_t *) pBuf, key2, len);
    if (!pos || (len + pBuf - pos < 7)) {
        invalid++;
    }else goto KEY_FOUND;
    pos = memmatch((uint8_t *) pBuf, key3, len);
    if (!pos || (len + pBuf - pos < 7)) {
        invalid++;
    }else goto KEY_FOUND;
    pos = memmatch((uint8_t *) pBuf, key4, len);
    if (!pos || (len + pBuf - pos < 7)) {
        invalid++;
    }else goto KEY_FOUND;
    pos = memmatch((uint8_t *) pBuf, key5, len);
    if (!pos || (len + pBuf - pos < 7)) {
        invalid++;
    }else goto KEY_FOUND;

    if (invalid == 5) {
        logErr ( "mbus plugin report: invalid command");
        return NULL;
    }
    KEY_FOUND:

    // 01 50 01 01 01 05 59
    // 01 50 01 01 01 05 00 16 08 00 0a 00 00 00 81

    if (pos[1] == READ_CMD_STATUS) {
        MbusObj_t **ppMbusObj = NewMbusObjs(MAX_IDX);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] report() NewMbusObjs() failed!");
            return NULL;
        }

        ppMbusObj[SWITCH]->localPoint = ACD_SWITCH;
        ppMbusObj[TEMP]->localPoint = ACD_TEMP;
        ppMbusObj[MODE]->localPoint = ACD_MODE;
        ppMbusObj[SPEED]->localPoint = ACD_SPEED;
        ppMbusObj[ROOM_TEMP]->localPoint = ACD_ENV_TEMP;
        ppMbusObj[SWITCH]->value = pos[6];
        ppMbusObj[TEMP]->value = pos[7];
        ppMbusObj[ROOM_TEMP]->value = pos[10] * 10;
        /*
         * tuya : cold, hot, wet, wind
         * zhonghong: cold(1),wet(2),wind(4),hot(8)
         * */
        char *pModeTag = NULL;
        if (pos[8] == 1) {
            ppMbusObj[MODE]->value = 0;
            pModeTag = (char *)"cold";
        } else if (pos[8] == 2) {
            ppMbusObj[MODE]->value = 2;
            pModeTag = (char *)"wet";
        } else if (pos[8] == 4) {
            ppMbusObj[MODE]->value = 3;
            pModeTag = (char *)"wind";
        } else if (pos[8] == 8) {
            ppMbusObj[MODE]->value = 1;
            pModeTag = (char *)"hot";
        } else {
            zlog_error(pLogCat, COLOR_RED "[mbus Plugin] acd mode not support.\n" COLOR_CLEAR);
        }
        /*
         * tuya : auto, high, middle, low
         * zhonghong: auto(0),high(1),middle(2),low(4)
         * */
        char *pSpeedTag = NULL;
        if (pos[9] == 4) {
            ppMbusObj[SPEED]->value = 3;
            pSpeedTag = (char *)"low";
        } else if (pos[9] == 2) {
            ppMbusObj[SPEED]->value = pos[9];
            pSpeedTag = (char *)"middle";
        } else if (pos[9] == 1) {
            ppMbusObj[SPEED]->value = pos[9];
            pSpeedTag = (char *)"high";
        } else if (pos[9] == 0) {
            ppMbusObj[SPEED]->value = pos[9];
            pSpeedTag = (char *)"auto";
        } else {
            zlog_error(pLogCat, COLOR_RED "[mbus Plugin] acd speed not support.\n" COLOR_CLEAR);
        }

        logTrc ( "read mbus acd%d switch(%s) mode(%s) temp(%d) env(%d) speed(%s)", regAddr,
                    (ppMbusObj[SWITCH]->value ? "on" : "off"), pModeTag, ppMbusObj[TEMP]->value, ppMbusObj[ROOM_TEMP]->value, pSpeedTag);

        return ppMbusObj;
    } else if (pos[1] == WRITE_CMD_SWITCH) {
        MbusObj_t **ppMbusObj = NewMbusObjs(1);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] report() NewMbusObjs() failed!");
            return NULL;
        }
        ppMbusObj[0]->localPoint = ACD_SWITCH;
        ppMbusObj[0]->value = pos[2];

        return ppMbusObj;
    } else if (pos[1] == WRITE_CMD_TEMP) {
        MbusObj_t **ppMbusObj = NewMbusObjs(1);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] report() NewMbusObjs() failed!");
            return NULL;
        }
        ppMbusObj[0]->localPoint = ACD_TEMP;
        ppMbusObj[0]->value = pos[2];

        return ppMbusObj;
    } else if (pos[1] == WRITE_CMD_MODE) {
        MbusObj_t **ppMbusObj = NewMbusObjs(1);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] report() NewMbusObjs() failed!");
            return NULL;
        }
        ppMbusObj[0]->localPoint = ACD_MODE;
        /*
         * tuya : cold, hot, wet, wind
         * zhonghong: cold(1),wet(2),wind(4),hot(8)
         * */
        char *pModeTag = NULL;
        if (pos[2] == 1) {
            ppMbusObj[0]->value = 0;
            pModeTag = (char *)"cold";
        } else if (pos[2] == 2) {
            ppMbusObj[0]->value = 2;
            pModeTag = (char *)"wet";
        } else if (pos[2] == 4) {
            ppMbusObj[0]->value = 3;
            pModeTag = (char *)"wind";
        } else if (pos[2] == 8) {
            ppMbusObj[0]->value = 1;
            pModeTag = (char *)"hot";
        } else {
            zlog_error(pLogCat, COLOR_RED "[mbus Plugin] acd mode not support.\n" COLOR_CLEAR);
        }
        logTrc ( "write mbus acd%d mode(%s)", regAddr, pModeTag);
        return ppMbusObj;
    } else if (pos[1] == WRITE_CMD_SPEED) {
        MbusObj_t **ppMbusObj = NewMbusObjs(1);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] report() NewMbusObjs() failed!");
            return NULL;
        }
        ppMbusObj[0]->localPoint = ACD_SPEED;
        /*
         * tuya : auto, high, middle, low
         * zhonghong: auto(0),high(1),middle(2),low(4)
         *  01 34 0a 01 01 06 47
         *  01 34 00 01 01 06 3d
         * */
        char *pSpeedTag = NULL;
        if (pos[2] == 4) {
            ppMbusObj[0]->value = 3;
            pSpeedTag = (char *)"low";
        } else if (pos[2] == 2) {
            ppMbusObj[0]->value = 2;
            pSpeedTag = (char *)"middle";
        } else if (pos[2] == 1) {
            ppMbusObj[0]->value = 1;
            pSpeedTag = (char *)"high";
        } else if (pos[2] == 0) {
            ppMbusObj[0]->value = 0;
            pSpeedTag = (char *)"auto";
        } else {
            zlog_error(pLogCat, COLOR_RED "[mbus Plugin] acd speed not support.\n" COLOR_CLEAR);
        }
        logTrc ( "write mbus acd%d speed(%s-%d)", regAddr, pSpeedTag, ppMbusObj[0]->value);
        return ppMbusObj;
    }
    logErr ( "mbus plugin report: invalid command=%d", pos[1]);

    return NULL;
}

const MbusIf Mbus_Acd_ZhongHong = {
        .build = build,
        .parse = parse};

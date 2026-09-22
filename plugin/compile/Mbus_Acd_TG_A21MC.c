#include "common.h"
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include "zlog.h"
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>

/*
 * PID: tv27bplwmatqesdd
DP ID   功能点名称          标识符             数据类型      功能点属性
    1      开关            switch               rw          Bool
    2      温度设置        temp_set             rw          Value            数值范围: 16-32, 间距: 1, 倍数: 0, 单位: ℃
    3      当前温度        temp_current         ro          Value           数值范围: -10-50, 间距: 1, 倍数: 0, 单位: ℃
    4      工作模式        mode                 rw          Enum            举值: auto, cold, hot, wet, wind
    5      风速           fan_speed_enum       rw           Enum            枚举值: auto, low, middle, high
*/

#define READ_CODE 0x3
#define WRITE_CODE 0x6

#define REG_ADDR 0x64
#define STATUS_REG_LEN 2
#define CTRL_REG_LEN 2

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

#define MODBUS_MAX_LEN 256
#define MODBUS_MIN_LEN 8

typedef enum {
    SWITCH = 0,
    TEMP = 1,
    MODE = 2,
    SPEED = 3,
    MAX_IDX
} Idx_e;

//static MbusObj_t AcdCache[64][MAX_IDX];
static uint8_t AcdCache[64][2];

/* 注意:调度方做内存释放 */
Msg_t *build (uint8_t devId, uint16_t regAddr, uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj) {
    (void)pvMbusNode;
    logTrc ( "mbus acd trane build: devId=%d, regAddr=%d, groupAddr=%d", devId, regAddr, groupAddr);
    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ( "mbus Plugin control alloc fail!");
        return NULL;
    }
    pMsg->pBuf = (uint8_t *) calloc (8, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( "mbus Plugin control alloc fail!");
        return NULL;
    }
    pMsg->len = 8;
    *((uint8_t *) (pMsg->pBuf + 0)) = devId;
    if (!pObj) {
        *((uint8_t *) (pMsg->pBuf + 1)) = READ_CODE;
        *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + STATUS_REG_LEN * regAddr);
        *((uint8_t *) (pMsg->pBuf + 4)) = 0x0;
        *((uint8_t *) (pMsg->pBuf + 5)) = 0x1;
        uint16_t crc = crc16 (pMsg->pBuf, 6);
        *((uint16_t *) (pMsg->pBuf + 6)) = htons (crc);

        return pMsg;
    }
    *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CODE;
    *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + CTRL_REG_LEN * regAddr);
    *((uint8_t *) (pMsg->pBuf + 4)) = AcdCache[groupAddr][0];
    *((uint8_t *) (pMsg->pBuf + 5)) = AcdCache[groupAddr][1];

    switch (pObj->dpid) {
        case 1: /* Switch */
            *((uint8_t *) (pMsg->pBuf + 4)) &= 0x3F;
            *((uint8_t *) (pMsg->pBuf + 4)) += (pObj->value.dp_bool > 0 ? 2 : 1) << 14;

            break;
        case 2: /* Temp */
            *((uint8_t *) (pMsg->pBuf + 5)) &= 0xE0;
            *((uint8_t *) (pMsg->pBuf + 5)) += pObj->value.dp_value + 15;

            break;
        case 4: /* Mode */
            // tuya :枚举值: auto, cold, hot, wet, wind
            // acd: wind 4, hot 8, cold 1, wet 2, auto 16
            *((uint8_t *) (pMsg->pBuf + 4)) &= 0xC1;
            if (pObj->value.dp_enum == 1) {// tuya.mode.cold
                *((uint8_t *) (pMsg->pBuf + 4)) += 1 << 1;
                logTrc ( "mbus acd trane build: mode=cold, tuya.mode.cold=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 3) {// tuya.mode.wet
                *((uint8_t *) (pMsg->pBuf + 4)) += 2 << 1;
                logTrc ( "mbus acd trane build: mode=wet, tuya.mode.wet=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 4) {// tuya.mode.wind
                *((uint8_t *) (pMsg->pBuf + 4)) += 4 << 1;
                logTrc ( "mbus acd trane build: mode=wind, tuya.mode.wind=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 2) {// tuya.mode.hot
                *((uint8_t *) (pMsg->pBuf + 4)) += 8 << 1;
                logTrc ( "mbus acd trane build: mode=hot, tuya.mode.hot=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 0) {// tuya.mode.auto
                *((uint8_t *) (pMsg->pBuf + 4)) += 16 << 1;
                logTrc ( "mbus acd trane build: mode=auto, tuya.mode.auto=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            }

            break;
        case 5: /* Speed */
            /* tuya 枚举值: auto, low, middle, high */
            // acd: low1, middle2, high4, auto8
            *((uint8_t *) (pMsg->pBuf + 4)) &= 0xFE;
            *((uint8_t *) (pMsg->pBuf + 5)) &= 0x1F;
            if (pObj->value.dp_enum == 1) {
                *((uint8_t *) (pMsg->pBuf + 5)) += 1 << 5;
                logTrc ( "mbus acd trane build: mode=cold, tuya.mode.cold=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 3) {
                *((uint8_t *) (pMsg->pBuf + 5)) += 4 << 5;
                logTrc ( "mbus acd trane build: mode=wet, tuya.mode.wet=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 2) {
                *((uint8_t *) (pMsg->pBuf + 5)) += 2 << 5;
                logTrc ( "mbus acd trane build: mode=hot, tuya.mode.hot=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 0) {
                *((uint8_t *) (pMsg->pBuf + 5)) += 8 << 5;
                logTrc ( "mbus acd trane build: mode=auto, tuya.mode.auto=%d, trane.mode=%d", pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            }

            break;
        default:
            logErr ( "[mbus Plugin] localPoint(%d) not be supported!", pObj->dpid);
            break;
    }
    uint16_t tmp = crc16 ((uint8_t *) pMsg->pBuf, 6);
    *((uint16_t *) (pMsg->pBuf + 6)) = htons (tmp);

    return pMsg;
}

MbusObj_t **parse (uint8_t devId, uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    (void)pvMbusNode;
    if (!pBuf || (len < MODBUS_MIN_LEN)) {
        logErr ( "[mbus Plugin] report() invalid parameter! pBuf=%p, len=%d", pBuf, len);
        return NULL;
    }

    uint16_t key1 = devId * 256 + READ_CODE;
    uint16_t key2 = devId * 256 + WRITE_CODE;
    uint8_t invalid = 0;
    char szlog[128] = {0};
    sprintf (szlog, "[mbus trane acd parse] len=%d", len);
    // logHex ( (uint8_t *) pBuf, len);
    uint8_t *pos = memmatch ((uint8_t *) pBuf, key1, len);
    if (!pos || (len + pBuf - pos < 8)) {
        invalid++;
        pos = memmatch ((uint8_t *) pBuf, key2, len);
        if (!pos || (len + pBuf - pos < 8)) {
            invalid++;
        }
    }
    if (invalid == 2) {
        logErr ( "mbus plugin report: invalid command");
        return NULL;
    }

    if (pos[1] == READ_CODE) {
        MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
            return NULL;
        }

        // 01 03 0a 31 a9 42 02 00 aa 00 00 01 0e
        AcdCache[regAddr][0] = pos[3];
        AcdCache[regAddr][1] = pos[4];

        ppMbusObj[SWITCH]->localPoint = ACD_SWITCH;
        ppMbusObj[SPEED]->localPoint = ACD_SPEED;
        ppMbusObj[MODE]->localPoint = ACD_MODE;
        ppMbusObj[TEMP]->localPoint = ACD_TEMP;

        ppMbusObj[SWITCH]->value = AcdCache[regAddr][0] >> 14;
        if (ppMbusObj[SWITCH]->value == 1) {
            ppMbusObj[SWITCH]->value = 0;
        } else if (ppMbusObj[SWITCH]->value == 2) {
            ppMbusObj[SWITCH]->value = 1;
        }

        ppMbusObj[TEMP]->value = (AcdCache[regAddr][1] & 0x1F) - 15;

        char speedBuf[64] = {0};
        /* tuya 枚举值: auto, low, middle, high */
        // acd: low1, middle2, high4, auto8
        ppMbusObj[SPEED]->value = ((AcdCache[regAddr][0] & 1) << 4) + (AcdCache[regAddr][1] >> 5);
        if (ppMbusObj[SPEED]->value == 8) {
            ppMbusObj[SPEED]->value = 0;
            strcpy (speedBuf, "auto");
        } else if (ppMbusObj[SPEED]->value == 1) {
            ppMbusObj[SPEED]->value = 1;
            strcpy (speedBuf, "low");
        } else if (ppMbusObj[SPEED]->value == 2) {
            ppMbusObj[SPEED]->value = 2;
            strcpy (speedBuf, "mid");
        } else if (ppMbusObj[SPEED]->value == 3) {
            ppMbusObj[SPEED]->value = 4;
            strcpy (speedBuf, "high");
        }

        // tuya :枚举值: auto, cold, hot, wet, wind
        // acd: wind 4, hot 8, cold 1, wet 2, auto 16
        ppMbusObj[MODE]->value = (AcdCache[regAddr][0] & 0x3E) >> 1;

        char modeBuf[64] = {0};
        if (ppMbusObj[MODE]->value == 4) {
            ppMbusObj[MODE]->value = 4;
            strcpy (modeBuf, "wind");
        } else if (ppMbusObj[MODE]->value == 8) {
            ppMbusObj[MODE]->value = 2;
            strcpy (modeBuf, "hot");
        } else if (ppMbusObj[MODE]->value == 1) {
            ppMbusObj[MODE]->value = 1;
            strcpy (modeBuf, "cold");
        } else if (ppMbusObj[MODE]->value == 2) {
            ppMbusObj[MODE]->value = 3;
            strcpy (modeBuf, "wet");
        } else if (ppMbusObj[MODE]->value == 16) {
            ppMbusObj[MODE]->value = 0;
            strcpy (modeBuf, "auto");
        }
        logTrc ( "read mbus acd%d switch(%s) mode(%s) temp(%d) speed(%s)", regAddr, (ppMbusObj[SWITCH]->value ? "on" : "off"), modeBuf, ppMbusObj[TEMP]->value, speedBuf);

        return ppMbusObj;
    } else if (pos[1] == WRITE_CODE) {
        uint16_t reg = pos[2] << 8;
        reg += pos[3] << 0;
        reg -= REG_ADDR;

        uint16_t air = reg / CTRL_REG_LEN;// 每个空调占有3个寄存器
//        uint16_t idx = reg % CTRL_REG_LEN;// 空调占有哪个寄存器

        MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
            return NULL;
        }

        ppMbusObj[SWITCH]->localPoint = ACD_SWITCH;
        ppMbusObj[SPEED]->localPoint = ACD_SPEED;
        ppMbusObj[MODE]->localPoint = ACD_MODE;
        ppMbusObj[TEMP]->localPoint = ACD_TEMP;

        ppMbusObj[SWITCH]->value = pos[4] >> 14;
        if (ppMbusObj[SWITCH]->value == 1) {
            ppMbusObj[SWITCH]->value = 0;
        } else if (ppMbusObj[SWITCH]->value == 2) {
            ppMbusObj[SWITCH]->value = 1;
        }

        ppMbusObj[TEMP]->value = (pos[5] & 0x1F) - 15;

        char speedBuf[64] = {0};
        /* tuya 枚举值: auto, low, middle, high */
        // acd: low1, middle2, high4, auto8
        ppMbusObj[SPEED]->value = ((pos[4] & 1) << 4) + (pos[5] >> 5);
        if (ppMbusObj[SPEED]->value == 8) {
            ppMbusObj[SPEED]->value = 0;
            strcpy (speedBuf, "auto");
        } else if (ppMbusObj[SPEED]->value == 1) {
            ppMbusObj[SPEED]->value = 1;
            strcpy (speedBuf, "low");
        } else if (ppMbusObj[SPEED]->value == 2) {
            ppMbusObj[SPEED]->value = 2;
            strcpy (speedBuf, "mid");
        } else if (ppMbusObj[SPEED]->value == 3) {
            ppMbusObj[SPEED]->value = 4;
            strcpy (speedBuf, "high");
        }

        // tuya :枚举值: auto, cold, hot, wet, wind
        // acd: wind 4, hot 8, cold 1, wet 2, auto 16
        ppMbusObj[MODE]->value = (pos[4] & 0x3E) >> 1;

        char modeBuf[64] = {0};
        if (ppMbusObj[MODE]->value == 4) {
            ppMbusObj[MODE]->value = 4;
            strcpy (modeBuf, "wind");
        } else if (ppMbusObj[MODE]->value == 8) {
            ppMbusObj[MODE]->value = 2;
            strcpy (modeBuf, "hot");
        } else if (ppMbusObj[MODE]->value == 1) {
            ppMbusObj[MODE]->value = 1;
            strcpy (modeBuf, "cold");
        } else if (ppMbusObj[MODE]->value == 2) {
            ppMbusObj[MODE]->value = 3;
            strcpy (modeBuf, "wet");
        } else if (ppMbusObj[MODE]->value == 16) {
            ppMbusObj[MODE]->value = 0;
            strcpy (modeBuf, "auto");
        }
        logTrc ( "read mbus acd%d switch(%s) mode(%s) temp(%d) speed(%s)", air, (ppMbusObj[SWITCH]->value ? "on" : "off"), modeBuf, ppMbusObj[TEMP]->value, speedBuf);

        return ppMbusObj;
    }

    return NULL;
}

const MbusIf Mbus_Acd_TG_A21MC= {
    .build = build,
    .parse = parse};

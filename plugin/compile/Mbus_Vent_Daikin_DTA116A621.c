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
 * producct id: ft2p6tky
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
39    开关             switch          rw         bool
28    风速设置          wind            rw         enum       枚举值: low, high
*/

#define READ_CODE 0x4
#define WRITE_CODE 0x6

#define REG_ADDR 0x07D0
#define STATUS_REG_LEN 6
#define CTRL_REG_LEN 3

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

#define MODBUS_MAX_LEN 256
#define MODBUS_MIN_LEN 7

#define CLOUD_VENT_SWTICH 39
#define CLOUD_VENT_SPEED 28

typedef enum {
    SWITCH = 0,
    SPEED = 1,
    DIR = 2,
    MAX_IDX
} Idx_e;

static MbusObj_t MbusObj[16][3];
//static int SpeedHigh = 0;
//static int SpeedLow = 0;

char *logTime (void) {
    time_t rawtime;
    struct tm *info;
    static char buffer[80];

    time (&rawtime);
    info = localtime (&rawtime);
    strftime (buffer, 80, "%Y-%m-%d %H:%M:%S", info);

    return buffer;
}

/* 注意:调度方做内存释放 */
Msg_t *build (uint8_t devId, uint16_t regAddr, uint16_t groupAddr, UNUSED const void *pvMbusNode, ty_obj_dp_s *pObj) {
    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ( COLOR_RED "mbus Plugin control alloc fail!\n" COLOR_CLEAR);
        return NULL;
    }
    pMsg->pBuf = (uint8_t *) calloc (8, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ( COLOR_RED "mbus Plugin control alloc fail!\n" COLOR_CLEAR);
        return NULL;
    }
    pMsg->len = 8;
    *((uint8_t *) (pMsg->pBuf + 0)) = devId;
    if (!pObj) {
        *((uint8_t *) (pMsg->pBuf + 1)) = READ_CODE;
        *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + STATUS_REG_LEN * regAddr);
        *((uint8_t *) (pMsg->pBuf + 4)) = 0x0;
        *((uint8_t *) (pMsg->pBuf + 5)) = 0x1;// read one register
        uint16_t tmp = crc16 (pMsg->pBuf, 6);
        *((uint16_t *) (pMsg->pBuf + 6)) = htons (tmp);

        return pMsg;
    }
    *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CODE;

    switch (pObj->dpid) {
        case CLOUD_VENT_SWTICH: /* Switch */
            *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + CTRL_REG_LEN * regAddr + 0);
            *((uint8_t *) (pMsg->pBuf + 4)) = MbusObj[groupAddr][SPEED].rcvValue << 4;// 风速
            *((uint8_t *) (pMsg->pBuf + 4)) += MbusObj[groupAddr][DIR].rcvValue << 0; // 风向
            *((uint8_t *) (pMsg->pBuf + 5)) = pObj->value.dp_bool;               // 开机
            break;
        case CLOUD_VENT_SPEED: /* Speed */
            *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + CTRL_REG_LEN * regAddr + 0);
            if (pObj->value.dp_enum == 0) {// tuya.speed.low
                *((uint8_t *) (pMsg->pBuf + 4)) = 0x10;
            } else if (pObj->value.dp_enum == 1) {// tuya.speed.high
                *((uint8_t *) (pMsg->pBuf + 4)) = 0x50;
            }

            *((uint8_t *) (pMsg->pBuf + 4)) += MbusObj[groupAddr][DIR].rcvValue << 0;// 风向
            *((uint8_t *) (pMsg->pBuf + 5)) = MbusObj[groupAddr][SWITCH].rcvValue;   // 开机
            break;
        default:
            logErr ( COLOR_RED "[mbus Plugin] localPoint(%d) not be supported!\n" COLOR_CLEAR, pObj->dpid);
            break;
    }
    uint16_t tmp = crc16 ((uint8_t *) pMsg->pBuf, 6);
    *((uint16_t *) (pMsg->pBuf + 6)) = htons (tmp);

    return pMsg;
}

MbusObj_t **parse (uint8_t devId, uint16_t regAddr, UNUSED const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    if (!pBuf || (len < MODBUS_MIN_LEN)) {
        logErr ( COLOR_RED "[mbus Plugin] report() invalid parameter!\n" COLOR_CLEAR);
        return NULL;
    }

    char szlog[128] = {0};
    sprintf (szlog, "[mbus daikin vent parse] len=%d", len);
    LogTraceHex(szlog, (uint8_t *)pBuf, len);

    uint16_t key1 = devId * 256 + READ_CODE;
    uint16_t key2 = devId * 256 + WRITE_CODE;
    uint8_t invalid = 0;
    uint8_t *pos = memmatch ((uint8_t *) pBuf, key1, len);
    if (!pos || (len + pBuf - pos < MODBUS_MIN_LEN)) {
        invalid++;
        pos = memmatch ((uint8_t *) pBuf, key2, len);
        if (!pos || (len + pBuf - pos < MODBUS_MIN_LEN)) {
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

        // 01 04 0a 31 a9 42 02 00 aa 00 00 01 0e
        MbusObj[regAddr][SWITCH].rcvValue = pos[4] & 0x1;      // 开机
        MbusObj[regAddr][DIR].rcvValue = pos[3] & 0xF;         // 风向
        MbusObj[regAddr][SPEED].rcvValue = (pos[3] >> 4) & 0x7;// 风速

        ppMbusObj[SWITCH]->localPoint = VENT_SWITCH;
        ppMbusObj[DIR]->localPoint = VENT_WIND_DIR;
        ppMbusObj[SPEED]->localPoint = VENT_SPEED;

        ppMbusObj[SWITCH]->value = MbusObj[regAddr][SWITCH].rcvValue;

        char speedBuf[64] = {0};
        if (MbusObj[regAddr][SPEED].rcvValue <= 3) {// dajing.speed.middle
            MbusObj[regAddr][SPEED].rcvValue = 1;
            ppMbusObj[SPEED]->value = 0;// tuya.speed.middle
            strcpy (speedBuf, "middle");
        } else {// dajing.speed.high
            MbusObj[regAddr][SPEED].rcvValue = 5;
            ppMbusObj[SPEED]->value = 1;// tuya.speed.high
            strcpy (speedBuf, "high");
        }
        logTrc ( "read mbus vent%d %s speed=%s", regAddr, (MbusObj[regAddr][SWITCH].value ? "on" : "off"), speedBuf);

        return ppMbusObj;
    } else if (pos[1] == WRITE_CODE) {
        uint16_t reg = pos[2] << 8;
        reg += pos[3] << 0;
        reg -= REG_ADDR;
        //        01 06 07 e2 10 00 25 48
        uint16_t air = reg / 3;// 每个空调占有3个寄存器
        uint16_t idx = reg % 3;// 空调占有哪个寄存器

        MbusObj_t **ppMbusObj = NewMbusObjs (2);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
            return NULL;
        }

        if (idx == 0) {// dajing.switch and speed
            ppMbusObj[0]->value = MbusObj[air][SWITCH].rcvValue = pos[5] & 0x1;
            MbusObj[air][SPEED].rcvValue = (pos[4] >> 4) & 0x7;
            logTrc ( "write mbus vent%d switch-rcvValue=%d speed-rcvValue=%d", regAddr, MbusObj[air][SWITCH].rcvValue, MbusObj[air][SPEED].rcvValue);
            char speedBuf[64] = {0};
            if (MbusObj[air][SPEED].rcvValue <= 3) {// dajing.speed.middle
                MbusObj[air][SPEED].rcvValue = 1;
                ppMbusObj[1]->value = 0;
                strcpy (speedBuf, "middle");
            } else {// dajing.speed.high
                MbusObj[air][SPEED].rcvValue = 5;
                ppMbusObj[1]->value = 1;
                strcpy (speedBuf, "high");
            }
            logTrc ( "write mbus vent%d %s speed=%s", regAddr, (ppMbusObj[0]->value ? "on" : "off"), speedBuf);
            ppMbusObj[0]->localPoint = VENT_SWITCH;
            ppMbusObj[1]->localPoint = VENT_SPEED;
        }

        return ppMbusObj;
    }

    return NULL;
}

const MbusIf Mbus_Vent_Daikin_DTA116A621 = {
    .build = build,
    .parse = parse};

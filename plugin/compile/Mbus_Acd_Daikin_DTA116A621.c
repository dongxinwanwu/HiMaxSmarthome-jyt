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
 * PID: c4hfcrxbkq1pfs9i
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关            switch            rw        bool
2     温度设置         temp_set          rw        value     数值范围: 16-32, 间距: 1, 倍数: 0, 单位:
3     当前温度         temp_current      ro        value     数值范围: -10-65, 间距: 1, 倍数: 0, 单位:
4     工作模式         mode              rw        enum      枚举值: (auto), cold, hot, wet, wind, (sleep, refreshing)
5     风速            fan_speed_enum    rw        enum      枚举值: auto, superlow, low, middle, high, superhigh
101   故障码          battery_fault_xna  ro       string
*/

/*
 * RS485: 0 auro, 1 ll, 2 l, 3 m, 4 h, 5 hh
 * */

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
#define MODBUS_MIN_LEN 8

typedef enum {
    SWITCH = 0,
    TEMP = 1,
    MODE = 2,
    SPEED = 3,
    ENV_TEMP = 4,
    ERROR_CODE = 5,
    DIR = 6,
    MODE_OUTDOOR = 7,
    AUTHORITY = 8,
    MAX_IDX
} Idx_e;

typedef struct {
    uint8_t value;
    uint8_t symble;
} DaiKinFault_t;

static MbusObj_t MbusObj[16][MAX_IDX];// todo: 库升请内存，地址给到主程序。

uint8_t fault2[32] = {'0', 'A', 'C', 'E', 'H', 'F', 'J', 'L', 'P', 'U', '9', '8', '7', '6', '5', '4', '3', '2', '1', 'G', 'K', 'M', 'N', 'R', 'T', 'V', 'W', 'X', 'Y', 'Z', '*', ' '};
uint8_t fault1[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};

/* 注意:调度方做内存释放 */
Msg_t *build (uint8_t devId, uint16_t regAddr, uint16_t groupAddr, UNUSED const void *pvMbusNode, ty_obj_dp_s *pObj) {
    logTrc ("mbus acd daikin build: devId=%d, regAddr=%d, groupAddr=%d", devId, regAddr, groupAddr);
    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        logErr ("mbus Plugin control alloc fail!");
        return NULL;
    }
    pMsg->pBuf = (uint8_t *) calloc (8, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        logErr ("mbus Plugin control alloc fail!");
        return NULL;
    }
    pMsg->len = 8;
    *((uint8_t *) (pMsg->pBuf + 0)) = devId;
    if (!pObj) {
        *((uint8_t *) (pMsg->pBuf + 1)) = READ_CODE;
        *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + STATUS_REG_LEN * regAddr);
        *((uint8_t *) (pMsg->pBuf + 4)) = 0x0;
        *((uint8_t *) (pMsg->pBuf + 5)) = 0x5;
        uint16_t crc = crc16 (pMsg->pBuf, 6);
        *((uint16_t *) (pMsg->pBuf + 6)) = htons (crc);

        return pMsg;
    }
    *((uint8_t *) (pMsg->pBuf + 1)) = WRITE_CODE;

    switch (pObj->dpid) {
        case 1: /* Switch */
            *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + CTRL_REG_LEN * regAddr + 0);
            *((uint8_t *) (pMsg->pBuf + 4)) = (MbusObj[groupAddr][SPEED].rcvValue & 0x0F) << 4;// 风速
            *((uint8_t *) (pMsg->pBuf + 4)) += MbusObj[groupAddr][DIR].rcvValue & 0x0F;        // 风向
            *((uint8_t *) (pMsg->pBuf + 5)) = (pObj->value.dp_bool ? 1 : 0);                   // 开机
            *((uint8_t *) (pMsg->pBuf + 5)) += 6 << 4;
            // *((uint8_t *) (pMsg->pBuf + 5)) += MbusObj[groupAddr][MODE].rcvValue << 4;

            break;
        case 2: /* Temp */
            *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + CTRL_REG_LEN * regAddr + 2);
            *((uint16_t *) (pMsg->pBuf + 4)) = htons (pObj->value.dp_value * 10);

            break;
        case 4: /* Mode */
            // tuya :枚举值: auto, cold, hot, wet, wind
            // daikin: wind 0, hot 1, cold 2, wet 7, auto 3
            *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + CTRL_REG_LEN * regAddr + 1);
            *((uint8_t *) (pMsg->pBuf + 4)) = MbusObj[regAddr][MODE_OUTDOOR].rcvValue;// 制冷模式风速控制不响应   // 外机模式 old:0

            if (pObj->value.dp_enum == 1) {// tuya.mode.cold
                *((uint8_t *) (pMsg->pBuf + 4)) = 2;
                *((uint8_t *) (pMsg->pBuf + 5)) = 2;
                logTrc ("mbus acd%d daikin build: tuya.mode.cold=%d, daikin.mode=%d", regAddr, pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
                if ((MbusObj[regAddr][AUTHORITY].rcvValue != 2) && (MbusObj[regAddr][MODE_OUTDOOR].rcvValue != 2)) { // 无冷热选择权
                    free (pMsg->pBuf);
                    free (pMsg);
                    logErr (COLOR_RED"mbus acd%d daikin set cold mode: No hot or cold option!"COLOR_CLEAR, regAddr);
                    return NULL;
                }
            } else if (pObj->value.dp_enum == 3) {// tuya.mode.wet
                *((uint8_t *) (pMsg->pBuf + 5)) = 7;
                logTrc ("mbus acd%d daikin build: tuya.mode.wet=%d, daikin.mode=%d", regAddr, pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 4) {// tuya.mode.wind
                *((uint8_t *) (pMsg->pBuf + 4)) = 0;
                *((uint8_t *) (pMsg->pBuf + 5)) = 0;
                logTrc ("mbus acd%d daikin build: tuya.mode.wind=%d, daikin.mode=%d", regAddr, pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            } else if (pObj->value.dp_enum == 2) {// tuya.mode.hot
                *((uint8_t *) (pMsg->pBuf + 4)) = 1;
                *((uint8_t *) (pMsg->pBuf + 5)) = 1;
                logTrc ("mbus acd%d daikin build: tuya.mode.hot=%d, daikin.mode=%d", regAddr, pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
                if ((MbusObj[regAddr][AUTHORITY].rcvValue != 2) && (MbusObj[regAddr][MODE_OUTDOOR].rcvValue != 1)) {
                    free (pMsg->pBuf);
                    free (pMsg);
                    logErr ("COLOR_REDmbus acd%d daikin set hot mode: No hot or cold option!"COLOR_CLEAR, regAddr);
                    return NULL;
                }
            } else if (pObj->value.dp_enum == 0) {// tuya.mode.auto
                *((uint8_t *) (pMsg->pBuf + 5)) = 3;
                logTrc ("mbus acd%d daikin build: tuya.mode.auto=%d, daikin.mode=%d", regAddr, pObj->value.dp_enum, *((uint8_t *) (pMsg->pBuf + 5)));
            }

            break;
        case 5: /* Speed */
            *((uint16_t *) (pMsg->pBuf + 2)) = htons (REG_ADDR + CTRL_REG_LEN * regAddr + 0);
            *((uint8_t *) (pMsg->pBuf + 4)) = pObj->value.dp_enum << 4;
            *((uint8_t *) (pMsg->pBuf + 4)) += MbusObj[groupAddr][DIR].rcvValue << 0;// 风向

            *((uint8_t *) (pMsg->pBuf + 5)) = 1;
            MbusObj[groupAddr][SWITCH].rcvValue = 1;
            *((uint8_t *) (pMsg->pBuf + 5)) += 6 << 4;// 模式

            break;
        default:
            logErr ("[mbus Plugin] localPoint(%d) not be supported!", pObj->dpid);
            break;
    }
    uint16_t tmp = crc16 ((uint8_t *) pMsg->pBuf, 6);
    *((uint16_t *) (pMsg->pBuf + 6)) = htons (tmp);

    return pMsg;
}

MbusObj_t **parse (uint8_t devId, uint16_t regAddr, UNUSED const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    if (!pBuf || (len < MODBUS_MIN_LEN)) {
        logErr ("[mbus Plugin] report() invalid parameter! pBuf=%p, len=%d", pBuf, len);
        return NULL;
    }

    uint16_t key1 = devId * 256 + READ_CODE;
    uint16_t key2 = devId * 256 + WRITE_CODE;
    uint8_t invalid = 0;
    char szlog[128] = {0};
    sprintf (szlog, "[mbus daikin acd parse] len=%d", len);
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
        logErr ("mbus plugin report: invalid command");
        return NULL;
    }

    if (pos[1] == READ_CODE) {
        MbusObj_t **ppMbusObj = NewMbusObjs (MAX_IDX - 2);
        if (!ppMbusObj) {
            logErr ("[mbus Plugin] parse() NewMbusObjs() failed!");
            return NULL;
        }

        // 01 04 0a 31 a9 42 02 00 aa 00 00 01 0e
        MbusObj[regAddr][SWITCH].rcvValue = pos[4] & 0x1;          // 开机
        MbusObj[regAddr][DIR].rcvValue = pos[3] & 0xF;             // 风向
        MbusObj[regAddr][SPEED].rcvValue = (pos[3] >> 4) & 0x7;    // 风速
        MbusObj[regAddr][MODE].rcvValue = pos[6] & 0xF;            // 内机模式
        MbusObj[regAddr][MODE_OUTDOOR].rcvValue = pos[5] & 0x3;    // 外机模式
        MbusObj[regAddr][MODE_OUTDOOR].rcvValue = pos[5] & 0x3;    // 外机模式
        MbusObj[regAddr][AUTHORITY].rcvValue = (pos[5] >> 6) & 0x3;// 冷热选择权
        MbusObj[regAddr][TEMP].rcvValue = pos[7] << 8;             // 设置温度
        MbusObj[regAddr][TEMP].rcvValue += pos[8] << 0;            // 设置温度
        MbusObj[regAddr][ENV_TEMP].rcvValue = pos[11] << 8;        // 环境温度
        MbusObj[regAddr][ENV_TEMP].rcvValue += pos[12] << 0;       // 环境温度
        MbusObj[regAddr][ERROR_CODE].rcvValue = pos[10];           // 故障码

        ppMbusObj[SWITCH]->localPoint = ACD_SWITCH;
        ppMbusObj[SPEED]->localPoint = ACD_SPEED;
        ppMbusObj[MODE]->localPoint = ACD_MODE;
        ppMbusObj[TEMP]->localPoint = ACD_TEMP;
        ppMbusObj[ENV_TEMP]->localPoint = ACD_ENV_TEMP;
        ppMbusObj[ERROR_CODE]->localPoint = ACD_ERROR_CODE;

        ppMbusObj[SWITCH]->value = MbusObj[regAddr][SWITCH].rcvValue;
        ppMbusObj[TEMP]->value = MbusObj[regAddr][TEMP].rcvValue / 10;
        ppMbusObj[ENV_TEMP]->value = MbusObj[regAddr][ENV_TEMP].rcvValue / 10;
        uint8_t F2 = MbusObj[regAddr][ERROR_CODE].rcvValue >> 4;
        uint8_t F1 = MbusObj[regAddr][ERROR_CODE].rcvValue & 0x0F;
        ppMbusObj[ERROR_CODE]->value = fault2[F2];
        ppMbusObj[ERROR_CODE]->value <<= 8;
        ppMbusObj[ERROR_CODE]->value += fault1[F1];

        char modeBuf[64] = {0};
        char outModeBuf[64] = {0};
        ppMbusObj[SPEED]->value = MbusObj[regAddr][SPEED].rcvValue;

        /* tuya: auto 0, cold 1, hot 2, wet 3, wind 4 */
        // daikin: wind 0, hot 1, cold 2, wet 7, auto 3
        if (MbusObj[regAddr][MODE].rcvValue == 0) {// dajing.mode.wind
            ppMbusObj[MODE]->value = 4;
            strcpy (modeBuf, "wind");
        } else if (MbusObj[regAddr][MODE].rcvValue == 1) {// dajing.mode.hot
            ppMbusObj[MODE]->value = 2;
            strcpy (modeBuf, "hot");
        } else if (MbusObj[regAddr][MODE].rcvValue == 2) {// dajing.mode.cold
            ppMbusObj[MODE]->value = 1;
            strcpy (modeBuf, "cold");
        } else if (MbusObj[regAddr][MODE].rcvValue == 7) {// dajing.mode.wet
            ppMbusObj[MODE]->value = 3;
            strcpy (modeBuf, "wet");
        } else if (MbusObj[regAddr][MODE].rcvValue == 3) {// dajing.mode.auto
            ppMbusObj[MODE]->value = 0;
            strcpy (modeBuf, "auto");
        }

        if (MbusObj[regAddr][MODE_OUTDOOR].rcvValue == 0) {
            strcpy (outModeBuf, "wind");
        } else if (MbusObj[regAddr][MODE_OUTDOOR].rcvValue == 1) {
            strcpy (outModeBuf, "hot");
        } else if (MbusObj[regAddr][MODE_OUTDOOR].rcvValue == 2) {
            strcpy (outModeBuf, "cold");
        } else {
            strcpy (outModeBuf, "unknown");
        }
        logTrc ("read mbus acd%d switch(%s) mode(out-%s:in-%s) temp(%d) env(%d) speed(L%d) authority(%s)",
                regAddr, (ppMbusObj[SWITCH]->value ? "on" : "off"), outModeBuf, modeBuf, ppMbusObj[TEMP]->value, ppMbusObj[ENV_TEMP]->value, ppMbusObj[SPEED]->value,
                (MbusObj[regAddr][AUTHORITY].rcvValue == 0 ? "unknown" : (MbusObj[regAddr][AUTHORITY].rcvValue == 1 ? "no" : "yes")));

        return ppMbusObj;
    } else if (pos[1] == WRITE_CODE) {
        uint16_t reg = pos[2] << 8;
        reg += pos[3] << 0;
        reg -= REG_ADDR;

        uint16_t air = reg / 3;// 每个空调占有3个寄存器
        uint16_t idx = reg % 3;// 空调占有哪个寄存器

        MbusObj_t **ppMbusObj = NewMbusObjs (2);
        if (!ppMbusObj) {
            logErr ("[mbus Plugin] parse() NewMbusObjs() failed!");
            return NULL;
        }

        if (idx == 0) {// dajing.switch and speed
            // 01 06 07 d6 50 11 95 4a
            ppMbusObj[0]->value = MbusObj[air][SWITCH].rcvValue = pos[5] & 0x1;
            ppMbusObj[1]->value = MbusObj[air][SPEED].rcvValue = (pos[4] >> 4) & 0x7;
            logTrc ("write mbus acd%d switch=%s speed=L%d", air, (ppMbusObj[0]->value ? "on" : "off"), ppMbusObj[1]->value);
            ppMbusObj[0]->localPoint = ACD_SWITCH;
            ppMbusObj[1]->localPoint = ACD_SPEED;
        } else if (idx == 1) {// dajing.mode
            MbusObj[air][MODE].rcvValue = pos[5] & 0xf;
            MbusObj[air][MODE_OUTDOOR].rcvValue = pos[4] & 0x3;// 外机模式
            /* tuya: auto 0, cold 1, hot 2, wet 3, wind 4 */
            // daikin: wind 0, hot 1, cold 2, wet 7, auto 3
            if (MbusObj[air][MODE].rcvValue == 0) {// dajing.mode.wind
                ppMbusObj[0]->value = 4;
                logTrc ("write mbus acd%d mode=wind", air);
            } else if (MbusObj[air][MODE].rcvValue == 1) {// dajing.mode.hot
                ppMbusObj[0]->value = 2;
                logTrc ("write mbus acd%d mode=hot", air);
            } else if (MbusObj[air][MODE].rcvValue == 2) {// dajing.mode.cold
                ppMbusObj[0]->value = 1;
                logTrc ("write mbus acd%d mode=cold", air);
            } else if (MbusObj[air][MODE].rcvValue == 7) {// dajing.mode.wet
                ppMbusObj[0]->value = 3;
                logTrc ("write mbus acd%d mode=wet", air);
            } else if (MbusObj[air][MODE].rcvValue == 3) {// dajing.mode.auto
                ppMbusObj[0]->value = 0;
                logTrc ("write mbus acd%d mode=auto", air);
            }
            ppMbusObj[0]->localPoint = ACD_MODE;
            ppMbusObj[1] = NULL;
        } else if (idx == 2) {// dajing.temp
            MbusObj[air][TEMP].rcvValue = ntohs (*(uint16_t *) &pos[4]);
            ppMbusObj[0]->value = MbusObj[air][TEMP].rcvValue / 10;
            ppMbusObj[0]->localPoint = ACD_TEMP;
            ppMbusObj[1] = NULL;
            logTrc ("write mbus acd%d temp=%d", air, ppMbusObj[0]->value);
        }

        return ppMbusObj;
    }

    return NULL;
}

int init (void *pUser) {

    return 0;
}

const MbusIf Mbus_Acd_Daikin_DTA116A621 = {
    .build = build,
    .parse = parse,
    .init =init
};

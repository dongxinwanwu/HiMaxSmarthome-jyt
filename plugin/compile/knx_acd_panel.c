#include "misc_dev_plugins.h"
#include <stdio.h>
#include "zlog.h"
#include "main.h"

/*
 * PID: 6ginsnhouasjj7ai
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关            switch            rw        bool
2     温度设置         temp_set          rw        value     数值范围: 16-30, 间距: 1, 倍数: 0, 单位:
3     当前温度         temp_current      ro        value     数值范围: -20-50, 间距: 1, 倍数: 0, 单位:
4     工作模式         mode              rw        enum      枚举值: cold, wet, wind, hot
5     风速            fan_speed_enum    rw        enum      枚举值: auto, high, middle, low
*/

/*
 * 中弘KNX空调集控器
 * 开关：  0关, 1开
 * 模式：  8制热, 4通风, 2除湿, 1制冷
 * 风速：  4低速, 2中速, 1高速, 0自动
 * 温度：  16-30°C
 * */

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

short ConvetTemp2Uint16(float temp) {
    for (uint8_t e = 0; e <= 15; e++) {
        short m = (short) (temp / (1 << e) / 0.01);
        if (m > -2048 && m <= 2047) {
            if (m >= 0) {
                return (m | (e << 11));
            } else {
                m = (m & 0x87ff) | (e << 11);
                return m;
            }
        }
    }
    return 0xffff;
}

float ConvetUint162Temp(short value) {
    float temp;
    short e, m;// , n;

    e = (value & 0x7800) >> 11;
    if (0 == (value & 0x8000)) {
        m = value & 0x07ff;
        temp = (float) (0.01 * m * (1 << e));
    } else {
        m = value & 0x07ff;
        //        n = m | 0xf800;
        temp = (float) (0.01 * m * (1 << e));
    }
    return temp;
}

int knx_dev_control(const ty_obj_dp_s *pObj, KnxObj_t *pCtrl) {
    if (!pObj || !pCtrl) {
        logErr ( "knx acd panel plugin param error!");
        return -1;
    }
    logTrc ( "knx acd panel plugin: group:%s, point type=%d, value=%d", pCtrl->addr, pCtrl->localPoint,
                pObj->value.dp_value);
    switch (pCtrl->localPoint) {
        case ACD_SWITCH:
            pCtrl->value = pObj->value.dp_bool;
            logTrc ( "knx acd pannel ctrl switch=%s", (pCtrl->value ? "on" : "off"));
            break;
        case ACD_TEMP:
            pCtrl->value = ConvetTemp2Uint16(pObj->value.dp_value);
            logTrc ( "knx acd pannel ctrl temp=%d", pCtrl->value);
            break;
        case ACD_MODE:
            if (pObj->value.dp_enum == 0) { /* cold */
                pCtrl->value = 1;
                logTrc ( "set knx acd pannel cold mode");
            } else if (pObj->value.dp_enum == 1) { /* wet */
                pCtrl->value = 2;
                logTrc ( "set knx acd pannel wet mode");
            } else if (pObj->value.dp_enum == 2) { /* wind */
                pCtrl->value = 4;
                logTrc ( "set knx acd pannel wind mode");
            } else if (pObj->value.dp_enum == 3) { /* hot */
                pCtrl->value = 8;
                logTrc ( "set knx acd pannel hot mode");
            }
            break;
        case ACD_SPEED:
            if (pObj->value.dp_enum == 0) { /* auto */
                pCtrl->value = 0;
                logTrc ( "set knx acd pannel auto speed");
            } else if (pObj->value.dp_enum == 1) { /* high */
                pCtrl->value = 1;
                logTrc ( "set knx acd pannel high speed");
            } else if (pObj->value.dp_enum == 2) { /* middle */
                pCtrl->value = 2;
                logTrc ( "set knx acd pannel middle speed");
            } else if (pObj->value.dp_enum == 3) { /* low */
                pCtrl->value = 4;
                logTrc ( "set knx acd pannel low speed");
            }

            break;
        default:
            logErr ( "knx acd panel plugin: knx point(%d) not be supported!", pCtrl->localPoint);
            break;
    }

    return 0;
}

int knx_dev_report(const KnxObj_t *pStatus, ty_obj_dp_s *pObj) {
    if (!pObj || !pStatus) {
        logTrc ( "knx acd panel report plugin param error!");
        return -1;
    }
    logTrc ( "knx acd report plugin: groupAddr:%s, point type:%d, value=0x%04x", pStatus->addr,
                pStatus->localPoint, pStatus->value);
    switch (pStatus->localPoint) {
        case ACD_SWITCH_STATUS:
            pObj->value.dp_bool = pStatus->value;
            pObj->type = PROP_BOOL;
            logTrc ( "knx acd pannel report switch=%s", (pStatus->value ? "on" : "off"));
            break;

        case ACD_TEMP_STATUS:
            pObj->value.dp_value = ConvetUint162Temp(pStatus->value);
            pObj->type = PROP_VALUE;
            logTrc ( "knx acd pannel report temp=%d", pObj->value.dp_value);
            break;

        case ACD_MODE_STATUS:
            pObj->type = PROP_ENUM;
            if (pStatus->value == 1) {
                pObj->value.dp_value = 0;
                logTrc ( "knx acd pannel report mode=cold");
            } else if (pStatus->value == 2) {
                pObj->value.dp_value = 1;
                logTrc ( "knx acd pannel report mode=wet");
            } else if (pStatus->value == 4) {
                pObj->value.dp_value = 2;
                logTrc ( "knx acd pannel report mode=wind");
            } else if (pStatus->value == 8) {
                pObj->value.dp_value = 3;
                logTrc ( "knx acd pannel report mode=hot");
            }
            break;
        case ACD_SPEED_STATUS:
            if (pStatus->value == 0) { /* auto */
                pObj->value.dp_value = 0;
                logTrc ( "knx acd pannel report speed=auto");
            } else if (pStatus->value == 1) { /* high */
                pObj->value.dp_value = 1;
                logTrc ( "knx acd pannel report speed=high");
            } else if (pStatus->value == 2) { /* middle */
                pObj->value.dp_value = 2;
                logTrc ( "knx acd pannel report speed=middle");
            } else if (pStatus->value == 4) { /* low */
                pObj->value.dp_value = 3;
                logTrc ( "knx acd pannel report speed=low");
            }
            pObj->type = PROP_ENUM;
            break;
        case ACD_ENV_TEMP:
            pObj->value.dp_value = ConvetUint162Temp(pStatus->value);
            pObj->type = PROP_VALUE;
            logTrc ( "knx acd pannel env temp=%d", pObj->value.dp_value);
            break;
        default:
            logErr ( "knx acd report plugin: point(%d) not be supported!", pStatus->localPoint);
            break;
    }

    return 0;
}

const KnxIf knx_acd_panel = {
        .control = knx_dev_control,
        .report = knx_dev_report
};

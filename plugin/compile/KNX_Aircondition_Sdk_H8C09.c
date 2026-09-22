#include "misc_dev_plugins.h"
#include <stdio.h>
#include "zlog.h"
#include "main.h"
/*  */
/*
 * producct id: mvcwtzvg
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关            switch            rw        bool
2     温度设置         temp_set          rw        value     数值范围: 16-30, 间距: 1, 倍数: 0, 单位:
3     当前温度         temp_current      ro        value     数值范围: 0-55, 间距: 1, 倍数: 0, 单位:
4     工作模式         mode              rw        enum      枚举值: cold, hot, wet, wind
5     风速            fan_speed_enum    rw        enum      枚举值: auto, superlow, low, middle, high, superhigh
*/

/*
 * 模式：制冷0，制热1，通风2，除湿3，自动4
 * 风速:自动0,最低值1,低值2,中值3,高值4,最高值5
 * */

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

#define DEBUG 1

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
    short e, m;//, n;

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
        logErr ( "knx acd sdk report plugin param error!\n");
        return -1;
    }
    logTrc ( "knx sdk acd ctrl plugin: group:%s, point type=%d, value=%d\n", pCtrl->addr, pCtrl->localPoint, pObj->value.dp_value);

    switch (pCtrl->localPoint) {
        case ACD_SWITCH:
            pCtrl->value = pObj->value.dp_bool;
            logTrc ( "ctrl knx sdk acd switch=%s", (pCtrl->value ? "on" : "off"));
            break;
        case ACD_TEMP:
            pCtrl->value = ConvetTemp2Uint16(pObj->value.dp_value);
            logTrc ( "ctrl knx sdk acd temp=%d", pCtrl->value);
            break;
        case ACD_MODE:
            // knx 模式：制冷0，制热1，通风2，除湿3，自动4
            // 塗鴉 枚举值: cold, hot, wet, wind
            if (pObj->value.dp_enum == 0) {
                pCtrl->value = 0;
                logTrc ( "ctrl knx sdk acd mode=cold");
            } else if (pObj->value.dp_enum == 1) {
                pCtrl->value = 1;
                logTrc ( "ctrl knx sdk acd mode=hot");
            } else if (pObj->value.dp_enum == 2) {
                pCtrl->value = 3;
                logTrc ( "ctrl knx sdk acd mode=wet");
            } else if (pObj->value.dp_enum == 3) {
                pCtrl->value = 2;
                logTrc ( "ctrl knx sdk acd mode=wind");
            }
            break;
        case ACD_SPEED:
            // knx 风速:自动0,最低值1,低值2,中值3,高值4,最高值5
            // tuya 枚举值: auto, superlow, low, middle, high, superhigh
            pCtrl->value = pObj->value.dp_enum;
            logTrc ( "ctrl knx sdk acd speed=%d", pCtrl->value);

            break;
        default:
            logErr ( "knx acd sdk ctrl plugin: knx point(%d) not be supported!", pCtrl->localPoint);
            break;
    }

    return 0;
}

int knx_dev_report(const KnxObj_t *pStatus, ty_obj_dp_s *pObj) {
    if (!pObj || !pStatus) {
        logErr ( "knx acd sdk report plugin param error!");
        return -1;
    }
    logTrc ( "knx acd sdk report plugin: groupAddr:%s, point type:%d, value=0x%04x", pStatus->addr, pStatus->localPoint, pStatus->value);

    switch (pStatus->localPoint) {
        case ACD_SWITCH_STATUS:
            pObj->value.dp_bool = pStatus->value;
            pObj->type = PROP_BOOL;
            logTrc ( "report knx sdk acd switch=%s", (pObj->value.dp_bool ? "on" : "off"));
            break;

        case ACD_TEMP_STATUS:
            pObj->value.dp_value = ConvetUint162Temp(pStatus->value);
            pObj->type = PROP_VALUE;
            logTrc ( "report knx sdk acd temp=%d", pObj->value.dp_value);
            break;

        case ACD_MODE_STATUS:
            // knx 模式：制冷0，制热1，通风2，除湿3，自动4
            // 塗鴉 枚举值: cold, hot, wet, wind
            pObj->type = PROP_ENUM;
            if (pStatus->value == 0) {
                pObj->value.dp_value = 0;
                logTrc ( "report knx sdk acd mode=cold");
            } else if (pStatus->value == 1) {
                pObj->value.dp_value = 1;
                logTrc ( "report knx sdk acd mode=hot");
            } else if (pStatus->value == 2) {
                pObj->value.dp_value = 3;
                logTrc ( "report knx sdk acd mode=wind");
            } else if (pStatus->value == 3) {
                pObj->value.dp_value = 2;
                logTrc ( "report knx sdk acd mode=wet");
            }
            break;

        case ACD_SPEED_STATUS:
            // knx 风速:自动0,最低值1,低值2,中值3,高值4,最高值5
            // tuya 枚举值: auto, superlow, low, middle, high, superhigh
            pObj->value.dp_value = pStatus->value;
            pObj->type = PROP_ENUM;
            logTrc ( "report knx sdk speed=%d", pObj->value.dp_value);

            break;
        case ACD_ENV_TEMP:
            pObj->value.dp_value = ConvetUint162Temp(pStatus->value);
            pObj->type = PROP_VALUE;
            logTrc ( "report knx sdk acd env temp=%d", pObj->value.dp_value);
            break;
        default:
            logErr ( "knx acd sdk report plugin: point(%d) not be supported!", pStatus->localPoint);
            break;
    }

    return 0;
}

const KnxIf KNX_Aircondition_Sdk_H8C09 = {
        .control = knx_dev_control,
        .report = knx_dev_report};

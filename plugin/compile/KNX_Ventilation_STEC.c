#include "misc_dev_plugins.h"
#include <stdio.h>
#include "zlog.h"
#include "main.h"
/*
 * producct id: ab9ft3ytq7rs6sel
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
39     开关             switch          rw         bool
46    室内温度         temp_indoor      ro         value      数值范围: 0-50, 间距: 1, 倍数: 0, 单位:
28    风速设置          wind            rw         enum       枚举值: off, low, mid, high
*/

/*
 * 风速:低值1,高值2
 * */

#define DEBUG 1

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
        zlog_error(pLogCat, "knx vent sdk ctrl plugin param error!");
        return -1;
    }
    logTrc ( "knx vent sdk ctrl plugin: group:%s, point type=%d, value=%d\n", pCtrl->addr, pCtrl->localPoint, pObj->value.dp_value);

    switch (pCtrl->localPoint) {
        case VENT_SWITCH:
            pCtrl->value = pObj->value.dp_bool;
            logTrc ( "ctrl knx sdk vent switch=%s", (pCtrl->value ? "on" : "off"));
            break;

        case VENT_SPEED:
            // KNX 枚举值: off, low, mid, high
            // tuya 枚举值: off, low, mid, high
            pCtrl->value = pObj->value.dp_enum;
            break;
        default:
            logErr ( "knx vent sdk ctrl plugin: knx point(%d) not be supported!", pCtrl->localPoint);
            break;
    }

    return 0;
}

int knx_dev_report(const KnxObj_t *pStatus, ty_obj_dp_s *pObj) {
    if (!pObj || !pStatus) {
        logErr ( "knx vent sdk report plugin param error!");
        return -1;
    }
    logTrc ( "knx vent sdk report plugin: groupAddr:%s, point type:%d, value=0x%04x\n", pStatus->addr, pStatus->localPoint, pStatus->value);

    switch (pStatus->localPoint) {
        case VENT_SWITCH_STATUS:
            pObj->value.dp_bool = pStatus->value;
            pObj->type = PROP_BOOL;
            logTrc ( "report knx sdk vent switch=%s", (pObj->value.dp_bool ? "on" : "off"));
            break;
        case VENT_SPEED_STATUS:
            pObj->type = PROP_ENUM;
            pObj->value.dp_enum = pStatus->value;
            logTrc ( "report knx sdk vent speed=%d", pObj->value.dp_enum);
            break;
        case VENT_ENV_TEMP:
            pObj->value.dp_value = ConvetUint162Temp(pStatus->value);
            pObj->type = PROP_VALUE;
            logTrc ( "report knx sdk vent env temp=%d", pObj->value.dp_value);
            break;
        default:
            logErr ( "knx vent sdk report plugin: point(%d) not be supported!", pStatus->localPoint);
            break;
    }

    return 0;
}

const KnxIf KNX_Ventilation_STEC = {
        .control = knx_dev_control,
        .report = knx_dev_report};

#include "misc_dev_plugins.h"
#include <stdio.h>
#include "zlog.h"
#include "main.h"

/*
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关            switch            rw        bool
2     目标温度         temp_set         rw         value      数值范围: 0-40,间距: 1, 倍数: 0,单位: ℃
3     当前温度         temp_current     ro         value      数值范围: -20-50,间距: 1, 倍数: 0,单位: ℃
*/

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
    short e, m; // , n;

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
        logErr ( "knx heat ctrl plugin param error!");
        return -1;
    }
    logTrc ( "knx heat ctrl plugin: groupAddr:%s, point type:%d, value=0x%04x", pCtrl->addr, pCtrl->localPoint, pCtrl->value);
    switch (pCtrl->localPoint) {
        case HEAT_SWITCH:
            pCtrl->value = pObj->value.dp_bool;
            logTrc ( "knx heat pannel ctrl switch=%s", (pCtrl->value ? "on" : "off"));
            break;

        case HEAT_TEMP:
            pCtrl->value = ConvetTemp2Uint16(pObj->value.dp_value);
            logTrc ( "knx heat pannel ctrl temp=%d", pCtrl->value);
            break;

        default:
            logErr ( "knx heat ctrl plugin: knx point(%d) not be supported!", pCtrl->localPoint);
            break;
    }

    return 0;
}

int knx_dev_report(const KnxObj_t *pStatus, ty_obj_dp_s *pObj) {
    if (!pObj || !pStatus) {
        logErr ( "knx heat report plugin param error!");
        return -1;
    }
    logTrc ( "knx heat report plugin: groupAddr:%s, point type:%d, value=0x%04x", pStatus->addr, pStatus->localPoint, pStatus->value);

    switch (pStatus->localPoint) {
        case HEAT_SWITCH_STATUS:
            pObj->value.dp_bool = pStatus->value;
            pObj->type = PROP_BOOL;
            logTrc ( "knx heat pannel report switch=%s", (pStatus->value ? "on" : "off"));
            break;

        case HEAT_TEMP_STATUS:
            pObj->value.dp_value = ConvetUint162Temp(pStatus->value);
            pObj->type = PROP_VALUE;
            logTrc ( "knx heat pannel report temp=%d", pObj->value.dp_value);
            break;

        case HEAT_ENV_TEMP:
            pObj->value.dp_value = ConvetUint162Temp(pStatus->value);
            pObj->type = PROP_VALUE;
            logTrc ( "knx heat pannel report env temp=%d", pObj->value.dp_value);
            break;

        case HEAT_VALVE_STATUS:
            pObj->value.dp_bool = pStatus->value;
            pObj->type = PROP_BOOL;
            break;

        default:
            logErr ( "knx heat report plugin: point(%d) not be supported!", pStatus->localPoint);
            break;
    }

    return 0;
}

const KnxIf knx_heat_panel = {
        .control = knx_dev_control,
        .report = knx_dev_report
};

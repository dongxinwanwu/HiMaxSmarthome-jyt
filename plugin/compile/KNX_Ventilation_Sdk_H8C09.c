#include "misc_dev_plugins.h"
#include <stdio.h>
#include "zlog.h"
#include "main.h"
/*
 * producct id: ft2p6tky
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关             switch          rw         bool
9     室内温度         temp_indoor      ro         value      数值范围: -20-60, 间距: 1, 倍数: 0, 单位:
12    风速设置          wind            rw         enum       枚举值: low, high
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
            // KNX 枚举值: 低值1,高值2
            // tuya 枚举值: low, high
            pCtrl->value = pObj->value.dp_enum;
            if (pObj->value.dp_enum == 0) {
                pCtrl->value = 1;
            } else if (pObj->value.dp_enum == 1) {
                pCtrl->value = 2;
            }
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
            // KNX 枚举值: 低值1,高值2
            // tuya 枚举值: low, high
            if (pStatus->value == 1) {
                pObj->value.dp_enum = 0;
            } else if (pStatus->value == 2) {
                pObj->value.dp_enum = 1;
            }
            pObj->type = PROP_ENUM;
            logTrc ( "report knx sdk vent speed=%d", pObj->value.dp_value);
            break;
        default:
            logErr ( "knx vent sdk report plugin: point(%d) not be supported!", pStatus->localPoint);
            break;
    }

    return 0;
}

const KnxIf KNX_Ventilation_Sdk_H8C09 = {
        .control = knx_dev_control,
        .report = knx_dev_report};

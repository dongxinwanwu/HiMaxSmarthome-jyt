#include "misc_dev_plugins.h"
#include <stdio.h>
#include "zlog.h"
#include "main.h"

/*
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
39    开关             switch          rw         bool
101   风速设置          wind            rw         enum       枚举值: off, low, middle, high
*/

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4


int knx_dev_control(const ty_obj_dp_s *pObj, KnxObj_t *pCtrl) {
    if (!pObj || !pCtrl) {
        logErr ( "knx vent ctrl plugin param error");
        return -1;
    }
    logTrc ( "knx vent ctrl plugin: group:%s, point type=%d, value=%d\n", pCtrl->addr, pCtrl->localPoint, pObj->value.dp_value);
    switch (pCtrl->localPoint) {
        case VENT_SWITCH:
            pCtrl->value = pObj->value.dp_bool;
            logTrc ( "ctrl knx vent ctrl switch=%s", (pCtrl->value ? "on" : "off"));
            break;

        case VENT_SPEED:
            // KNX枚举值: 1低风、2中风、3高风
            // tuya 枚举值: off, low, middle, high
            pCtrl->value = pObj->value.dp_enum;

            if (pObj->value.dp_enum == 0) {
                logTrc ( "ctrl knx vent ctrl speed=%s", "off");
            } else if (pObj->value.dp_enum == 1) {
                logTrc ( "ctrl knx vent ctrl speed=%s", "low");
            } else if (pObj->value.dp_enum == 2) {
                logTrc ( "ctrl knx vent ctrl speed=%s", "middle");
            } else  if (pObj->value.dp_enum == 3) {
                logTrc ( "ctrl knx vent ctrl speed=%s", "high");
            }

            break;
        default:
            logErr ( "knx vent ctrl plugin: knx point(%d) not be supported!", pCtrl->localPoint);
            break;
    }

    return 0;
}

int knx_dev_report(const KnxObj_t *pStatus, ty_obj_dp_s *pObj) {
    if (!pObj || !pStatus) {
        logErr ( "knx vent report plugin param invalid!");
        return -1;
    }
    logTrc ( "knx vent report plugin: groupAddr:%s, point type:%d, value=0x%04x\n" COLOR_CLEAR, pStatus->addr, pStatus->localPoint, pStatus->value);
    switch (pStatus->localPoint) {
        case VENT_SWITCH_STATUS:
            pObj->value.dp_bool = pStatus->value;
            pObj->type = PROP_BOOL;
            logTrc ( "knx vent pannel report switch=%s", (pStatus->value ? "on" : "off"));
            break;

        case VENT_SPEED_STATUS:
            // KNX枚举值: 1低风、2中风、3高风
            pObj->value.dp_value = pStatus->value;
            pObj->type = PROP_ENUM;
            if (pObj->value.dp_value == 0) {
                logTrc ( "knx vent pannel report speed=%s", "off");
            } else if (pObj->value.dp_value == 1) {
                logTrc ( "knx vent pannel report speed=%s", "low");
            }else if (pObj->value.dp_value == 2) {
                logTrc ( "knx vent pannel report speed=%s", "middle");
            }else if (pObj->value.dp_value == 3) {
                logTrc ( "knx vent pannel report speed=%s", "high");
            }

            break;
        default:
            logErr ( "knx vent report plugin: point(%d) not be supported!", pStatus->localPoint);
            break;
    }

    return 0;
}

const KnxIf knx_vent_panel = {
        .control = knx_dev_control,
        .report = knx_dev_report
};

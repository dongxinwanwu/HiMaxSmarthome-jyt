#include "misc_dev_plugins.h"
#include <stdio.h>

/*  */
/*
 * DPID:6ginsnhouasjj7ai
DP    功能点名称        标识符            传输类型    数据类型    功能点属性
1     开关            switch            rw        bool
2     温度设置         temp_set          rw        value     数值范围: 16-32, 间距: 1, 倍数: 0, 单位:
3     当前温度         temp_current      ro        value     数值范围: -20-50, 间距: 1, 倍数: 0, 单位:
4     工作模式         mode              rw        enum      枚举值: cold, wet, wind, hot
5     风速            fan_speed_enum    rw        enum      枚举值: auto, high, middle, low
*/
/*
 * KNX ACD
 * 开关： 0/1
 * 温度： 16-32
 * 模式： auto0, cold1, wet2, wind4, hot8.
 * 风速： auto0, high1, middle2, low4
 * */

#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

#define DEBUG 1

short ConvetTemp2Uint16 (float temp) {
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

float ConvetUint162Temp (short value) {
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

int knx_dev_control (const ty_obj_dp_s *pObj, KnxObj_t *pCtrl) {
    if (!pObj || !pCtrl) {
        printf ("knx acd sdk report plugin param error!\n");
        return -1;
    }
    printf ("knx sdk acd ctrl plugin: group:%s, point type=%d, value=%d\n", pCtrl->addr, pCtrl->localPoint, pObj->value.dp_value);

    switch (pCtrl->localPoint) {
        case ACD_SWITCH:
            pCtrl->value = pObj->value.dp_bool;
#if defined(DEBUG) && (DEBUG == 1)
            printf ("ctrl knx sdk acd switch=%s\n", (pCtrl->value ? "on" : "off"));
#endif
            break;
        case ACD_TEMP:
            pCtrl->value = ConvetTemp2Uint16 (pObj->value.dp_value);
#if defined(DEBUG) && (DEBUG == 1)
            printf ("ctrl knx sdk acd temp=%d\n", pCtrl->value);
#endif
            break;
        case ACD_MODE:
            // knx 温控面板：auto0, cold1, wet2, wind4, hot8.
            // 塗鴉 枚举值: cold, wet, wind, hot
            if (pObj->value.dp_enum == 0) {
                pCtrl->value = 1;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("ctrl knx sdk acd mode=cold\n");
#endif
            } else if (pObj->value.dp_enum == 1) {
                pCtrl->value = 2;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("ctrl knx sdk acd mode=wet\n");
#endif
            } else if (pObj->value.dp_enum == 2) {
                pCtrl->value = 4;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("ctrl knx sdk acd mode=wind\n");
#endif
            } else if (pObj->value.dp_enum == 3) {
                pCtrl->value = 8;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("ctrl knx sdk acd mode=hot\n");
#endif
            }
            break;
        case ACD_SPEED:
            // knx 温控面板：auto0, high1, middle2, low4
            // tuya枚举值: auto, high, middle, low
            if (pObj->value.dp_enum == 0) {
                pCtrl->value = 0;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("ctrl knx sdk acd speed=auto\n");
#endif
            } else if (pObj->value.dp_enum == 1) {
                pCtrl->value = 1;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("ctrl knx sdk acd speed=high\n");
#endif
            } else if (pObj->value.dp_enum == 2) {
                pCtrl->value = 2;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("ctrl knx sdk acd speed=mid\n");
#endif
            } else if (pObj->value.dp_enum == 3) {
                pCtrl->value = 4;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("ctrl knx sdk acd speed=low\n");
#endif
            }
            break;
        default:
            printf ("knx acd sdk ctrl plugin: knx point(%d) not be supported!\n", pCtrl->localPoint);
            break;
    }

    return 0;
}

int knx_dev_report (const KnxObj_t *pStatus, ty_obj_dp_s *pObj) {
    if (!pObj || !pStatus) {
        printf ("knx acd sdk report plugin param error!\n");
        return -1;
    }
    printf (COLOR_RED "knx acd sdk report plugin: groupAddr:%s, point type:%d, value=0x%04x\n" COLOR_CLEAR, pStatus->addr, pStatus->localPoint, pStatus->value);

    switch (pStatus->localPoint) {
        case ACD_SWITCH_STATUS:
            pObj->value.dp_bool = pStatus->value;
            pObj->type = PROP_BOOL;
#if defined(DEBUG) && (DEBUG == 1)
            printf ("report knx sdk acd switch=%s\n", (pObj->value.dp_bool ? "on" : "off"));
#endif
            break;

        case ACD_TEMP_STATUS:
            pObj->value.dp_value = ConvetUint162Temp (pStatus->value);
            pObj->type = PROP_VALUE;
#if defined(DEBUG) && (DEBUG == 1)
            printf ("report knx sdk acd temp=%d\n", pObj->value.dp_value);
#endif
            break;

        case ACD_MODE_STATUS:
            // knx 温控面板：auto0, cold1, wet2, wind4, hot8.
            // 塗鴉 枚举值: cold, wet, wind, hot
            pObj->type = PROP_ENUM;
            if (pStatus->value == 0) {
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd mode=auto\n");
#endif
            } else if (pStatus->value == 1) {
                pObj->value.dp_value = 0;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd mode=cold\n");
#endif
            } else if (pStatus->value == 2) {
                pObj->value.dp_value = 1;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd mode=wet\n");
#endif
            } else if (pStatus->value == 4) {
                pObj->value.dp_value = 2;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd mode=wind\n");
#endif
            } else if (pStatus->value == 8) {
                pObj->value.dp_value = 3;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd mode=hot\n");
#endif
            }
            break;

        case ACD_SPEED_STATUS:
            // knx 温控面板：auto0, high1, middle2, low4
            // tuya枚举值: auto, high, middle, low
            if (pStatus->value == 0) {
                pObj->value.dp_value = 0;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd speed=auto\n");
#endif
            } else if (pStatus->value == 1) {
                pObj->value.dp_value = 1;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd speed=high\n");
#endif
            } else if (pStatus->value == 2) {
                pObj->value.dp_value = 2;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd speed=middle\n");
#endif
            } else if (pStatus->value == 4) {
                pObj->value.dp_value = 3;
#if defined(DEBUG) && (DEBUG == 1)
                printf ("report knx sdk acd speed=low\n");
#endif
            }
            pObj->type = PROP_ENUM;

            break;
        case ACD_ENV_TEMP:
            pObj->value.dp_value = ConvetUint162Temp (pStatus->value);
            pObj->type = PROP_VALUE;
#if defined(DEBUG) && (DEBUG == 1)
            printf ("report knx sdk acd env temp=%d\n", pObj->value.dp_value);
#endif
            break;
        default:
            printf ("knx acd sdk report plugin: point(%d) not be supported!\n", pStatus->localPoint);
            break;
    }

    return 0;
}

const KnxIf KNX_Acd_General = {
    .control = knx_dev_control,
    .report = knx_dev_report};

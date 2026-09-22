#include "common.h"
#include "crc16.h"
#include "misc_dev_plugins.h"
#include <netinet/in.h>
#include "zlog.h"
#include "main.h"
#include "utils.h"

/*
producct id: mqwefweal5pkfypv

DP    功能点名称        标识符            传输类型    数据类型    功能点属性
8     当前温度         temp_current	只上报（ro）	数值型（Value）	数值范围: -20-60, 间距: 1, 倍数: 0, 单位: ℃
9     当前湿度         humidity_value	只上报（ro）	数值型（Value）	数值范围: 20-90, 间距: 1, 倍数: 0, 单位: %RH
33    PM2.5检测值     pm25_value	只上报（ro）	数值型（Value）	数值范围: 0-999, 间距: 1, 倍数: 0, 单位: ug/m3
37    CO2检测值        co2_value	只上报（ro）	数值型（Value）	数值范围: 450-2000, 间距: 1, 倍数: 0, 单位: ppm
*/

#define READ_CODE 0x42
#define WRITE_CODE 0x06


#define PROP_BOOL 0
#define PROP_VALUE 1
#define PROP_STR 2
#define PROP_ENUM 3
#define PROP_BITMAP 4

#define MODBUS_MAX_LEN 256
#define MODBUS_MIN_LEN 7

typedef enum {
    TEMP,
    HUMI,
    PM25,
    CO2,
    MAX_IDX
} Idx_e;

MbusObj_t MbusObj[250][MAX_IDX];

Msg_t *build(uint8_t devId, uint16_t regAddr, uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj) {
    (void)devId;
    (void)regAddr;
    (void)groupAddr;
    (void)pObj;
    (void)pvMbusNode;
    zlog_error(pLogCat, "AQI control interface not supported");

    return NULL;
}

MbusObj_t **parse(uint8_t devId, uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    (void)regAddr;
    (void)pvMbusNode;
    if (!pBuf || (len < MODBUS_MIN_LEN)) {
        logErr (COLOR_RED "[mbus Plugin] report() invalid parameter!\n" COLOR_CLEAR);
        return NULL;
    }
//    01 42 12 00 01 00 01 00 01 00 01 00 35 00 2B 00 60 00 02 01 92 06 D7
    uint16_t key1 = devId * 256 + READ_CODE;
    uint16_t key2 = devId * 256 + WRITE_CODE;
    uint8_t invalid = 0;
    uint8_t *pos = memmatch((uint8_t *) pBuf, key1, len);
    if (!pos || (len + pBuf - pos < MODBUS_MIN_LEN)) {
        invalid++;
        pos = memmatch((uint8_t *) pBuf, key2, len);
        if (!pos || (len + pBuf - pos < MODBUS_MIN_LEN)) {
            invalid++;
        }
    }
    if (invalid == 2) {
        logErr ("mbus plugin report: invalid command");
        return NULL;
    }

    if (pos[1] == READ_CODE) {
        MbusObj_t **ppMbusObj = NewMbusObjs(MAX_IDX);
        if (!ppMbusObj) {
            logErr ( "[mbus Plugin] report() NewMbusObjs() failed!");
            return NULL;
        }

        ppMbusObj[HUMI]->localPoint = HEAT_SWITCH;
        ppMbusObj[TEMP]->localPoint = HEAT_TEMP;
        ppMbusObj[PM25]->localPoint = HEAT_ENV_TEMP;
        ppMbusObj[CO2]->localPoint = HEAT_ENV_TEMP;

        ppMbusObj[TEMP]->value = ntohs(*(uint16_t *) (pos + 11)) - 25;
        ppMbusObj[HUMI]->value = ntohs(*(uint16_t *) (pos + 13));
        ppMbusObj[PM25]->value = ntohs(*(uint16_t *) (pos + 15));
        ppMbusObj[CO2]->value = ntohs(*(uint16_t *) (pos + 19));

        logTrc ( "AQI report: temp %d, humi %d, pm25 %d, co2 %d",
                    ppMbusObj[TEMP]->value, ppMbusObj[HUMI]->value,
                    ppMbusObj[PM25]->value, ppMbusObj[CO2]->value);

        return ppMbusObj;
    }

    return NULL;
}

const MbusIf Mbus_AQI_Ohosure = {
        .build = build,
        .parse = parse};

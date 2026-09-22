/*
* FileName: point.h
* Author:    wjian
* Date:      2023-11-16
* Version:   v1.0
* Description: data point abstract definition.
* */
#ifndef __POINT_H__
#define __POINT_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include "uthash.h"
// #include "misc_dev_plugins.h"

#define TAG_NAME_LEN 16
#define KNX_DPT_ID_LENGTH 16
#define KNX_DPT_FORMAT_LENGTH 128
#define KNX_DPT_NAME_LENGTH 128

#define PREFIX_SIZE 1
#define EIB_ADDR 9

typedef enum {
    DEVICE_TYPE_UNKNOWN,
    DEVICE_TYPE_ZIGBEE,
    DEVICE_TYPE_MODBUS,
    DEVICE_TYPE_KNX,
    DEVICE_TYPE_RF433,
    DEVICE_TYPE_DIO,
} CommunicateMode_e;

typedef enum {
    PANEL_ONE_KEY = 1,
    PANEL_TWO_KEY = 2,
    PANEL_THREE_KEY = 3,
    PANEL_FOUR_KEY = 4,
    PANEL_FIVE_KEY = 5,
    PANEL_SIX_KEY = 6,
    PANEL_SEVEN_KEY = 7,
    PANEL_EIGHT_KEY = 8,
} KEY_SIZE_u;

typedef enum {
    FEATURE_UNKNOWN = 0,
    FEATURE_SCENE,
    FREATURE_FAULT,
    FEATURE_POS,
    FEATURE_MODE,

    PANEL_SWITCH = 100,
    PANEL_SWITCH_STATUS,
    DIMMING_BRIGHTNESS,
    DIMMING_BRIGHTNESS_STATUS,

    ACD_SWITCH = 200,
    ACD_SWITCH_STATUS,
    ACD_TEMP,
    ACD_TEMP_STATUS,
    ACD_MODE,
    ACD_MODE_STATUS,
    ACD_SPEED,
    ACD_SPEED_STATUS,
    ACD_WIND_DIR,
    ACD_WIND_DIR_STATUS,
    ACD_MODE_OUTDOOR,
    ACD_MODE_OUTDOOR_STATUS,
    ACD_ENV_TEMP,
    ACD_ERROR_CODE,

    HEAT_SWITCH = 300,
    HEAT_SWITCH_STATUS,
    HEAT_TEMP,
    HEAT_TEMP_STATUS,
    HEAT_ENV_TEMP,
    HEAT_VALVE_STATUS,
    HEAT_ERROR_CODE,

    VENT_SWITCH = 400,
    VENT_SWITCH_STATUS,
    VENT_SPEED,
    VENT_SPEED_STATUS,
    VENT_WIND_DIR,
    VENT_WIND_DIR_STATUS,
    VENT_MODE,
    VENT_MODE_STATUS,
    VENT_VALVE,
    VENT_VALVE_STATUS,
    VENT_HUMI,
    VENT_HUMI_STATUS,
    VENT_ENV_TEMP,
    VENT_ENV_HUMI,
    VENT_ERROR_CODE,

    CURTAIN_SWITCH = 500,
    CURTAIN_SWITCH_STATUS,
    CURTAIN_OPEN_CLOSE,
    CURTAIN_OPEN,
    CURTAIN_CLOSE,
    CURTAIN_STOP_CONTINUOUS,
    CURTAIN_STOP,
    CURTAIN_CONTINUOUS,
    CURTAIN_PERCENT,
    CURTAIN_DIRECTION,

    AQI_TEMP = 600,
    AQI_HUMI,
    AQI_PM25,
    AQI_CO2,

    HUMIDIFIER_SWITCH = 700,
    HUMIDIFIER_HUMIDITY_SET,

    PURIFIER_SWITCH = 800,
    PURIFIER_FLOW,
    PURIFIER_TDS,
    PURIFIER_PP_FILTER_TIME,
    PURIFIER_RESET

} PointFeature_u;

/*
 * int32,uint32,float,double,bool,enum, {text,date,array,struct}
 * */
#define PointValue_t(T) \
    typedef struct {    \
        T value;        \
        T step;         \
        T scaling;      \
        T min;          \
        T max;          \
        int trigger;    \
    } Point##T_t;

typedef union {
    bool b1;
    int8_t c8;
    uint8_t u8;
    int16_t s16;
    uint16_t u16;
    int32_t i32;
    uint32_t u32;
    float f32;
    double f64;
    void *pv;
} PointValue_t;

typedef enum {
    POINT_TYPE_BOOL,
    POINT_TYPE_CHAR,
    POINT_TYPE_U8,
    POINT_TYPE_S16,
    POINT_TYPE_U16,
    POINT_TYPE_I32,
    POINT_TYPE_U32,
    POINT_TYPE_FLOAT,
    POINT_TYPE_DOUBLE,
    POINT_TYPE_POINT
} PointType_u;

typedef enum {
    POINT_ATTR_READ_WRITE,
    POINT_ATTR_READ_ONLY,
    POINT_ATTR_WRITE_ONLY  /* not ack */
} PointMode_u;

/* blurt */
typedef enum {
    POINT_POLICY_POSITIVE = 1,  /* 主动报告 */
    POINT_POLICY_NEGTIVE = 2,
    POINT_POLICY_QA = 4 /* questions and answers */
} PointPolicy_u;

typedef enum {
    POINT_TRIGGER_PERIOD = 0,
    POINT_TRIGGER_CHANGE = 1
} PointTrig_u;

typedef enum {
    POINT_ATTR_TYPE_BOOL,
    POINT_ATTR_TYPE_VALUE,
    POINT_ATTR_TYPE_STR,
    POINT_ATTR_TYPE_ENUM,
    POINT_ATTR_TYPE_BITMAP
} PointAttrType_t;

typedef struct {
    PointAttrType_t type;
    union {
        struct {
            PointValue_t step;
            PointValue_t scaling;
            PointValue_t min;
            PointValue_t max;
            char unit[64];
        } value;
        struct {
            char **ppEnum;
            int cnt;
        } enumerate;
        struct {
            char *pStr;
            int len;
            int maxlen;
        } string;
        struct {
            uint32_t raw;
            uint32_t len;
        } raw;
    } attr;
} PointAttr_t;

typedef struct {
    uint16_t id;                /* 对应涂鸦DP */
    PointFeature_u feature;     /* 功能点名称 */
    char tag[TAG_NAME_LEN + 1]; /* 标识符 */
    bool bRejected;             /* 标记非法设备 */
    PointType_u type;
    PointMode_u mode;
    PointAttr_t attr;
    PointTrig_u trig;
    PointValue_t value;
    PointValue_t oldValue;
    struct timespec timestamp;
    uint32_t threshold; // unit:100ms
} Point_t;

typedef struct KnxDataType {
    char hwAddr[EIB_ADDR + PREFIX_SIZE];
    char groupAddr[EIB_ADDR + PREFIX_SIZE];
    uint16_t phy;
    bool bRejected;
    uint16_t value;
    char cmd[10];
    struct timespec timestamp;
} KnxDataType_t;

typedef struct {
    char *pGrp;
    KnxDataType_t *pValue;
    struct timespec timestamp;
    uint32_t expire;
    bool bDone;
    UT_hash_handle hh;
} kv_item_t;

const char *GetPointFeature (PointFeature_u feature);
int kv_set(const char *grp, KnxDataType_t *pValue, uint32_t expire_ms);
KnxDataType_t *kv_get(const char *grp);
bool kv_find(const char *grp);
int kv_update (const char *grp, KnxDataType_t *pValue);
void KNX_DevDebounce (void *param);

#endif
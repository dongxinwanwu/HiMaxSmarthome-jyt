/*
* FileName: misc_dev_plugins.h
* Author:    wjian
* Date:      2022-09-07
* Version:   v1.0
* Description: device interface file
* */
#ifndef __MISC_DEVICE_PLUGIN_H__
#define __MISC_DEVICE_PLUGIN_H__

#include "common.h"
#include "device.h"
// #include "knx.h"
#include "point.h"
#include "queue.h"
#include "serialHandle.h"
#include "thpool.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define SHARED_LIBRARY_LENGTH 64
#define MANUFACTURER_NAME_LENGTH 2
#define MODEL_ID_LENGTH 32
#define PRODUCT_ID_LENGTH 32
#define DEVICE_ID_LENGTH 32
#define UUID_LEN 20
#define KEY_LEN 32
#define DEVICE_NAME_LENGTH 32

typedef enum {
    MODBUS_FLAG_READ,
    MODBUS_FLAG_WRITE,
} MbusCmdFlag_t;

/*
 * obsolete
 * */
typedef enum {
    KNX_FLAG_NONE,
    KNX_FLAG_communication = 0x01,
    KNX_FLAG_READ = 0x02,
    KNX_FLAG_WRITE = 0x04,
    KNX_FLAG_TRANSMISSION = 0x08,
    KNX_FLAG_UPDATE = 0x10,
    KNX_FLAG_INIT = 0x20
} KnxFlag_u;

typedef enum {
    DI_LEVEL_LOW,
    DI_LEVEL_HIGH,
} DiTriggerLevel_e;

typedef enum {
    DO_LEVEL_OPEN,
    DO_LEVEL_CLOSE,
} DoOutStatus_e;

/*
  KNX配置规则:
    1.温度值用2bytes;
    2.开关值，0关，1开;
*/
typedef enum {
    KNX_DATA_TYPE_1BIT,
    KNX_DATA_TYPE_2BIT,
    KNX_DATA_TYPE_4BIT,
    KNX_DATA_TYPE_1BYTE,
    KNX_DATA_TYPE_2BYTE,
    KNX_DATA_TYPE_3BYTE,
    KNX_DATA_TYPE_4BYTE,
    KNX_DATA_TYPE_5BYTE,
    KNX_DATA_TYPE_6BYTE,
    KNX_DATA_TYPE_8BYTE,
} KnxDataSize_u;

typedef enum {
    KNX_DPT_BIT,
    KNX_DPT_LONG,
    KNX_DPT_DOUBLE,
    KNX_DPT_STRING
} KnxDataType_u;

typedef enum {
    MODBUS_DATA_TYPE_BOOL,
    MODBUS_DATA_TYPE_VALUE,
    MODBUS_DATA_TYPE_STR,
    MODBUS_DATA_TYPE_ENUM,

    MODBUS_DATA_TYPE_BITMAP
} ModbusDataType_e;

typedef union {
    int32_t dp_value;  // valid when dp type is value
    uint32_t dp_enum;  // valid when dp type is enum
    int8_t *dp_str;    // valid when dp type is str
    bool dp_bool;      // valid when dp type is bool
    uint32_t dp_bitmap;// valid when dp type is bitmap
} ty_obj_dp_value_u;

typedef struct {
    char id[DEVICE_ID_LENGTH + PREFIX_SIZE];
    uint16_t node_id;
    uint16_t profile_id;
    uint16_t cluster_id;
    uint8_t src_endpoint;
    uint8_t dst_endpoint;
    uint16_t group_id;
    uint8_t cmd_type;  // zcl command type. 0x00: global command, 0x01: command is specific to a cluster.
    uint8_t cmd_id;    // zcl command id
    uint8_t frame_type;// 0: Unicast, 1: Multicast, 2: Broadcast
    uint16_t manufacturer_code;
    char disable_ack;
    uint16_t msg_length;
    uint8_t *message;
} ty_z3_aps_frame_s;

typedef struct {
    uint8_t dpid;
    uint8_t type;
    ty_obj_dp_value_u value;
    uint32_t time_stamp;
} ty_obj_dp_s;

typedef struct {
    char addr[EIB_ADDR + PREFIX_SIZE];
} KnxAddr_t;

typedef struct {
    PointFeature_u localPoint;
    KnxDataSize_u type;
    uint16_t value;
    uint16_t valueOld;
    int cloudPoint;
    char addr[EIB_ADDR + PREFIX_SIZE];
    struct timespec timestamp;
    uint32_t threshold; // unit:100ms
} KnxObj_t;

typedef struct {
    char addr[EIB_ADDR + PREFIX_SIZE];
    char addrName[DEVICE_NAME_LENGTH + PREFIX_SIZE];
    // KnxDpt_t dpt;
} KnxGroup_t;

typedef struct {
    int remap; /* 映射id */
    char addr[EIB_ADDR + PREFIX_SIZE];
    char addrName[DEVICE_NAME_LENGTH + PREFIX_SIZE];
    uint8_t value; /* knx scene value:1 ~ 64 */
} KnxScene_t;

typedef struct {
    Point_t point;
    union {
        KnxGroup_t *pCtrl;
        KnxGroup_t *pGroup;
        KnxGroup_t *pFeedback;
    };
} KnxPoint_t;

typedef struct {
    Point_t point;
    union {
        KnxGroup_t *pCtrl; /* todo：名字区分控制器和执行器 */
        KnxGroup_t *pGroup;
        KnxScene_t *pScene;
    };
    union {
        KnxGroup_t *pStatus;
        KnxGroup_t *pFeedback;
    };
} KnxPoints_t;

typedef struct {
    const char *pPrimaryKey;// 本对象的父设备ID
    DeviceType_u ePrimaryType;

    PointFeature_u localPoint;
    ModbusDataType_e type;
    uint32_t rcvValue;// recv value
    uint32_t sndValue;// send value
    uint32_t value;
    uint32_t valueOld;// 上个状态值
    int forceSync;    /* 强制同步 */
    int cloudPoint;
    MbusCmdFlag_t flag;
} MbusObj_t;

typedef struct {
    const char *szName;   /* 设备类型的名称 */
    const char *szPlugin; /* 插件名称(包含路径) */
    const char *pPrimaryKey;
    uint32_t devId; /* 设备ID: pPrimaryKey的非mac部分 */
    const char *szUUID;
    const char *szAuthKey;
    const char *pProductId;
    const char *pForeignKey; /* 关联设备主键 */
    DeviceType_u primaryType;
    CommunicateMode_e foreignType;
    void *pvForeignKey;  /* 关联设备结构体地址 */
    bool bMaster;        /* 主设备, 物模型设备 */
    DeviceAttri_u attri; /* 设备属性: DEVICE_ATTRI_ACTUATOR做主设备，bMaster逐渐被废弃 */
} DeviceElement_t;

typedef struct {
    char *pManuName;
    char *pModelId;

    int (*z3_dev_control) (const char *pCid, const ty_obj_dp_s *pObj, ty_z3_aps_frame_s *pFrame);
    int (*z3_dev_report) (const ty_z3_aps_frame_s *pFrame, ty_obj_dp_s *pObj);
    int (*z3_dev_heartbeat) (const char *pCid, ty_z3_aps_frame_s *pFrame);
} Z3If;

/* Warning: The caller must free memory */
/* todo: next version, app layer backup point history value */
typedef struct {
    int (*init) (void *pUser);
    int (*fini) (void);
    /* pObj: write is not null, read is null */
    Msg_t *(*build) (uint8_t devId, uint16_t regAddr, uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pObj);
    MbusObj_t **(*parse) (uint8_t devId, uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len);

    Msg_t *(*buildExt) (uint16_t devId, uint16_t regAddr, uint16_t groupAddr, ty_obj_dp_s *pObj);
    MbusObj_t **(*parseExt) (uint16_t devId, uint16_t regAddr, const uint8_t *pBuf, uint16_t len);
} MbusIf;

typedef struct {
    int (*init) (void *pUser);
    int (*fini) (void);
    int (*control) (const ty_obj_dp_s *pObj, KnxObj_t *pCtrl); /* pObj:in, pCtrl:out */
    int (*report) (const KnxObj_t *pStatus, ty_obj_dp_s *pObj);
} KnxIf;

typedef struct {
    DeviceElement_t element;
    uint32_t uddd;

    Z3If *pZ3;
} Z3Dev_t;

typedef struct {
    KnxIf *pIf; /* It has to be the first element */
    DeviceElement_t element;
    char phyAddr[EIB_ADDR + PREFIX_SIZE]; /* knx physical address */
    uint16_t phy;
    bool bFeedback;    /* yes or no */
    bool bOnlineCheck; /* yes or no */
    int hbIgnore;      /* 心跳忽略次数 */
} KnxBase_t;

typedef struct MbusBase {
    MbusIf *pIf; /* It has to be the first element */
    DeviceElement_t element;

    uint16_t devAddr; // modbus设备地址,有些异类是16bits
    uint16_t regAddr; // modbus寄存器地址
    uint16_t delay;   // 通讯延时
    bool bFeedback;   // 控制命令是否响应，少数非Mbus协议控制命令不应答
    bool bOnlineCheck;// 设备在线检测
    int hbIgnore;     /* 心跳忽略次数 */
    bool bFullDuplex; // 是否全双工
    bool bGroupMember;// 是否为组成员
    queue_t *pMbusDataArrived;
    Serial_t *pSerial;
    uint8_t lostCnt;// 通讯丢失次数
    int flag;       // 特殊标志: 常明窗帘=1读取转向

    struct MbusBase *pNext;
} MbusBase_t;

typedef struct {
    DeviceElement_t element;
    DiTriggerLevel_e eLevel;
    uint8_t dataPoint;
    uint8_t channel;
    char cmd[128];
    uint8_t value;
    int status;
    int retryNum;
    int retryCnt;
} DiDev_t;

typedef struct {
    DeviceElement_t element;
    DoOutStatus_e eLevel;
    uint8_t dataPoint;
    uint8_t channel;
    int fd;
    int value;
} DoDev_t;

typedef struct {
    KnxBase_t base;

    uint8_t roomNo;
    KnxObj_t *pSwitch;
    KnxObj_t *pSwitchStatus;
    KnxObj_t *pSpeed;
    KnxObj_t *pSpeedStatus;
    KnxObj_t *pMode;
    KnxObj_t *pModeStatus;
    KnxObj_t *pTemp;
    KnxObj_t *pTempStatus;
    KnxObj_t *pEnvTemp;
    KnxObj_t *pErrorCode;
} KnxAcd_t;

typedef struct {
    KnxBase_t base;

    uint8_t roomNo;
    KnxObj_t *pSwitch;
    KnxObj_t *pSwitchStatus;
    KnxObj_t *pSpeed;
    KnxObj_t *pSpeedStatus;
    KnxObj_t *pEnvTemp;
    KnxObj_t *pErrorCode;
} KnxVent_t;

typedef struct {
    KnxBase_t base;

    uint8_t roomNo;
    KnxObj_t *pSwitch;
    KnxObj_t *pSwitchStatus;
    KnxObj_t *pTemp;
    KnxObj_t *pTempStatus;
    KnxObj_t *pEnvTemp;
    KnxObj_t *pValve;
    KnxObj_t *pErrorCode;
} KnxHeat_t;

typedef struct {
    KnxBase_t base;
    KEY_SIZE_u keySum;
    KnxPoints_t keys[0];
} KnxPanel_t;

typedef struct {
    KnxPoints_t key;
    KnxPoints_t bright;
} KnxDimmerPoint_t;

typedef struct {
    KnxBase_t base;
    KEY_SIZE_u keySum;
    KnxDimmerPoint_t chl[0];
} KnxDimmer_t;

/* ZhongHai 673 */
typedef struct {
    KnxPoints_t key;
    KnxPoints_t scene;
    KnxPoints_t mode;
} KnxSmartPanelPoint_t;

typedef struct {
    KnxBase_t base;
    KEY_SIZE_u keySum;
    KnxSmartPanelPoint_t chl[0];
} KnxSmartPanel_t;

/*
 * STEK: open-close: one group address, 1bit, stop: one group address, 1bit.  switch panel.
 * General： open-close-stop-continue: one group address, 1byte.
 * */
typedef struct {
    KnxBase_t base;
    KnxPoints_t *pOpen;
    KnxPoints_t *pClose;
    KnxPoints_t *pStop;
    KnxPoints_t *pPersent;
    Point_t *pPos;
    Point_t *pFault;
} KnxCurtain_t;

typedef struct {
    char szPlugin[SHARED_LIBRARY_LENGTH + PREFIX_SIZE];
    char pPrimaryKey[DEVICE_ID_LENGTH + PREFIX_SIZE];
    uint32_t uddd;

} RfIf_t;

typedef struct MbusVent {
    MbusBase_t *pBase; /* It has to be the first element */

    MbusObj_t *pSwitch;
    MbusObj_t *pSpeed;
    MbusObj_t *pHumi;
    MbusObj_t *pEnvHumi;
    MbusObj_t * pMode;
    MbusObj_t *pValve;
    MbusObj_t *pErrorCode;
} MbusVent_t;

typedef struct MbusHeat {
    MbusBase_t *pBase; /* It has to be the first element */

    MbusObj_t *pSwitch;
    MbusObj_t *pTemp;
    MbusObj_t *pEnvTemp;
    MbusObj_t *pErrorCode;
} MbusHeat_t;

typedef struct MbusAcd {
    MbusBase_t *pBase; /* It has to be the first element */

    MbusObj_t *pSwitch;
    MbusObj_t *pSpeed;
    MbusObj_t *pMode;
    MbusObj_t *pTemp;
    MbusObj_t *pEnvTemp;
    MbusObj_t *pErrorCode;
} MbusAcd_t;

typedef struct {
    MbusBase_t *pBase; /* It has to be the first element */

    MbusObj_t *pSwitch;
    MbusObj_t *pFault;
    MbusObj_t *pPos;
    MbusObj_t *pDirec;
} MbusCurtain_t;

typedef struct {
    MbusBase_t *pBase; /* It has to be the first element */

    MbusObj_t *pTemp;
    MbusObj_t *pHumi;
    MbusObj_t *pPM25;
    MbusObj_t *pCO2;
} MbusAqi_t;

typedef struct {
    MbusBase_t *pBase; /* It has to be the first element */
    MbusObj_t *pSwitch;
    MbusObj_t *pHumidity;
} ModbusHumidifier_t;

typedef struct {
    MbusBase_t *pBase; /* It has to be the first element */
    MbusObj_t *pFlow;
    MbusObj_t *pTDS;
    MbusObj_t *pFilterTime;
    MbusObj_t *pReset;
} ModbusPurifier_t;

typedef struct {
    MbusBase_t *pBase; /* It has to be the first element */

    MbusObj_t *pSwitch;
} MbusPanel_t;

typedef struct {
    char *pProductId;
    Point_t *pState;
    Point_t *pReset;
    Point_t *pAlarm;
    Point_t *pScene;
    Point_t *pFault;
    Point_t *pPos;
} Gateway_t;

typedef struct {
    uint16_t groupId;
    int deviceType;
    char *pPrimaryKey;
    char **ppForeignKey;
} Group_t;

#endif /* __MISC_DEVICE_PLUGIN_H__ */
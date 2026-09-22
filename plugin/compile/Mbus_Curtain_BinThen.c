//
// Created by Haythem Kenway on 2022/8/11.
//
#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "zlog.h"
#include "../utils.h"
#include "common.h"

// tuya 枚举值: open, stop, close

#define MODBUS_OBJ_SIZE 1


#define CMD_TYPE_QUERY 0x0A01               //查询电机状态（主控 -> 电机）
#define CMD_TYPE_EXECUTE 0x0A02             //执行命令
#define CMD_TYPE_RETURN_CODE 0x0A03         //设备返回的状态（电机 -> 主控）
#define CMD_TYPE_TEST 0x0A04                //自检（没卵用）
#define CMD_TYPE_ADDRESS 0x0A05             //烧写电机SN码（触发该模式视设备型号而定）
#define CMD_TYPE_UPLOAD 0x0A06              //设置上行程
#define CMD_TYPE_DOWNLOAD 0x0A07            //设置下行程
#define CMD_TYPE_REMOVE 0x0A08              //设置删除行程
#define CMD_TYPE_ACTIVE_PERCENTAGE 0x0A09   //执行按照百分比的步进
#define CMD_TYPE_TOGGLE 0x0A0A              //换向

#define ACTION_STOP 0x02                    //停止电机
#define ACTION_OPEN 0x01                    //打开电机
#define ACTION_CLOSE 0x00                   //关闭电机

typedef uint8_t byte;

/**
 * <h2>丙申UART包</h2>
 * */
typedef struct {
    uint32_t header;                    //帧头 固定值：0x55AAAA55
    uint16_t crc;                       //校验位：0x2017 + 除了校验位所有字节的值
    uint16_t cmdType;                   //命令类型，可填写的值为CMD_TYPE开头的所有宏定义
    uint16_t dataLength;                //数据长度，固定值：10（自定义数据区字节大小，已写死）
    uint16_t protocolVersion;           //通讯版本号，固定值：0
    uint8_t deviceType;                 //设备类型，电机固定值：01
    uint8_t action;                     //动作数据，可填写的值为ACTION开头的所有宏定义
    uint8_t locationPercentage;         //动作百分比，0x00-0x64，精度为1
    uint8_t SN[3];                      //设备SN码，看做为设备ID，前两位写死01，使用第三位作为设备ID区分设备，范围0-255
    uint8_t undefined[4];               //预留数据位，无意义
}BinThen_Curtain_UARTPackage_t;

/**
 * <h2>丙申HEX源包</h2>
 * */
typedef struct {
    byte *data;                      //HEX数据包，最大22字节
    int length;                         //方便迭代(目前没卵用)
}BinThen_Curtain_HexPackage_t;


const uint32_t packageHeader = 0x55AAAA55;
const uint16_t crcHeader = 0x2017;

Msg_t *generatePackage (BinThen_Curtain_UARTPackage_t uartPackage) {
    Msg_t *pMsg = (Msg_t *) calloc (1, sizeof (Msg_t));
    if (!pMsg) {
        printf (COLOR_RED "mbus Plugin control alloc fail!\n" COLOR_CLEAR);
        return NULL;
    }
    pMsg->pBuf = (uint8_t *) calloc (22, 1);
    if (!pMsg->pBuf) {
        free (pMsg);
        printf (COLOR_RED "mbus Plugin control alloc fail!\n" COLOR_CLEAR);
        return NULL;
    }

    for (int i = 0; i < 4; ++i) {
        *((uint8_t *) pMsg->pBuf + pMsg->len++) = (uint8_t) (uartPackage.header >> 8 * i);
    }

    for (int i = 0; i < 2; ++i) {
        *((uint8_t *) pMsg->pBuf + pMsg->len++) = (uint8_t) (uartPackage.crc >> 8 * i);
    }

    for (int i = 0; i < 2; ++i) {
        *((uint8_t *) pMsg->pBuf + pMsg->len++) = (uint8_t) (uartPackage.cmdType >> 8 * i);
    }

    for (int i = 0; i < 2; ++i) {
        *((uint8_t *) pMsg->pBuf + pMsg->len++) = (uint8_t) (uartPackage.dataLength >> 8 * i);
    }

    for (int i = 0; i < 2; ++i) {
        *((uint8_t *) pMsg->pBuf + pMsg->len++) = (uint8_t) (uartPackage.protocolVersion >> 8 * i);
    }

    *((uint8_t *) pMsg->pBuf + pMsg->len++) = uartPackage.deviceType;

    *((uint8_t *) pMsg->pBuf + pMsg->len++) = uartPackage.action;

    *((uint8_t *) pMsg->pBuf + pMsg->len++) = uartPackage.locationPercentage;

    for (int i = 0; i < 3; ++i) {
        *((uint8_t *) pMsg->pBuf + pMsg->len++) = uartPackage.SN[i];
    }

    for (int i = 0; i < 4; ++i) {
        *((uint8_t *) pMsg->pBuf + pMsg->len++) = uartPackage.undefined[i];
    }

    //crc
    for (uint32_t i = 0; i < pMsg->len; ++i) {
        uartPackage.crc += *((uint8_t *) pMsg->pBuf + i);
    }

    uartPackage.crc += crcHeader;

    for (int i = 0; i < 2; ++i) {
        *((uint8_t *) pMsg->pBuf + i + 4) = (uint8_t) (uartPackage.crc >> 8 * i);
    }
    pMsg->len = 22;

    return pMsg;
}

BinThen_Curtain_UARTPackage_t BinThen_UnpackPackage (const uint8_t *pBuf) {
    BinThen_Curtain_UARTPackage_t res;
    int offset = 0;
    int tmp;

    tmp = offset;
    for (int i = offset; i < tmp + 4; ++i) {
        res.header |= (uint32_t) (pBuf[i] << 8 * (i - tmp));
        offset++;
    }

    tmp = offset;
    for (int i = offset; i < tmp + 2; ++i) {
        res.crc |= (uint16_t) (pBuf[i] << 8 * (i - tmp));
        offset++;
    }

    tmp = offset;
    for (int i = offset; i < tmp + 2; ++i) {
        res.cmdType |= (uint16_t) (pBuf[i] << 8 * (i - tmp));
        offset++;
    }

    tmp = offset;
    for (int i = offset; i < tmp + 2; ++i) {
        res.dataLength |= (uint16_t) (pBuf[i] << 8 * (i - tmp));
        offset++;
    }

    tmp = offset;
    for (int i = offset; i < tmp + 2; ++i) {
        res.protocolVersion |= (uint16_t) (pBuf[i] << 8 * (i - tmp));
        offset++;
    }

    res.deviceType = (uint8_t) pBuf[offset];
    offset++;

    res.action = (uint8_t) pBuf[offset];
    offset++;

    res.locationPercentage = (uint8_t) pBuf[offset];
    offset++;

    for (int i = 0; i < 3; ++i) {
        res.SN[i] = (uint8_t) pBuf[offset];
        offset++;
    }

    for (int i = 0; i < 4; ++i) {
        res.undefined[i] = (uint8_t) pBuf[offset];
        offset++;
    }

    return res;
}

Msg_t *BinThen_UART_PackageFactory (const uint16_t cmdType, const uint8_t action, const uint8_t SN3) {
    BinThen_Curtain_UARTPackage_t package = {0};
    package.header = packageHeader;
    package.crc = 0;
    package.cmdType = cmdType;
    package.dataLength = 0x000A;
    package.protocolVersion = 0;
    package.deviceType = 0x01;
    package.action = action;
    package.locationPercentage = 0;

    for (int i = 0; i < 3; ++i) {
        package.SN[i] = 0x01;
    }

    //以SN3作为窗帘电机ID
    package.SN[2] = SN3;

    return generatePackage (package);
}

Msg_t *modbus_dev_control (uint8_t modbusId, uint16_t regAddr, uint16_t groupAddr, const void *pvMbusNode, ty_obj_dp_s *pCloudObj) {
    (void) regAddr;
    (void) groupAddr;
    (void) pvMbusNode;

    if (!pCloudObj) {
        return BinThen_UART_PackageFactory (CMD_TYPE_QUERY, ACTION_OPEN, modbusId);
    }

    uint8_t action = 0;
    if (pCloudObj->value.dp_enum == 0) {// tuya.open
        action = ACTION_OPEN;
    } else if (pCloudObj->value.dp_enum == 1) {// tuya.stop
        action = ACTION_STOP;
    } else if (pCloudObj->value.dp_enum == 2) {// tuya.close
        action = ACTION_CLOSE;
    }

    return BinThen_UART_PackageFactory (CMD_TYPE_EXECUTE, action, modbusId);
}

MbusObj_t **modbus_dev_report (uint8_t modbusId, uint16_t regAddr, const void *pvMbusNode, const uint8_t *pBuf, uint16_t len) {
    (void) modbusId;
    (void) regAddr;
    (void)pvMbusNode;
    if (!pBuf || (len < 22)) {
        logErr ( "[mbus Plugin] BinThen Curtain handler: invalid parameter!");
        return NULL;
    }

    BinThen_Curtain_UARTPackage_t res = BinThen_UnpackPackage (pBuf);
    MbusObj_t **ppMbusObj = NewMbusObjs (MODBUS_OBJ_SIZE + 1);
    if (!ppMbusObj) {
        logErr ( "[mbus Plugin] parse() NewMbusObjs() failed!");
        return NULL;
    }
    ppMbusObj[1] = NULL;

    if (res.action == ACTION_OPEN) {
        ppMbusObj[0]->value = 0;
    } else if (res.action == ACTION_STOP) {
        ppMbusObj[0]->value = 1;
    } else if (res.action == ACTION_CLOSE) {
        ppMbusObj[0]->value = 2;
    } else {
        logErr ( "[mbus Plugin] BinThen Curtain handler: action(%d) not be supported!", res.action);
        return NULL;
    }
    ppMbusObj[0]->localPoint = CURTAIN_SWITCH;
    ppMbusObj[0]->rcvValue = ppMbusObj[0]->value;

    return ppMbusObj;
}

const MbusIf Mbus_Curtain_BinThen = {
    .build = modbus_dev_control,
    .parse = modbus_dev_report};

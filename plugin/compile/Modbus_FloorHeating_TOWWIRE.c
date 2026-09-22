//
// Created by haythem on 3/14/24.
//

#include "main.h"
#include "crc16.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#define READ_CODE   3
#define WRITE_CODE  6

__attribute__ ((unused))
typedef enum {
    SWITCH = 0,
    TEMP,
    ENV_TEMP,
    ERROR,
    USAGE
} DataPoints;

__attribute__ ((unused))
typedef enum {
    TY_SWITCH = 1,
    TY_TEMP = 2,
    TY_ENV_TEMP = 3
} TY_DataPoints;

static int checkPackage(const uint8_t* package, uint8_t length, uint8_t devId) {
    if (length < 3) {
        logErr ( "invalid package length");
        return -1;
    }

    uint16_t crc = crc16(package, length - 2);
    uint16_t packageCRC = (package[length - 2] << 8) + package[length - 1];
    if (crc != packageCRC) {
        logErr ( "invalid package length %d, which should be %d", packageCRC, crc);
        return -1;
    }

    if (devId != package[0]) {
        logErr ( "Invalid deviceId: %d, which should be %d", package[0], devId);
        return -1;
    }

    if (package[1] != READ_CODE && package[1] != WRITE_CODE) {
        logErr ( "invalid code %d", package[1]);
        return -1;
    }

    return 0;
}

Msg_t* build(uint8_t devId, uint16_t regAddr, __attribute__ ((unused)) uint16_t groupAddr, __attribute__ ((unused)) const void *modbusNode, ty_obj_dp_s *obj) {
    debugMessage(pLogCat, "Modbus floor heating: devId: %d, regAddr: %d", devId, regAddr);
    Msg_t *msg = calloc(1, sizeof(Msg_t));
    if(!msg) {
        logErr ( "alloc memory failed");
        return NULL;
    }

    msg->pBuf = calloc(8, sizeof(uint8_t));
    if (!msg->pBuf) {
        free(msg);
        logErr ( "alloc memory failed");
        return NULL;
    }

    msg->len = 8;
    msg->pBuf[0] = devId;
    if (!obj) {
        // 01 03 00 04 00 05 C4 08
        msg->pBuf[1] = READ_CODE;
        msg->pBuf[3] = 4 + 7 * (regAddr - 1);
        msg->pBuf[5] = 5;

        uint16_t crc = crc16(msg->pBuf, 6);
        msg->pBuf[6] = crc >> 8;
        msg->pBuf[7] = crc;

        return msg;
    }

    // 01 06 00 04 00 01 09 CB
    msg->pBuf[1] = WRITE_CODE;
    switch (obj->dpid) {
        case TY_SWITCH: {
            msg->pBuf[3] = (regAddr - 1) * 7 + 4;
            msg->pBuf[5] = obj->value.dp_bool;
            break;
        }
        case TY_TEMP: {
            msg->pBuf[3] = (regAddr - 1) * 7 + 5;
            msg->pBuf[4] = obj->value.dp_value >> 8;
            msg->pBuf[5] = obj->value.dp_value;
            break;
        }
        default: {
            logErr ( "Invalid dataPoint %d", obj->dpid);
            free(msg->pBuf);
            free(msg);
            return NULL;
        }
    }

    uint16_t crc = crc16(msg->pBuf, 6);
    msg->pBuf[6] = crc >> 8;
    msg->pBuf[7] = crc;

    return msg;
}

MbusObj_t** parse(uint8_t devId, __attribute__ ((unused)) uint16_t regAddr, __attribute__ ((unused)) const void *modbusNode, const uint8_t *package, uint16_t length) {
    if (!package || checkPackage(package, length, devId) == -1) return NULL;
    if (package[1] == READ_CODE) {
        MbusObj_t **modbusObj = NewMbusObjs(3);
        if (!modbusObj) return NULL;
        // 01 03 09 00 01 01 02 03 04 42 BA
        modbusObj[SWITCH]->localPoint = HEAT_SWITCH;
        modbusObj[SWITCH]->value = package[4];
        modbusObj[TEMP]->localPoint = HEAT_TEMP;
        modbusObj[TEMP]->value = (package[5] << 8) + package[6];
        modbusObj[ENV_TEMP]->localPoint = HEAT_ENV_TEMP;
        modbusObj[ENV_TEMP]->value = (package[7] << 8) + package[8];

        return modbusObj;
    }

    // WRITE_CODE
    MbusObj_t **modbusObj = NewMbusObjs(1);
    if(!modbusObj) return NULL;
    // mathematical function:
    // (x - 3) % 7 - 1 = y
    // y = [0, 4]
    uint8_t dataPoint = (package[3] - 3) % 7 - 1;
    modbusObj[0]->localPoint = HEAT_SWITCH + 2 * dataPoint;
    // 01 06 00 04 00 01 09 CB
    switch (dataPoint) {
        case SWITCH: {
            modbusObj[0]->value = package[5];
            break;
        }
        case TEMP:
        case ENV_TEMP: {
            modbusObj[0]->value = (package[4] << 8) + package[5];
            break;
        }
        default: {
            logErr ( "Invalid index");
            free(modbusObj[0]);
            free (modbusObj);
            return NULL;
        }
    }

    return modbusObj;
}

__attribute__((unused))
const MbusIf Modbus_FloorHeating_TOWWIRE = {
    .build = build,
    .parse = parse
};
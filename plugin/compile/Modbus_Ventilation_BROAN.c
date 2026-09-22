//
// Created by haythem on 3/14/24.
// This is NOT A MODBUS protocol!!!
//

#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#define HEADER              0xAA
#define FOOTER              0xF5
#define HOST_ID             2

__attribute__ ((unused))
typedef enum {
    TY_SPEED = 28,
    TY_SWITCH = 39,
    TY_ENV_TEMP = 46
} TY_DataPoints;

typedef enum {
    SPEED = 0,
    SWITCH,
    ENV_TEMP
} DataPoint;

static uint8_t checkSum(const uint8_t* package, uint8_t isReceive) {
    uint32_t sum = 0;
    for (int i = 1; i < 7 + 2 * isReceive; ++i) {
        sum += package[i];
    }

    return sum;
}

static int checkPackage(const uint8_t* package, uint8_t length, uint8_t devId) {
    if (package[0] != HEADER || package[length - 1] != FOOTER) {
        logErr ( "Invalid package format");
        return -1;
    }

    if (package[1] != devId) {
        logErr ( "Invalid device address: %d, which should be %d", package[1], devId);
        return -1;
    }

    if (package[2] != HOST_ID) {
        logErr ( "Invalid host ID: %d, which should be %d", package[2], HOST_ID);
        return -1;
    }

    uint8_t check = checkSum(package, 1);
    if (check != package[9]) {
        logErr ( "Invalid checkSum: %d, which should be %d", package[9], check);
        return -1;
    }

    return 0;
}

Msg_t* build(uint8_t devId, uint16_t regAddr, __attribute__ ((unused)) uint16_t groupAddr, __attribute__ ((unused)) const void *modbusNode, ty_obj_dp_s *obj) {
    debugMessage(pLogCat, "Modbus Ventilation: devId: %d, regAddr: %d", devId, regAddr);
    Msg_t *msg = calloc(1, sizeof(Msg_t));
    if(!msg) {
        logErr ( "alloc memory failed");
        return NULL;
    }

    msg->pBuf = calloc(9, sizeof(uint8_t));
    if (!msg->pBuf) {
        free(msg);
        logErr ( "alloc memory failed");
        return NULL;
    }
    msg->len = 9;

    msg->pBuf[0] = HEADER;
    msg->pBuf[1] = devId;
    msg->pBuf[2] = HOST_ID;

    if (!obj) {
        msg->pBuf[6] = 0xA5;
    } else {
        switch (obj->dpid) {
            case TY_SPEED: {
                msg->pBuf[3] = obj->value.dp_enum > 0;
                msg->pBuf[4] = obj->value.dp_enum;
                msg->pBuf[5] = obj->value.dp_enum;
                break;
            }
            case TY_SWITCH: {
                msg->pBuf[3] = obj->value.dp_bool > 0;
                msg->pBuf[4] = obj->value.dp_bool;
                msg->pBuf[5] = obj->value.dp_bool;
                break;
            }
            default: {
                logErr ( "Invalid TuYa dataPoint: %d", obj->dpid);
                free(msg->pBuf);
                free(msg);
                return NULL;
            }
        }
        msg->pBuf[6] = 0x5A;
    }
    msg->pBuf[7] = checkSum(msg->pBuf, 0);
    msg->pBuf[8] = FOOTER;

    return msg;
}

MbusObj_t** parse(uint8_t devId, __attribute__ ((unused)) uint16_t regAddr, __attribute__ ((unused)) const void *modbusNode, const uint8_t *package, uint16_t length) {
    if (!package || checkPackage(package, length, devId) == -1) return NULL;
    MbusObj_t **modbusObj = NewMbusObjs(3);
    if (!modbusObj) return NULL;

    modbusObj[SPEED]->localPoint = VENT_SPEED;
    modbusObj[SPEED]->value = package[4];
    modbusObj[SWITCH]->localPoint = VENT_SWITCH;
    modbusObj[SWITCH]->value = package[3];
    modbusObj[ENV_TEMP]->localPoint = VENT_ENV_TEMP;
    modbusObj[ENV_TEMP]->value = ~((package[6] & 0x7F) - 1);

    return modbusObj;
}

__attribute__((unused))
const MbusIf Modbus_Ventilation_BROAN = {
    .build = build,
    .parse = parse
};
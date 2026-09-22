//
// Created by haythem on 3/13/24.
// This is NOT A MODBUS protocol
//

#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#define HEADER                      0xAA
#define READ_LENGTH                 0x05
#define READ_CODE                   0x04
#define RESET_LENGTH                0x09
#define RESET_CODE                  0x05

__attribute__ ((unused))
typedef enum {
    TY_FLOW_RATE,
    TY_TDS,
    TY_PP_FILTER_TIME,
    TY_PP_FILTER_RESET
} TyDataPoints;

typedef enum {
    FLOW = 0,
    TDS,
    PP_FILTER_TIME,
    PP_FILTER_RESET
} DataPoint;

static uint8_t checkSum(const uint8_t* package, uint8_t length) {
    uint32_t sum = 0;
    for (int i = 1; i < length; ++i) {
        sum += package[i];
    }

    return sum;
}

static int checkPackage(const uint8_t* package, uint8_t length, uint8_t devId) {
    if (length < 3) {
        logErr ( "invalid package length < 3");
        return -1;
    }

    if (package[0] != HEADER) {
        logErr ( "invalid package header %d, which should be %d", package[0], HEADER);
        return -1;
    }

    if (package[1] != devId) {
        logErr ( "invalid device location %d, which should be %d", package[1], devId);
        return -1;
    }

    if (package[3] != READ_CODE && package[3] != RESET_CODE) {
        logErr ( "invalid function code %d", package[3]);
        return -1;
    }

    if (package[2] != length) {
        logErr ( "invalid packageLength: %d, which should be %d", package[2], length);
        return -1;
    }

    uint8_t check = checkSum(package, package[2] - 1);
    if (checkSum(package, package[2] - 1) != package[length - 1]) {
        logErr ( "invalid checksum: %d, which should be %d", package[length - 1], check);
        return -1;
    }

    return 0;
}

Msg_t* build(uint8_t devId, uint16_t regAddr, __attribute__ ((unused)) uint16_t groupAddr, __attribute__ ((unused)) const void *modbusNode, ty_obj_dp_s *obj) {
    debugMessage(pLogCat, "Modbus Purifier: devId: %d, regAddr: %d", devId, regAddr);
    Msg_t *msg = calloc(1, sizeof(Msg_t));
    if(!msg) {
        logErr ( "alloc memory failed");
        return NULL;
    }

    if (!obj) {
        msg->pBuf = calloc(5, sizeof(uint8_t));
        if (!msg->pBuf) {
            free(msg);
            logErr ( "alloc memory failed");
            return NULL;
        }
        msg->len = READ_LENGTH;

        msg->pBuf[2] = READ_LENGTH;
        msg->pBuf[3] = READ_CODE;
        msg->pBuf[4] = checkSum(msg->pBuf, 5);
    } else {
        msg->pBuf = calloc(9, sizeof(uint8_t));
        if (!msg->pBuf) {
            free(msg);
            logErr ( "alloc memory failed");
            return NULL;
        }
        msg->len = RESET_LENGTH;

        msg->pBuf[2] = RESET_LENGTH;
        msg->pBuf[3] = RESET_CODE;
        if (obj->dpid == 104) {
            memset(msg->pBuf + 4, 0x55, 4);
            msg->pBuf[8] = checkSum(msg->pBuf, 9);
        } else {
            free(msg->pBuf);
            free(msg);
            return NULL;
        }
    }

    msg->pBuf[0] = HEADER;
    msg->pBuf[1] = devId;

    return msg;
}

MbusObj_t** parse(uint8_t devId, __attribute__ ((unused)) uint16_t regAddr, __attribute__ ((unused)) const void *modbusNode, const uint8_t *package, uint16_t length) {
    if (!package || checkPackage(package, length, devId) == -1) return NULL;
    MbusObj_t **modbusObj = NULL;

    switch (package[2]) {
        case READ_CODE: {
            modbusObj = NewMbusObjs(4);
            if (!modbusObj) return NULL;

            modbusObj[FLOW]->localPoint = PURIFIER_FLOW;
            uint16_t outputTotal = 0;
            for (int i = 0; i < 4; ++i) {
                // 4 + i * 2 + 1
                outputTotal += (package[5 + i * 2] << 8) + package[4 + i * 2];
            }
            modbusObj[FLOW]->value = outputTotal;
            modbusObj[TDS]->localPoint = PURIFIER_TDS;
            // 3 + 18 3 + 17
            modbusObj[TDS]->value = (package[21] << 8) + package[20];
            modbusObj[PP_FILTER_TIME]->localPoint = PURIFIER_PP_FILTER_TIME;
            // 3 + 10 3 + 9
            modbusObj[PP_FILTER_TIME]->value = (package[13] << 8) + package[12];
            modbusObj[PP_FILTER_RESET]->localPoint = PURIFIER_RESET;
            modbusObj[PP_FILTER_RESET]->value = 0;
            break;
        }
        case RESET_CODE: {
            const uint8_t res[4] = {0};
            modbusObj = NewMbusObjs(1);
            if (!modbusObj) return NULL;
            if(memcmp(package + 4, res, 4) == 0) {
                modbusObj[0]->localPoint = PURIFIER_RESET;
                modbusObj[0]->value = 1;
            } else {
                logErr ( "reset failed, except 55 55 55 55 but found %02x %02x %02x %02x", package[4], package[5], package[6], package[7]);
            }
            break;
        }
        default: {
            logErr ( "Invalid code");
            return NULL;
        }
    }

    return modbusObj;
}

__attribute__((unused))
const MbusIf Modbus_Purifier = {
    .build = build,
    .parse = parse
};

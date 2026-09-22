//
// Created by haythem on 3/13/24.
//

#include "crc16.h"
#include "main.h"
#include "misc_dev_plugins.h"
#include "utils.h"
#include <stdlib.h>
#include <string.h>

#define WET_INFO_INPUT_REGISTER               0x90
#define TARGET_POWER_INPUT_REGISTER           0x91
#define CURRENT_POWER_INPUT_REGISTER          0x92
#define FAN_CURRENT_INPUT_REGISTER            0x93

#define READ_COIL                               1
#define READ_REGISTER                           4
#define WRITE_COIL                              5
#define WRITE_REGISTER                          6

typedef enum {
    SWITCH = 101,
    HUMIDITY_SET = 102
} TyDp;

int checkPackage(const uint8_t* package, uint8_t length, uint8_t devId) {
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

    if (package[1] != READ_REGISTER && package[1] != READ_COIL && package[1] != WRITE_REGISTER && package[1] != WRITE_COIL) {
        logErr ( "invalid code %d", package[1]);
        return -1;
    }

    return 0;
}

Msg_t* build(uint8_t devId, uint16_t regAddr, __attribute__ ((unused)) uint16_t groupAddr, __attribute__ ((unused)) const void *modbusNode, ty_obj_dp_s *obj) {
    static uint8_t count = 0;
    debugMessage(pLogCat, "Modbus Humidifier: devId: %d, regAddr: %d", devId, regAddr);
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
        if ((count & 1) == 1) {
            // switch
            // 01 01 00 00 00 01 FD CA
            msg->pBuf[1] = READ_COIL;
            msg->pBuf[5] = 1;
        } else {
            // output power
            // 01 04 00 91 00 01 60 27
            msg->pBuf[1] = READ_REGISTER;
            msg->pBuf[3] = TARGET_POWER_INPUT_REGISTER;
            msg->pBuf[5] = 1;
        }

        count++;
    } else {
        switch (obj->dpid) {
            case SWITCH: {
                // 01 05 00 00 FF 00 8C 3A
                msg->pBuf[1] = WRITE_COIL;
                msg->pBuf[4] = obj->value.dp_bool == 1 ? 0xFF : 0;
                break;
            }
            case HUMIDITY_SET: {
                // 01 06 00 62 00 30 28 00
                msg->pBuf[1] = WRITE_REGISTER;
                msg->pBuf[3] = 0x62;
                msg->pBuf[5] = obj->value.dp_value;
                break;
            }
            default: {
                logErr ( "Invalid TyDp: %d", obj->dpid);
                free(msg->pBuf);
                free(msg);
                return NULL;
            }
        }
    }
    uint16_t crc = crc16(msg->pBuf, 6);
    msg->pBuf[6] = crc >> 8;
    msg->pBuf[7] = crc;

    return msg;
}

MbusObj_t** parse(uint8_t devId, __attribute__ ((unused)) uint16_t regAddr, __attribute__ ((unused)) const void *modbusNode, const uint8_t *package, uint16_t length) {
    if (!package || checkPackage(package, length, devId) == -1) return NULL;
    MbusObj_t **modbusObj = NewMbusObjs(1);
    if (!modbusObj) return NULL;

    // odd for coil, even for register
    modbusObj[0]->localPoint = HUMIDIFIER_SWITCH + ((package[1] & 1) == 0);
    switch (package[1]) {
        case READ_COIL: {
            // 02 01 02 FF 00 BC 0C
            modbusObj[0]->value = package[3] == 0xFF ? 1 : 0;
            break;
        }
        case READ_REGISTER: {
            // 02 04 02 00 91 3C 9C
            modbusObj[0]->value = package[4];
            break;
        }
        case WRITE_COIL: {
            // 02 05 00 00 FF 00 8C 09
            modbusObj[0]->value = package[4] == 0xFF ? 1 : 0;
            break;
        }
        case WRITE_REGISTER: {
            // 02 06 00 01 00 62 59 D0
            modbusObj[0]->value = package[5];
            break;
        }
        default: {
            logErr ( "Invalid code: %d", package[1]);
            free(modbusObj[0]);
            free (modbusObj);
            return NULL;
        }
    }

    return modbusObj;
}

__attribute__((unused))
const MbusIf Modbus_Humidifier_SPTC = {
    .build = build,
    .parse = parse
};


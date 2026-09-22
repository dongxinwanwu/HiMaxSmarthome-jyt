#include <netinet/in.h>
#include "utils.h"
#include "common.h"
#include "zlog.h"

MbusObj_t **NewMbusObjs(int size) {
    MbusObj_t **ppMbusObj = (MbusObj_t **) calloc(size + 1, sizeof(MbusObj_t *));
    if (!ppMbusObj) {
        zlog_error(pLogCat, COLOR_RED"new MbusObj_t ** failed!"COLOR_CLEAR);
        return NULL;
    }

    for (int i = 0; i < size; i++) {
        ppMbusObj[i] = (MbusObj_t *) calloc(1, sizeof(MbusObj_t));
        if (!ppMbusObj[i]) {
            for (int j = 0 ; j <= i; j++) {
                free(ppMbusObj[j]);
            }
            free(ppMbusObj);
            zlog_error(pLogCat, COLOR_RED"new MbusObj_t * failed!"COLOR_CLEAR);
            return NULL;
        }
    }
    ppMbusObj[size] = NULL;

    return ppMbusObj;
}

uint8_t *memmatch(uint8_t *pSrc, uint16_t dst, int len) {
    if (!pSrc || (len == 0)) return NULL;

    for (int i = 0; i < len - 2; i++) {
        if (dst == ntohs(*(uint16_t *) &pSrc[i])) return &pSrc[i];
    }

    return NULL;
}

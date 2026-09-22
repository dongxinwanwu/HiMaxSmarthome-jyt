/*
* FileName: point.h
* Author:    wjian
* Date:      2023-11-16
* Version:   v1.0
* Description: knx device abstract definition.
* */
#ifndef __KNX_H__
#define __KNX_H__
#include "common.h"
#include "point.h"

#define KNX_DPST_ATRRI  SECTION(knx_dpst_attri)

typedef struct {
    char id[KNX_DPT_ID_LENGTH];
    char format[KNX_DPT_FORMAT_LENGTH];
    char name[KNX_DPT_NAME_LENGTH];
} KnxDpt_t;

int findKnxDptsAtrri(PointAttr_t *pAttr, char *pType);

#endif



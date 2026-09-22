#include "knx.h"
#include <stdio.h>
#include "common.h"

typedef char *(*KnxDpstAttri_t) (PointAttr_t *pAttr);

int WEAK findKnxDptsAtrri(PointAttr_t *pAttr, char *pType) {
#if 1
    (void)pAttr;
    (void)pType;
#else
    extern KnxDpstAttri_t _knx_dpts_start;
    extern KnxDpstAttri_t _knx_dpts_end;

    for (KnxDpstAttri_t *pfn = &_knx_dpts_start; pfn < &_knx_dpts_end; pfn++) {
        const char *pDataType = (*pfn)(pAttr);
        int major1, minor1, major2, minor2;
        sscanf(pDataType, "DPTS_%d_%d", &major1, &minor1);
        sscanf(pType, "DPTS-%d-%d", &major2, &minor2);
        if ((major1 == major2) && (minor1 == minor2)) {
            return 0;
        }
    }
#endif

    return -1;
}

KNX_DPST_ATRRI const char *DPST_5_1 (PointAttr_t *pAttr)  {
    pAttr->type = POINT_ATTR_TYPE_VALUE;
//    pAttr->attr.value.type = POINT_TYPE_U8;
    pAttr->attr.value.step.u8 = 1;
    pAttr->attr.value.scaling.u8 = 1;
    pAttr->attr.value.min.u8 = 0;
    pAttr->attr.value.max.u8 = 100;
    strcpy (pAttr->attr.value.unit, "%");
//    strncpy (pAttr->attr.value.format, "U8", sizeof (pAttr->attr.value.format) -1);
//
    return __func__;
}

/*
 * THIS DPT SHALL BE USE FOR ALL KINDS OF COUNTING,
 * */
KNX_DPST_ATRRI const char *DPST_5_10 (PointAttr_t *pAttr)  {
    pAttr->type = POINT_ATTR_TYPE_VALUE;
//    pAttr->attr.value.type = POINT_TYPE_U8;
    pAttr->attr.value.step.u8 = 1;
    pAttr->attr.value.scaling.u8 = 1;
    pAttr->attr.value.min.u8 = 0;
    pAttr->attr.value.max.u8 = 255;
    strcpy (pAttr->attr.value.unit, "%");
//    strncpy (pAttr->attr.value.format, "U8", sizeof (pAttr->attr.value.format) -1);

    return __func__;
}

KNX_DPST_ATRRI const char *DPST_9_1 (PointAttr_t *pAttr)  {
    pAttr->type = POINT_ATTR_TYPE_VALUE;
//    pAttr->attr.value.type = POINT_TYPE_FLOAT;
    pAttr->attr.value.step.f32 = 0.5f;
    pAttr->attr.value.scaling.f32 = 0.5f;
    pAttr->attr.value.min.f32 = -20.0f;
    pAttr->attr.value.max.f32 = 50.0f;
    strcpy (pAttr->attr.value.unit, "%");
//    strncpy (pAttr->attr.value.format, "F16", sizeof (pAttr->attr.value.format) -1);

    return __func__;
}





